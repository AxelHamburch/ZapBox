/**
 * @file NFCBoltCard.cpp
 * @brief NFC Bolt Card reader implementation for ZapBox (PN532 + NTAG424 DNA)
 *
 * See NFCBoltCard.h for full documentation.
 *
 * Also supports plain NDEF tags (e.g. NTAG215) carrying a bech32-encoded
 * LNURL (LNURLwithdraw). The LNURL is decoded to the target HTTPS URL and
 * processed via the same nfcLnurlwReceived() path as a Bolt Card.
 */

#ifdef ENABLE_NFC

#include "NFCBoltCard.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Log.h"

#include <Wire.h>
#include <Adafruit_PN532_NTAG424.h>
#include <vector>

// ─── Bech32 / LNURL helpers ──────────────────────────────────────────────────

// Forward declaration (defined in the module-private state section below)
static Adafruit_PN532 *s_nfc;

/**
 * @brief Decode a bech32-encoded LNURL to the underlying HTTPS URL.
 *
 * Accepts both bare "LNURL1..." and "lightning:LNURL1..." forms.
 * Returns an empty String on failure.
 */
static String decodeLnurlBech32(String lnurl)
{
    // Strip optional "lightning:" prefix
    lnurl.toLowerCase();
    if (lnurl.startsWith("lightning:")) lnurl = lnurl.substring(10);

    // Must start with "lnurl1"
    int sep = lnurl.indexOf('1');
    if (sep < 0) return "";
    String hrp  = lnurl.substring(0, sep);   // should be "lnurl"
    String data = lnurl.substring(sep + 1);  // base32 data + 6-char checksum

    if (hrp != "lnurl" || data.length() < 7) return "";

    // Drop 6 checksum characters
    data = data.substring(0, data.length() - 6);

    // Build reverse-lookup table for bech32 charset
    int8_t rev[128];
    memset(rev, -1, sizeof(rev));
    for (int i = 0; i < 32; i++) rev[(uint8_t)BECH32_CHARSET[i]] = i;

    // Decode base32 → 5-bit values
    std::vector<uint8_t> vals;
    vals.reserve(data.length());
    for (size_t i = 0; i < data.length(); i++) {
        char c = data[i];
        if (c < 0 || rev[(uint8_t)c] < 0) return "";
        vals.push_back((uint8_t)rev[(uint8_t)c]);
    }

    // Convert 5-bit groups to 8-bit bytes
    std::vector<uint8_t> result;
    int acc = 0, bits = 0;
    for (uint8_t v : vals) {
        acc = (acc << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            result.push_back((acc >> bits) & 0xFF);
        }
    }

    if (result.empty()) return "";
    return String((const char*)result.data()).substring(0, result.size());
}

// ─── NTAG215 / plain NDEF reader ─────────────────────────────────────────────

/**
 * @brief Read NDEF URI from a plain NFC tag (e.g. NTAG213/215/216) using
 *        page reads (4 bytes per page, starting at page 4).
 *
 * Parses the NDEF TLV structure and extracts the first URI record payload.
 * Returns empty String on any failure.
 */
