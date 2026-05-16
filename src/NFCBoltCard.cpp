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
#include "I2CBus.h"
#include "Log.h"

#include <esp_log.h>

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
static volatile bool   s_boltcardRunning = false;

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

// ─── PN532 RF field control ──────────────────────────────────────────────────

/**
 * Turn the PN532 RF carrier ON (on=true) or OFF (on=false).
 *
 * Uses the PN532 RFConfiguration command (0x32, CfgItem=0x01).
 * After sendCommandCheckAck the PN532 puts a response in its I2C output buffer
 * and pulls IRQ LOW. We MUST drain that response before the next command or the
 * I2C bus gets stuck (i2cRead Error 263 cascade).
 *
 * RF OFF during the NT3H phone-read window eliminates the PN532 carrier that
 * otherwise keeps the NT3H powered and interferes with the phone's read even
 * when the software task is not actively polling.
 */
static void pn532SetRFField(bool on) {
    if (!i2cTake()) return; // skip if I2C bus is busy (rare – NT3H write in progress)
    uint8_t cmd[] = { 0x32, 0x01, on ? (uint8_t)0x01 : (uint8_t)0x00 };
    if (s_nfc->sendCommandCheckAck(cmd, 3, 500)) {
        // Wait for PN532 to assert IRQ LOW (response ready, typically <5 ms)
        uint32_t deadline = millis() + 200;
        while (digitalRead(PIN_NFC_IRQ) != LOW && millis() < deadline) delay(1);
        // Drain response regardless — leaving it fills the PN532 output buffer
        Wire.requestFrom((uint8_t)0x24, (uint8_t)10);
        while (Wire.available()) Wire.read();
    }
    i2cGive();
}

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

    while (s_boltcardRunning)
    {
        // Pause polling while a phone is reading NFC Tag 2 (NT3H2111).
        // FD pin or PN532 self-detection sets pn532PauseUntil; stopping readPassiveTargetID
        // eliminates the active REQA/ATQA frames that collide with the phone's NFC read.
        static bool pn532WasPaused = false;
        if (nfcConfig.pn532PauseUntil > 0 && millis() < nfcConfig.pn532PauseUntil) {
            if (!pn532WasPaused) {
                pn532SetRFField(false);
                LOG_INFO("NFC", "PN532 RF OFF – NT3H phone read window open");
                pn532WasPaused = true;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (pn532WasPaused) {
            pn532SetRFField(true);
            LOG_INFO("NFC", "PN532 RF ON – NT3H phone read window closed");
            pn532WasPaused = false;
        }

        // Block until a card is detected (1 s per attempt, short timeout
        // so the task can exit quickly when nfcBoltCardStop() is called).
        bool found = s_nfc->readPassiveTargetID(
            PN532_MIFARE_ISO14443A, uid, &uidLength, 1000);

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
        //
        // Headless (ENABLE_DISPLAY=0): PRODUCT_SELECTION is the operational
        // "ready" state for single-channel mode – there is no display to
        // show a product selection screen, so NFC must be accepted there.
        // GPIO 12 is always the NFC target in headless mode.
        {
            DeviceState curState = deviceState.getState();
            bool tickerShowing = multiChannelConfig.btcTickerActive;
#if ENABLE_DISPLAY
            bool blockForProductSelection = (curState == DeviceState::PRODUCT_SELECTION);
#else
            bool blockForProductSelection = false; // Headless: PRODUCT_SELECTION = ready
#endif
            if (curState == DeviceState::INITIALIZING    ||
                curState == DeviceState::CONNECTING_WIFI  ||
                curState == DeviceState::ERROR_CRITICAL   ||
                curState == DeviceState::ERROR_RECOVERABLE||
                blockForProductSelection                  ||
                curState == DeviceState::BTC_TICKER      ||
                curState == DeviceState::SCREENSAVER     ||
                tickerShowing                            ||
                lightBarrierConfig.blocked)
            {
                LOG_WARN("NFC", String("Card tap ignored – device not ready (state: ") +
                                    deviceState.getDeviceStateName(curState) +
                                    String(", ticker: ") + String(tickerShowing) +
                                    String(", blocked: ") + String(lightBarrierConfig.blocked) + String(")"));
                // For PRODUCT_SELECTION (display only), BTC_TICKER and SCREENSAVER
                // the device can quickly transition to READY while the card is
                // still on the reader. Without waiting for removal the card
                // would be detected again immediately in the next loop iteration.
                if (blockForProductSelection              ||
                    curState == DeviceState::BTC_TICKER  ||
                    curState == DeviceState::SCREENSAVER ||
                    tickerShowing                        ||
                    lightBarrierConfig.blocked)
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
        // 250 ms is sufficient for even the slowest cards on the shared I2C bus.
        vTaskDelay(pdMS_TO_TICKS(250));

        // Take I2C bus mutex for the full APDU exchange sequence.
        // Released before nfcLnurlwReceived() and waitForCardRemoval() to avoid
        // holding the bus during slow HTTP calls and card-removal polling.
        if (!i2cTake()) {
            LOG_WARN("NFC", "I2C mutex timeout before APDU exchange – skipping card");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Suppress Wire "i2cRead Error -1" log spam during the APDU exchange.
        // The Adafruit PN532 library polls the chip's ready-status via requestFrom()
        // every ~46 ms until it responds. During this poll the ESP32 Wire driver
        // logs Error -1 for each unsuccessful attempt. These are benign retries –
        // the chip responds successfully after ~3 polls (~138 ms). Suppressing here
        // prevents misleading error messages; restored immediately after the exchange.
        esp_log_level_set("i2c.master", ESP_LOG_NONE);

        // Verify the card is an NTAG424 DNA (required for Bolt Card).
        bool isNtag = s_nfc->ntag424_isNTAG424();
        if (!isNtag) {
            String uri = readNdefUri();
            i2cGive(); // release bus before HTTP call
            esp_log_level_set("i2c.master", ESP_LOG_WARN); // restore Wire logging
            if (uri.length() == 0) {
                // Non-NTAG424 card without NDEF URI is likely a phone — pause polling
                // so the phone can read the NT3H2111 tag without REQA collision.
                if (millis() >= nfcConfig.pn532PauseUntil) {
                    LOG_INFO("NFC", "Phone detected – opening 8 s NT3H read window");
                }
                nfcConfig.pn532PauseUntil = millis() + 8000;
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
        // Retry up to 5 times; release the I2C mutex between retries so other
        // devices (PCF8574) can use the bus during the 100 ms wait.
        uint8_t bytesRead = 0;
        for (int attempt = 1; attempt <= 5 && bytesRead == 0; attempt++)
        {
            bytesRead = s_nfc->ntag424_ISOReadFile(fileBuf, sizeof(fileBuf) - 1);
            if (bytesRead == 0 && attempt < 5)
            {
                LOG_WARN("NFC", String("NTAG424 read attempt ") + attempt + " returned 0 bytes – retrying...");
                i2cGive();
                vTaskDelay(pdMS_TO_TICKS(100));
                i2cTake();
            }
        }
        i2cGive(); // release bus – remaining work (HTTP, card-removal poll) must not hold the mutex
        esp_log_level_set("i2c.master", ESP_LOG_WARN); // restore Wire logging

        if (bytesRead == 0)
        {
            LOG_WARN("NFC", "NTAG424 read returned 0 bytes after 5 attempts");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        fileBuf[bytesRead] = '\0'; // Null-terminate for safe String conversion.

        // Validate LNURLW prefix – Bolt Cards always start with "lnurlw://".
        // Case-insensitive: some cards return "LNURLW://" in uppercase.
        if (strncasecmp("lnurlw://", (char *)fileBuf, 9) != 0)
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

    LOG_INFO("NFC", "Bolt Card task ending");
    s_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool nfcBoltCardInit()
{
    LOG_INFO("NFC", "Initializing PN532 Bolt Card reader...");
    LOG_INFO("NFC", String("  I2C: SDA=GPIO") + String(PIN_IIC_SDA) + ", SCL=GPIO" + String(PIN_IIC_SCL));
    LOG_INFO("NFC", String("  IRQ: GPIO") + String(PIN_NFC_IRQ) + " (active LOW, INPUT_PULLUP)");
    LOG_INFO("NFC", "  I2C address: 0x24");

    // Quick I2C probe: check if anything responds at PN532 address 0x24.
    // beginTransmission / endTransmission returns 0 only if a device ACKs.
    // This avoids 20+ seconds of getFirmwareVersion() retries when no PN532 is connected.
    // Only call Wire.begin() if the bus is not already running (touch.begin() initializes it).
    // Calling Wire.begin() on an already-running bus can disrupt ongoing I2C operations.
    #if !ENABLE_DISPLAY
    Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL); // headless: use board-specific I²C pins (PinConfig.h)
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
    s_boltcardRunning = true;
    nfcConfig.boltcardActive = true;
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
        s_boltcardRunning = false;
        nfcConfig.boltcardActive = false;
        return false;
    }

    // Pull-up on IRQ pin (library sets INPUT_PULLUP in begin(), but be explicit).
    pinMode(PIN_NFC_IRQ, INPUT_PULLUP);

    LOG_INFO("NFC", "✓ Bolt Card reader ready");
    return true;
}

void nfcBoltCardStop()
{
    LOG_INFO("NFC", "Stopping Bolt Card reader...");
    s_boltcardRunning = false;
    nfcConfig.boltcardActive = false;

    // Wait for task to finish (with timeout — readPassiveTargetID can block up to 30s)
    for (int i = 0; i < 50 && s_taskHandle != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_taskHandle != nullptr) {
        LOG_WARN("NFC", "Bolt Card task did not exit cleanly — deleting");
        vTaskDelete(s_taskHandle);
        s_taskHandle = nullptr;
    }

    if (s_nfc) {
        delete s_nfc;
        s_nfc = nullptr;
    }

    LOG_INFO("NFC", "Bolt Card reader stopped");
}

#endif // ENABLE_NFC