static String readNdefUri()
{
    // Collect all readable pages (NTAG215 has pages 0-44, user data 4-39)
    uint8_t buf[256] = {0};
    int bufLen = 0;

    for (uint8_t page = 4; page < 44 && bufLen < (int)sizeof(buf) - 4; page++) {
        uint8_t pageData[4];
        if (!s_nfc->mifareultralight_ReadPage(page, pageData)) break;
        memcpy(buf + bufLen, pageData, 4);
        bufLen += 4;
    }

    if (bufLen == 0) return "";

    // Parse NDEF TLV: find tag 0x03 (NDEF Message)
    int pos = 0;
    while (pos < bufLen - 1) {
        uint8_t tag = buf[pos++];
        if (tag == 0xFE) break;          // terminator
        if (tag == 0x00) continue;       // NULL TLV

        // Read TLV length (can be 1 or 3 bytes)
        int tlvLen = 0;
        if (pos >= bufLen) break;
        if (buf[pos] == 0xFF) {          // 3-byte length
            if (pos + 2 >= bufLen) break;
            tlvLen = ((int)buf[pos+1] << 8) | buf[pos+2];
            pos += 3;
        } else {
            tlvLen = buf[pos++];
        }

        if (tag != 0x03) { pos += tlvLen; continue; }  // skip non-NDEF TLVs

        // We have an NDEF message at buf[pos], length tlvLen.
        // Minimal NDEF record header: [flags][type_len][payload_len][id_len?][type][payload]
        if (tlvLen < 5 || pos + tlvLen > bufLen) break;

        uint8_t flags      = buf[pos];
        uint8_t typeLen    = buf[pos+1];
        // payload length: 4 bytes for long records, 1 byte for short records (SR flag)
        int     payloadLen = 0;
        int     hdrOff     = 2;
        bool    sr         = (flags & 0x10) != 0;
        if (sr) {
            payloadLen = buf[pos + hdrOff++];
        } else {
            if (pos + hdrOff + 4 > bufLen) break;
            payloadLen = ((int)buf[pos+hdrOff]   << 24) | ((int)buf[pos+hdrOff+1] << 16) |
                         ((int)buf[pos+hdrOff+2] <<  8) |  (int)buf[pos+hdrOff+3];
            hdrOff += 4;
        }
        bool hasId = (flags & 0x08) != 0;
        if (hasId) hdrOff++;  // skip ID length byte (we don't need ID)

        int typeStart    = pos + hdrOff;
        int payloadStart = typeStart + typeLen + (hasId ? buf[pos+3] : 0);

        if (payloadStart + payloadLen > pos + tlvLen) break;

        // Check NDEF record type: 'U' (0x55) = URI record
        if (typeLen == 1 && buf[typeStart] == 0x55 && payloadLen > 1) {
            // First byte of payload is URI identifier code
            // 0x00 = no prefix, others map to common prefixes (https://, http://, etc.)
            static const char* URI_PREFIXES[] = {
                "", "http://www.", "https://www.", "http://", "https://",
                "tel:", "mailto:", "ftp://anonymous:anonymous@", "ftp://ftp.",
                "ftps://", "sftp://", "smb://", "nfs://", "ftp://",
                "dav://", "news:", "telnet://", "imap:", "rtsp://", "urn:",
                "pop:", "sip:", "sips:", "tftp:", "btspp://", "btl2cap://",
                "btgoep://", "tcpobex://", "irdaobex://", "file://",
                "urn:epc:id:", "urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:",
                "urn:epc:", "urn:nfc:"
            };
            uint8_t prefixCode = buf[payloadStart];
            String prefix = (prefixCode < 36) ? String(URI_PREFIXES[prefixCode]) : "";
            String uriPayload = prefix;
            for (int i = 1; i < payloadLen; i++) {
                uriPayload += (char)buf[payloadStart + i];
            }
            return uriPayload;
        }

        // Not a URI record – skip
        pos += tlvLen;
    }
    return "";
}

// ─── Module-private state ────────────────────────────────────────────────────

// Note: s_nfc is forward-declared before the helper functions above.
static TaskHandle_t    s_taskHandle = nullptr;

// ─── Card-removal helper ─────────────────────────────────────────────────────

/**
 * @brief Block until the card has been physically removed from the reader.
 *
 * A single missed read is not sufficient – the PN532 can lose track of a card
 * momentarily during RF field reset and still have it in range.  We therefore
 * require REQUIRED_ABSENT_POLLS consecutive "not found" results before declaring
 * the card gone.  With a 400 ms poll timeout this means the card must be absent
 * for at least 400 ms × 2 = ~0.8 s without interruption.
 */
static void waitForCardRemoval()
{
    constexpr int REQUIRED_ABSENT_POLLS = 2;

    LOG_INFO("NFC", "Waiting for card removal before accepting next tap...");

    int absentCount = 0;
    uint8_t chkUid[7] = {0};
    uint8_t chkLen    = 0;

    while (absentCount < REQUIRED_ABSENT_POLLS)
    {
        bool found = s_nfc->readPassiveTargetID(
            PN532_MIFARE_ISO14443A, chkUid, &chkLen, 400);

        if (!found)
        {
            absentCount++;
            LOG_INFO("NFC", String("Checking if NFC reader is free (") + absentCount +
                               "/" + REQUIRED_ABSENT_POLLS + ")...");
        }
        else
        {
            // Card still present – reset counter
            absentCount = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    LOG_INFO("NFC", "Card confirmed removed – ready for next tap");
}

// Shared device state (defined in main.cpp)
extern StateManager deviceState;

// ─── FreeRTOS task ───────────────────────────────────────────────────────────

/**
 * Main NFC task loop.
 *
 * Continuously waits for a card using readPassiveTargetID().
 * Because PIN_NFC_IRQ is passed to the Adafruit_PN532 constructor, the library
 * internally polls digitalRead(GPIO1) instead of querying the I2C bus.
 * This means the I2C bus is idle during the entire wait period and is only
 * briefly used (~5 ms) when a card is actually detected and its NDEF file read.
 */
static void nfc_task_code(void *pvParams)
{
    uint8_t uid[7]      = {0};
    uint8_t uidLength   = 0;
    uint8_t fileBuf[512];

    LOG_INFO("NFC", "Bolt Card task started – waiting for cards");

    while (true)
    {
        // Block until a card is detected (up to 30 s per attempt).
        bool found = s_nfc->readPassiveTargetID(
            PN532_MIFARE_ISO14443A, uid, &uidLength, 30000);

        if (!found)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Discard card tap when device is not ready for payments.
        // This prevents the NFC pending state from being entered during
        // initialisation, WiFi connect, any error condition, product
        // selection (user hasn't chosen a product yet), the BTC ticker,
        // or the screensaver (display off).
        //
        // btcTickerActive covers single mode where the device state stays
        // READY while the ticker is displayed – the user cannot see the
        // product/QR, so NFC must be blocked there as well.
        {
            DeviceState curState = deviceState.getState();
            bool tickerShowing = multiChannelConfig.btcTickerActive;
            if (curState == DeviceState::INITIALIZING    ||
                curState == DeviceState::CONNECTING_WIFI  ||
                curState == DeviceState::ERROR_CRITICAL   ||
                curState == DeviceState::ERROR_RECOVERABLE||
                curState == DeviceState::PRODUCT_SELECTION||
                curState == DeviceState::BTC_TICKER      ||
                curState == DeviceState::SCREENSAVER     ||
                tickerShowing)
            {
                LOG_WARN("NFC", String("Card tap ignored – device not ready (state: ") +
                                    deviceState.getDeviceStateName(curState) +
                                    String(", ticker: ") + String(tickerShowing) + String(")"));
                // For PRODUCT_SELECTION, BTC_TICKER and SCREENSAVER the device
                // can quickly transition to READY while the card is still on
                // the reader.  Without waiting for removal the card would be
                // detected again immediately in the next loop iteration.
                if (curState == DeviceState::PRODUCT_SELECTION ||
                    curState == DeviceState::BTC_TICKER       ||
                    curState == DeviceState::SCREENSAVER      ||
                    tickerShowing)
                {
                    waitForCardRemoval();
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                continue;
            }
        }

        LOG_INFO("NFC", String("Card detected – UID length: ") + String(uidLength));

        // Accept only 4- or 7-byte UIDs (ISO14443A standard lengths).
        if (uidLength != 4 && uidLength != 7)
        {
            LOG_WARN("NFC", "Incompatible card (unexpected UID length)");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Give the PN532 time to finish the ISO14443A activation sequence
        // before sending NTAG424 APDUs. Without this pause the PN532 is still
        // busy and I2C requestFrom() returns Error -1.
        // 150 ms is enough for even the slowest cards on the shared I2C bus.
        vTaskDelay(pdMS_TO_TICKS(150));

        // Verify the card is an NTAG424 DNA (required for Bolt Card).
        if (!s_nfc->ntag424_isNTAG424())
        {
            LOG_INFO("NFC", "Not an NTAG424 – trying plain NDEF read (NTAG215 / LNURL tag)...");

            String uri = readNdefUri();
            if (uri.length() == 0) {
                LOG_WARN("NFC", "No readable NDEF URI found on card");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            LOG_INFO("NFC", String("NDEF URI: ") + uri.substring(0, 60));

            // Strip optional "lightning:" URI scheme prefix
            String lnurlRaw = uri;
            if (lnurlRaw.startsWith("lightning:") || lnurlRaw.startsWith("LIGHTNING:"))
                lnurlRaw = lnurlRaw.substring(10);

            // Decode bech32 LNURL → https:// URL
            String url = decodeLnurlBech32(lnurlRaw);
            if (!url.startsWith("https://") && !url.startsWith("http://")) {
                LOG_WARN("NFC", String("NDEF URI is not a valid LNURL (decoded: ") + url.substring(0, 40) + ")");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            // Convert https:// to lnurlw:// so the server handler can process it
            // (views_api.py /nfc endpoint expects lnurlw:// and converts to https://)
            String lnurlw = "lnurlw://" + url.substring(url.indexOf("://") + 3);
            LOG_INFO("NFC", String("✓ LNURL decoded → lnurlw: ") + lnurlw.substring(0, 50) + "...");

            nfcLnurlwReceived(lnurlw);
            waitForCardRemoval();
            continue;
        }

        // Read the NDEF file from the NTAG424.
        // Retry up to 5 times with 100 ms between attempts – the shared I2C bus
        // (Touch + PN532) and PN532 internal timing can occasionally delay reads.
        uint8_t bytesRead = 0;
        for (int attempt = 1; attempt <= 5 && bytesRead == 0; attempt++)
        {
            bytesRead = s_nfc->ntag424_ISOReadFile(fileBuf, sizeof(fileBuf) - 1);
            if (bytesRead == 0 && attempt < 5)
            {
                LOG_WARN("NFC", String("NTAG424 read attempt ") + attempt + " returned 0 bytes – retrying...");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        if (bytesRead == 0)
        {
            LOG_WARN("NFC", "NTAG424 read returned 0 bytes after 5 attempts");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        fileBuf[bytesRead] = '\0'; // Null-terminate for safe String conversion.

        // Validate LNURLW prefix – Bolt Cards always start with "lnurlw://".
        if (strncmp("lnurlw://", (char *)fileBuf, 9) != 0)
        {
            LOG_WARN("NFC", String("Card does not contain LNURLW (got: ") +
                                String((char *)fileBuf).substring(0, 20) + "...)");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        String lnurlw = String((char *)fileBuf);
        LOG_INFO("NFC", String("✓ LNURLW read: ") + lnurlw.substring(0, 40) + "...");

        // Notify network layer \u2013 sets nfcPaymentPending, shows PENDING NFC screen,
        // and sends the HTTP POST. Flag stays true until QR screen is restored.
        nfcLnurlwReceived(lnurlw);
        waitForCardRemoval();
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool nfcBoltCardInit()
{
    LOG_INFO("NFC", "Initializing PN532 Bolt Card reader...");
    LOG_INFO("NFC", "  I2C: SDA=GPIO18, SCL=GPIO17  (shared with Touch controller)");
    LOG_INFO("NFC", String("  IRQ: GPIO") + String(PIN_NFC_IRQ) + " (active LOW, INPUT_PULLUP)");
    LOG_INFO("NFC", "  I2C address: 0x24");

    // Quick I2C probe: check if anything responds at PN532 address 0x24.
    // beginTransmission / endTransmission returns 0 only if a device ACKs.
    // This avoids 20+ seconds of getFirmwareVersion() retries when no PN532 is connected.
    // Only call Wire.begin() if the bus is not already running (touch.begin() initializes it).
    // Calling Wire.begin() on an already-running bus can disrupt ongoing I2C operations.
    #if !ENABLE_DISPLAY
    Wire.begin(18, 17); // headless: no touch controller initializes Wire, so do it here
    #endif
    Wire.beginTransmission(0x24);
    uint8_t probeResult = Wire.endTransmission();
    if (probeResult != 0) {
        LOG_INFO("NFC", String("No device at I2C address 0x24 (probe result: ") + String(probeResult) + ") – PN532 not connected");
        LOG_INFO("NFC", "  → Bolt Card feature disabled, normal operation unaffected");
        return false;
    }
    LOG_INFO("NFC", "I2C probe OK – device found at 0x24");

    // Wire was already initialized by TouchCST816S::begin() on GPIO17/18.
    // Create the PN532 instance using the shared Wire bus.
    // Passing PIN_NFC_IRQ makes the library use digitalRead() for card detection
    // instead of I2C polling – critical for not blocking the shared I2C bus.
    s_nfc = new Adafruit_PN532(PIN_NFC_IRQ, /* rst= */ -1, &Wire);

    if (!s_nfc->begin())
    {
        LOG_ERROR("NFC", "PN532 begin() failed – hardware not found or I2C error");
        LOG_ERROR("NFC", "  → Bolt Card feature disabled, normal operation unaffected");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Verify PN532 firmware version.
    uint32_t versiondata = s_nfc->getFirmwareVersion();
    if (!versiondata)
    {
        LOG_ERROR("NFC", "PN532 firmware version not readable");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    LOG_INFO("NFC", String("Found PN532 chip v") +
                        String((versiondata >> 24) & 0xFF) + ", firmware v" +
                        String((versiondata >> 16) & 0xFF) + "." +
                        String((versiondata >> 8) & 0xFF));

    // Configure PN532 Security Access Module for passive card reading.
    if (!s_nfc->SAMConfig())
    {
        LOG_ERROR("NFC", "SAMConfig failed");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Create NFC task BEFORE attaching the interrupt so s_taskHandle is valid
    // when the ISR first fires.
    BaseType_t res = xTaskCreatePinnedToCore(
        nfc_task_code,
        "NFCBoltCard",
        8192,          // Stack: NTAG424 operations need more than a basic PN532 task
        nullptr,
        1,             // Priority 1 – equal to Task1 (button/touch handler)
        &s_taskHandle,
        0              // Core 0 – same as Task1 and the main loop task
    );

    if (res != pdTRUE || s_taskHandle == nullptr)
    {
        LOG_ERROR("NFC", "Failed to create NFC FreeRTOS task");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Pull-up on IRQ pin (library sets INPUT_PULLUP in begin(), but be explicit).
    pinMode(PIN_NFC_IRQ, INPUT_PULLUP);

    LOG_INFO("NFC", "✓ Bolt Card reader ready");
    return true;
}

#endif // ENABLE_NFC
