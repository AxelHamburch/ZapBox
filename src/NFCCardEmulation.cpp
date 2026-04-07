/**
 * @file NFCCardEmulation.cpp
 * @brief NFC Card Emulation implementation for ZapBox (PN532 Target Mode)
 *
 * Emulates an NFC Forum Type 4 Tag so that smartphones can read the
 * current LNURLp payment link by tapping the PN532 module.
 *
 * Protocol: ISO 14443-4 APDU exchange via PN532 Target Mode.
 * The phone sends standard NFC Forum Type 4 Tag commands (SELECT, READ BINARY)
 * and we respond with the appropriate NDEF data.
 *
 * See NFCCardEmulation.h for full documentation.
 */

#ifdef ENABLE_NFC

#include "NFCCardEmulation.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Log.h"

#include <Wire.h>
#include <Adafruit_PN532_NTAG424.h>

// ─── Module-private state ────────────────────────────────────────────────────

static Adafruit_PN532 *s_emulNfc = nullptr;
static TaskHandle_t    s_emulTaskHandle = nullptr;
static volatile bool   s_emulRunning = false;

// Shared device state (defined in main.cpp)
extern StateManager deviceState;

// ─── NDEF helpers ────────────────────────────────────────────────────────────

// NFC Forum Type 4 Tag Application (NDEF tag application)
static const uint8_t NDEF_APP_SELECT[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};

// Capability Container (CC) file — describes NDEF file properties
// Mapping version 2.0, max read size 0xFF, NDEF file ID 0xE104, max NDEF size 0x0400
static const uint8_t CC_FILE[] = {
    0x00, 0x0F,  // CC length: 15 bytes
    0x20,        // Mapping version 2.0
    0x00, 0xFF,  // Max R-APDU data size (255 bytes)
    0x00, 0xFF,  // Max C-APDU data size (255 bytes)
    0x04,        // NDEF File Control TLV: Type = NDEF
    0x06,        // TLV length = 6
    0xE1, 0x04,  // NDEF file ID
    0x04, 0x00,  // Max NDEF file size (1024 bytes)
    0x00,        // Read access: allowed without security
    0xFF         // Write access: denied
};

// APDU success response
static const uint8_t APDU_OK[] = {0x90, 0x00};

/**
 * @brief Build NDEF URI record from a lightning URL string.
 *
 * Creates a complete NDEF message containing a single URI record.
 * The URI is prefixed with the "lightning:" scheme.
 *
 * @param lnurl The LNURL string (with or without "lightning:" prefix)
 * @param buf Output buffer (must be large enough)
 * @param bufLen Output: total NDEF message length (including 2-byte length prefix)
 * @return true on success, false if buffer too small
 */
static bool buildNdefUri(const String &lnurl, uint8_t *buf, uint16_t *bufLen, uint16_t maxBuf)
{
    // Strip existing "lightning:" prefix if present — we'll add URI prefix code 0x00
    String uri = lnurl;
    if (uri.startsWith("lightning:") || uri.startsWith("LIGHTNING:")) {
        // Keep as-is — use prefix code 0x00 (no abbreviation)
    }

    uint16_t uriLen = uri.length();

    // NDEF record:
    // [flags] [type_len] [payload_len] [type:"U"] [prefix_code] [uri_bytes]
    // flags: MB=1 ME=1 CF=0 SR=1 IL=0 TNF=0x01 = 0xD1
    uint16_t payloadLen = 1 + uriLen;  // prefix_code + uri string
    if (payloadLen > 255) {
        // Need long record format (SR=0)
        // flags: MB=1 ME=1 CF=0 SR=0 IL=0 TNF=0x01 = 0xC1
        uint16_t recordLen = 1 + 1 + 4 + 1 + payloadLen;  // flags + type_len + payload_len(4) + type + payload
        uint16_t totalLen = 2 + recordLen;  // 2-byte NDEF length prefix
        if (totalLen > maxBuf) return false;

        uint16_t pos = 0;
        // NDEF message length (2 bytes, big-endian)
        buf[pos++] = (recordLen >> 8) & 0xFF;
        buf[pos++] = recordLen & 0xFF;
        // NDEF record header
        buf[pos++] = 0xC1;        // MB|ME|TNF=0x01 (long record)
        buf[pos++] = 0x01;        // Type length = 1
        buf[pos++] = (payloadLen >> 24) & 0xFF;
        buf[pos++] = (payloadLen >> 16) & 0xFF;
        buf[pos++] = (payloadLen >> 8) & 0xFF;
        buf[pos++] = payloadLen & 0xFF;
        buf[pos++] = 0x55;        // Type = "U" (URI record)
        buf[pos++] = 0x00;        // URI prefix = none (full URI in payload)
        memcpy(buf + pos, uri.c_str(), uriLen);
        pos += uriLen;
        *bufLen = pos;
    } else {
        // Short record format (SR=1)
        uint16_t recordLen = 1 + 1 + 1 + 1 + payloadLen;  // flags + type_len + payload_len(1) + type + payload
        uint16_t totalLen = 2 + recordLen;  // 2-byte NDEF length prefix
        if (totalLen > maxBuf) return false;

        uint16_t pos = 0;
        buf[pos++] = (recordLen >> 8) & 0xFF;
        buf[pos++] = recordLen & 0xFF;
        buf[pos++] = 0xD1;        // MB|ME|SR|TNF=0x01
        buf[pos++] = 0x01;        // Type length = 1
        buf[pos++] = (uint8_t)payloadLen;
        buf[pos++] = 0x55;        // Type = "U"
        buf[pos++] = 0x00;        // URI prefix = none
        memcpy(buf + pos, uri.c_str(), uriLen);
        pos += uriLen;
        *bufLen = pos;
    }

    return true;
}

// ─── APDU response helpers ───────────────────────────────────────────────────

// Forward declarations — defined below with raw I2C helpers
static bool setDataTargetIRQ(const uint8_t *cmd, uint8_t cmdlen);
static void pn532RecoverBus();
static bool pn532TransactIRQ(const uint8_t *cmd, uint8_t cmdlen,
                              uint8_t *frame, uint8_t *frameLen,
                              uint8_t readLen,
                              uint16_t timeout_ms,
                              uint8_t pollIntervalMs);

/**
 * @brief Send a response APDU back to the phone via PN532 TgSetData.
 *
 * Prepends the TgSetData command byte (0x8E) to the response.
 * Uses IRQ-based raw I2C to avoid bus contention with Touch controller.
 */
static bool sendResponse(const uint8_t *data, uint16_t dataLen, uint8_t sw1, uint8_t sw2)
{
    uint8_t resp[256];
    uint16_t pos = 0;

    resp[pos++] = 0x8E;  // TgSetData command
    if (data && dataLen > 0) {
        if (dataLen + 3 > sizeof(resp)) return false;
        memcpy(resp + pos, data, dataLen);
        pos += dataLen;
    }
    resp[pos++] = sw1;
    resp[pos++] = sw2;

    return setDataTargetIRQ(resp, pos);
}

/**
 * @brief Send SW 90 00 (success with no data).
 */
static bool sendOK()
{
    return sendResponse(nullptr, 0, 0x90, 0x00);
}

/**
 * @brief Send data followed by SW 90 00.
 */
static bool sendDataOK(const uint8_t *data, uint16_t len)
{
    return sendResponse(data, len, 0x90, 0x00);
}

/**
 * @brief Send SW 6A 82 (file not found).
 */
static bool sendFileNotFound()
{
    return sendResponse(nullptr, 0, 0x6A, 0x82);
}

// ─── FreeRTOS task ───────────────────────────────────────────────────────────

/**
 * Wait for PN532 RDY byte via I2C (single poll, no retry loop).
 *
 * @return true if PN532 is ready (RDY byte == 0x01), false otherwise
 */
static bool pn532IsReadyI2C()
{
    uint8_t rdy = 0;
    Wire.requestFrom((uint8_t)PN532_I2C_ADDRESS, (uint8_t)1);
    if (Wire.available()) {
        rdy = Wire.read();
    }
    return (rdy == PN532_I2C_READY);
}

/**
 * Write a raw PN532 command frame via I2C.
 *
 * Frame format: [PREAMBLE 00] [STARTCODE 00 FF] [LEN] [LCS] [TFI D4] [CMD...] [DCS] [POSTAMBLE 00]
 *
 * @param cmd     Command bytes (starting with command code, e.g. 0x8C)
 * @param cmdlen  Number of command bytes
 */
static void pn532WriteCommand(const uint8_t *cmd, uint8_t cmdlen)
{
    uint8_t LEN = cmdlen + 1;  // +1 for TFI byte (D4)
    uint8_t packet[8 + cmdlen];

    packet[0] = PN532_PREAMBLE;
    packet[1] = PN532_STARTCODE1;
    packet[2] = PN532_STARTCODE2;
    packet[3] = LEN;
    packet[4] = ~LEN + 1;       // Length checksum
    packet[5] = PN532_HOSTTOPN532;  // TFI = D4

    uint8_t sum = PN532_HOSTTOPN532;
    for (uint8_t i = 0; i < cmdlen; i++) {
        packet[6 + i] = cmd[i];
        sum += cmd[i];
    }
    packet[6 + cmdlen] = ~sum + 1;  // Data checksum
    packet[7 + cmdlen] = PN532_POSTAMBLE;

    Wire.beginTransmission(PN532_I2C_ADDRESS);
    Wire.write(packet, 8 + cmdlen);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_INFO("NFC", String("I2C write err=") + String(err) + ", recovering");
        pn532RecoverBus();
        // Retry once after bus recovery
        Wire.beginTransmission(PN532_I2C_ADDRESS);
        Wire.write(packet, 8 + cmdlen);
        Wire.endTransmission();
    }
}

/**
 * Read ACK frame from PN532 via I2C.
 *
 * ACK frame: [RDY] [00 00 FF 00 FF 00]
 *
 * @return true if valid ACK received
 */
static bool pn532ReadAck()
{
    uint8_t ack[7];  // 1 RDY byte + 6 ACK bytes
    Wire.requestFrom((uint8_t)PN532_I2C_ADDRESS, (uint8_t)7);
    for (int i = 0; i < 7 && Wire.available(); i++) {
        ack[i] = Wire.read();
    }
    // ACK pattern after RDY byte: 00 00 FF 00 FF 00
    static const uint8_t expectedAck[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    return (memcmp(ack + 1, expectedAck, 6) == 0);
}

/**
 * Send SAMConfig via raw I2C (no library dependency).
 *
 * Command: D4 14 01 (SAMConfig: Normal mode, no timeout).
 * This resets the PN532 from Target Mode back to normal mode.
 * Uses pn532TransactIRQ for safe two-phase reads.
 */
static void pn532SAMConfigRaw()
{
    uint8_t sam[] = {0x14, 0x01};
    uint8_t frame[32];
    uint8_t frameLen = 0;
    // SAMConfig response is fast (~5ms), use short timeout
    pn532TransactIRQ(sam, sizeof(sam), frame, &frameLen, 12, 200, 5);
}

/**
 * Reset the I2C peripheral to recover from a stuck bus.
 *
 * Called after I2C read errors (Error -1 or Error 263) which leave
 * the bus in a hung state (SDA held low by PN532).
 */
static void pn532RecoverBus()
{
    LOG_INFO("NFC", "Recovering I2C bus...");
    Wire.end();
    delay(50);
    Wire.begin(18, 17);
    delay(10);
}

/**
 * Generic: send PN532 command, read ACK, wait for response via IRQ, read response.
 *
 * This is the core I2C-safe communication primitive. All PN532 commands during
 * runtime go through this function to avoid I2C polling that conflicts with
 * the Touch controller on the shared bus.
 *
 * I2C usage: ~4ms per call (write + ACK + read). Bus is idle during IRQ wait.
 *
 * @param cmd            Command bytes (e.g., [0x8C, ...] for TgInitAsTarget)
 * @param cmdlen         Number of command bytes
 * @param frame          Output: response frame starting at PREAMBLE (RDY byte stripped)
 * @param frameLen       Output: number of bytes written to frame
 * @param readLen        (unused — kept for API compat, length is auto-detected from frame header)
 * @param timeout_ms     Maximum IRQ wait (0 = no timeout)
 * @param pollIntervalMs Delay between IRQ checks (50 for slow waits, 5 for APDU)
 * @return true if valid response frame received
 */
static bool pn532TransactIRQ(const uint8_t *cmd, uint8_t cmdlen,
                              uint8_t *frame, uint8_t *frameLen,
                              uint8_t readLen,
                              uint16_t timeout_ms,
                              uint8_t pollIntervalMs = 5)
{
    *frameLen = 0;

    // Step 1: Send command (~1ms I2C write)
    pn532WriteCommand(cmd, cmdlen);

    // Step 2: Wait for ACK ready (brief I2C polling, typically 1-2ms)
    bool ackReady = false;
    for (int i = 0; i < 10; i++) {
        delay(1);
        if (pn532IsReadyI2C()) { ackReady = true; break; }
    }
    if (!ackReady) {
        LOG_INFO("NFC", "pn532TransactIRQ: ACK not ready");
        return false;
    }

    // Step 3: Read ACK (~1ms I2C read)
    if (!pn532ReadAck()) {
        LOG_INFO("NFC", "pn532TransactIRQ: invalid ACK");
        return false;
    }

    // Step 4: Wait for response via IRQ pin — NO I2C POLLING.
    // For TgGetData/TgSetData with pre-buffered data, IRQ is already LOW.
    // For TgInitAsTarget, this waits for a phone (seconds).
    // NO delay after ACK — the delay(3)/RDY-poll version broke APDU exchange.
    // The working version (successful payment) had zero delay here.
    unsigned long start = millis();
    while (digitalRead(PIN_NFC_IRQ) != LOW) {
        if (!s_emulRunning) return false;
        if (timeout_ms > 0 && (millis() - start > timeout_ms)) return false;
        vTaskDelay(pdMS_TO_TICKS(pollIntervalMs));
    }

    // Step 5: IRQ LOW — brief settle then read.
    // NO RDY polling loop — each Wire.requestFrom may disturb the PN532
    // clone's I2C state. The working version just did delay(2) then read.
    delay(2);

    uint8_t actualRead = (readLen > 0 && readLen <= 64) ? readLen : 32;
    uint8_t raw[64];
    uint8_t readCount = 0;
    Wire.requestFrom((uint8_t)PN532_I2C_ADDRESS, actualRead);
    while (Wire.available() && readCount < actualRead) {
        raw[readCount++] = Wire.read();
    }

    // I2C retry: if first read failed (Error -1 / NACK), try once more.
    // The PN532 data is still in its buffer — a brief pause often clears
    // the bus glitch. This saves sessions that would otherwise abort.
    if (readCount < 8 || raw[0] != PN532_I2C_READY) {
        delay(2);
        readCount = 0;
        Wire.requestFrom((uint8_t)PN532_I2C_ADDRESS, actualRead);
        while (Wire.available() && readCount < actualRead) {
            raw[readCount++] = Wire.read();
        }
    }

    // Validate: RDY byte must be 0x01
    if (readCount < 8 || raw[0] != PN532_I2C_READY) {
        LOG_INFO("NFC", String("pn532TransactIRQ: bad, cnt=") + String(readCount) +
                        " rdy=0x" + String(raw[0], HEX));
        pn532RecoverBus();
        return false;
    }

    // Validate preamble + start code
    if (raw[1] != 0x00 || raw[2] != 0x00 || raw[3] != 0xFF) {
        LOG_INFO("NFC", String("pn532TransactIRQ: bad preamble ") +
                        String(raw[1], HEX) + " " + String(raw[2], HEX) + " " + String(raw[3], HEX));
        pn532RecoverBus();
        return false;
    }

    // Strip RDY byte → frame starts at raw[1]
    *frameLen = readCount - 1;
    memcpy(frame, raw + 1, *frameLen);
    return true;
}

/**
 * IRQ-based TgGetData — receive APDU command from phone.
 *
 * Replaces the library's getDataTarget() which internally polls I2C
 * via waitready(1000ms) = 100 I2C transactions before timeout.
 *
 * @param cmd     Output: APDU command bytes from phone
 * @param cmdLen  Output: number of APDU bytes
 * @return true if APDU received, false if phone disconnected or timeout
 */
static bool getDataTargetIRQ(uint8_t *cmd, uint8_t *cmdLen, uint16_t timeout_ms = 1000)
{
    uint8_t tgGetData[] = {0x86};  // TgGetData command
    uint8_t frame[70];
    uint8_t frameLen = 0;

    if (!pn532TransactIRQ(tgGetData, 1, frame, &frameLen, 32, timeout_ms)) {
        return false;
    }

    // Validate: frame[5]=D5 (TFI PN532→host), frame[6]=0x87 (TgGetData response)
    if (frameLen < 9 || frame[5] != 0xD5 || frame[6] != 0x87) {
        LOG_INFO("NFC", String("getDataTargetIRQ: invalid response, len=") + String(frameLen) +
                        " [5]=0x" + String(frameLen > 5 ? frame[5] : 0, HEX) +
                        " [6]=0x" + String(frameLen > 6 ? frame[6] : 0, HEX));
        return false;
    }

    // Check error status byte
    if (frame[7] != 0x00) {
        LOG_INFO("NFC", String("getDataTargetIRQ: error status 0x") + String(frame[7], HEX));
        return false;
    }

    // Data length = LEN - 3 (LEN includes TFI + response code + status)
    uint8_t dataLen = frame[3] - 3;
    if (dataLen > 60) dataLen = 60;  // Safety cap (cmdBuf is 64 bytes)
    if (dataLen > 0 && (uint8_t)(8 + dataLen) <= frameLen) {
        memcpy(cmd, frame + 8, dataLen);
    }
    *cmdLen = dataLen;
    return true;
}

/**
 * IRQ-based TgSetData — send APDU response to phone.
 *
 * Replaces the library's setDataTarget() which internally polls I2C
 * via waitready(100ms).
 *
 * @param cmd     Command buffer (first byte MUST be 0x8E = TgSetData)
 * @param cmdlen  Total length including 0x8E prefix
 * @return true if data sent to phone successfully
 */
static bool setDataTargetIRQ(const uint8_t *cmd, uint8_t cmdlen)
{
    uint8_t frame[70];
    uint8_t frameLen = 0;

    if (!pn532TransactIRQ(cmd, cmdlen, frame, &frameLen, 12, 500)) {
        return false;
    }

    // Validate: TFI=D5, response code=0x8F (TgSetData response)
    if (frameLen < 7 || frame[5] != 0xD5) return false;
    return true;
}

/**
 * Enter PN532 Target Mode and wait for a phone using IRQ.
 *
 * Uses pn532TransactIRQ with 50ms poll interval (slow — phone may take minutes).
 * On timeout, resets PN532 via SAMConfig for clean retry.
 *
 * @param timeout_ms  Maximum wait for a phone (0 = infinite)
 * @return true if a phone connected and session is active
 */
static bool asTargetIRQ(uint16_t timeout_ms = 0)
{
    // TgInitAsTarget command payload — ORIGINAL parameters that produced
    // successful ISO-DEP APDU exchanges on this PN532 clone (v50, fw 1.6).
    //
    // IMPORTANT: This PN532 clone reports mode=0x08 in the response even
    // when the phone actually activated ISO-DEP (RATS/ATS). The mode byte
    // is UNRELIABLE on clones — do NOT reject based on it. Just proceed
    // with APDU exchange; if it's truly NFC-DEP, TgGetData will fail
    // immediately and we'll cleanly exit the session.
    static const uint8_t target[] = {
        0x8C,             // INIT AS TARGET
        0x00,             // MODE: passive, all speeds, PICC + DEP
        0x08, 0x00,       // SENS_RES (ATQA)
        0xdc, 0x44, 0x20, // NFCID1T (3-byte UID)
        0x20,             // SEL_RES (SAK: 0x20 = ISO-DEP only in NFC Forum spec)
        0x01, 0xfe,       // NFCID2T (Felica, starts with 01fe)
        0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, // PAD
        0xff, 0xff,                                         // SYSTEM CODE
        0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44,
        0x33, 0x22, 0x11, 0x01, 0x00,                       // NFCID3t
        0x0d, 0x52, 0x46, 0x49, 0x44, 0x49, 0x4f,
        0x74, 0x20, 0x50, 0x4e, 0x35, 0x33, 0x32            // HISTORICAL BYTES
    };

    uint8_t frame[70];
    uint8_t frameLen = 0;

    if (!pn532TransactIRQ(target, sizeof(target), frame, &frameLen, 32, timeout_ms, 50)) {
        return false;
    }

    // Validate TgInitAsTarget response: D5 8D [MODE]
    // Log mode for diagnostics only — do NOT reject based on mode byte.
    // PN532 clones report mode=0x08 even for ISO-DEP connections.
    for (uint8_t i = 0; i + 2 < frameLen; i++) {
        if (frame[i] == 0xD5 && frame[i + 1] == 0x8D) {
            uint8_t mode = frame[i + 2];
            LOG_INFO("NFC", String("TgInitAsTarget: mode=0x") + String(mode, HEX));
            return true;
        }
    }

    LOG_DEBUG("NFC", "asTargetIRQ: invalid response");
    return false;
}

/**
 * NFC Card Emulation task loop.
 *
 * Emulates an NFC Forum Type 4 Tag by responding to standard APDU commands:
 * 1. SELECT NDEF Application (AID D2760000850101)
 * 2. SELECT CC File (ID E103)
 * 3. READ BINARY CC
 * 4. SELECT NDEF File (ID E104)
 * 5. READ BINARY NDEF (returns current LNURLp as URI record)
 *
 * The LNURLp content is refreshed from lightningConfig.lightning on each
 * new phone connection, so product changes are reflected immediately.
 */
static void emulation_task_code(void *pvParams)
{
    uint8_t cmdBuf[64];
    uint8_t cmdLen;

    // Pre-built NDEF buffer (updated when lightning URL changes)
    uint8_t ndefBuf[512];
    uint16_t ndefLen = 0;
    String lastLnurl = "";

    // Which "file" is currently selected: 0=none, 1=CC, 2=NDEF
    enum SelectedFile { SEL_NONE, SEL_CC, SEL_NDEF };

    LOG_INFO("NFC", "Card emulation task started");

    uint8_t consecutiveFastFails = 0;  // Track repeated fast-fail retries

    while (s_emulRunning)
    {
        // Only emulate when device is in a payment-ready state
        {
            DeviceState curState = deviceState.getState();
            bool tickerShowing = multiChannelConfig.btcTickerActive;
            if (curState != DeviceState::READY || tickerShowing)
            {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
        }

        // Rebuild NDEF if lightning URL changed
        String currentLnurl = String(lightningConfig.lightning);
        if (currentLnurl != lastLnurl && currentLnurl.length() > 0) {
            if (buildNdefUri(currentLnurl, ndefBuf, &ndefLen, sizeof(ndefBuf))) {
                lastLnurl = currentLnurl;
                LOG_INFO("NFC", String("NDEF updated (") + String(ndefLen) + " bytes) for: " + currentLnurl.substring(0, 40) + "...");
            } else {
                LOG_WARN("NFC", "Failed to build NDEF URI (too long?)");
            }
        }

        // Enter Target Mode — wait for phone using IRQ (no I2C polling)
        LOG_DEBUG("NFC", "Entering Target Mode (waiting for phone)...");
        if (!asTargetIRQ(2000)) {
            // No phone within 2 seconds — retry (I2C bus was idle the whole time)
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        LOG_INFO("NFC", "Phone connected — processing APDUs");
        // Minimal delay for RATS/ATS — PN532 handles this internally.
        // The successful payment session used 30ms. Shorter is better:
        // the phone may time out if we wait too long before TgGetData.
        vTaskDelay(pdMS_TO_TICKS(20));
        SelectedFile selectedFile = SEL_NONE;
        bool sessionActive = true;
        uint8_t apduCount = 0;  // Track APDUs exchanged in this session

        // First APDU: use short timeout (300ms).
        // If the phone connected via NFC-DEP (P2P) instead of ISO-DEP,
        // no APDU will arrive. Fail fast so we can re-enter TgInitAsTarget
        // and catch the next attempt (which may be ISO-DEP).
        // This PN532 clone reports mode=0x08 for ALL connections, so we
        // can't distinguish ISO-DEP from NFC-DEP via mode byte.
        cmdLen = 0;
        if (!getDataTargetIRQ(cmdBuf, &cmdLen, 300)) {
            consecutiveFastFails++;
            if (consecutiveFastFails >= 3) {
                // Phone is on the field but not sending APDUs.
                // Wait 3s so phone detects "tag gone" and resets NFC stack.
                LOG_INFO("NFC", String("Fast-fail x") + String(consecutiveFastFails) +
                                " — cooldown 3s for phone NFC reset");
                pn532RecoverBus();
                vTaskDelay(pdMS_TO_TICKS(3000));
                consecutiveFastFails = 0;
            } else {
                LOG_INFO("NFC", "First APDU not received (300ms) — retrying Target Mode");
            }
            continue;
        }
        consecutiveFastFails = 0;  // Successful APDU resets counter
        goto first_apdu_received;

        // APDU exchange loop — process commands from the phone
        while (sessionActive && s_emulRunning)
        {
            cmdLen = 0;
            if (!getDataTargetIRQ(cmdBuf, &cmdLen)) {
                LOG_INFO("NFC", "getDataTarget failed — phone disconnected");
                sessionActive = false;
                break;
            }

        first_apdu_received:
            apduCount++;
            // Log received APDU (hex dump, first 16 bytes max)
            {
                String hex = "";
                for (uint8_t i = 0; i < cmdLen && i < 16; i++) {
                    if (cmdBuf[i] < 0x10) hex += "0";
                    hex += String(cmdBuf[i], HEX);
                    hex += " ";
                }
                LOG_INFO("NFC", String("APDU rx (") + String(cmdLen) + "): " + hex);
            }

            if (cmdLen < 4) {
                // Malformed APDU — ignore
                sendOK();
                continue;
            }

            uint8_t cla = cmdBuf[0];
            uint8_t ins = cmdBuf[1];
            uint8_t p1  = cmdBuf[2];
            uint8_t p2  = cmdBuf[3];
            uint8_t lc  = (cmdLen > 4) ? cmdBuf[4] : 0;

            // SELECT command (INS=0xA4)
            if (ins == 0xA4) {
                if (p1 == 0x04 && lc == 7) {
                    // SELECT by AID — check for NDEF application
                    if (memcmp(cmdBuf + 5, NDEF_APP_SELECT, 7) == 0) {
                        LOG_INFO("NFC", "APDU: SELECT NDEF App -> 9000");
                        selectedFile = SEL_NONE;
                        sendOK();
                        continue;
                    }
                }

                if (p1 == 0x00 && lc == 2) {
                    // SELECT by File ID
                    uint16_t fileId = ((uint16_t)cmdBuf[5] << 8) | cmdBuf[6];
                    if (fileId == 0xE103) {
                        LOG_INFO("NFC", "APDU: SELECT CC -> 9000");
                        selectedFile = SEL_CC;
                        sendOK();
                    } else if (fileId == 0xE104) {
                        LOG_INFO("NFC", "APDU: SELECT NDEF -> 9000");
                        selectedFile = SEL_NDEF;
                        sendOK();
                    } else {
                        LOG_INFO("NFC", String("APDU: SELECT unknown 0x") + String(fileId, HEX));
                        sendFileNotFound();
                    }
                    continue;
                }

                // Unknown SELECT variant
                sendOK();
                continue;
            }

            // READ BINARY command (INS=0xB0)
            if (ins == 0xB0) {
                uint16_t offset = ((uint16_t)p1 << 8) | p2;
                uint8_t le = (cmdLen > 4) ? cmdBuf[4] : 0;
                if (le == 0) le = 255;  // Le=0 means 256 bytes, limit to 255

                if (selectedFile == SEL_CC) {
                    LOG_INFO("NFC", String("APDU: READ CC off=") + String(offset) + " len=" + String(le));
                    uint16_t avail = sizeof(CC_FILE) - offset;
                    if (offset >= sizeof(CC_FILE)) {
                        sendOK();  // Past end of file
                    } else {
                        uint8_t readLen = (le < avail) ? le : avail;
                        sendDataOK(CC_FILE + offset, readLen);
                    }
                } else if (selectedFile == SEL_NDEF) {
                    LOG_INFO("NFC", String("APDU: READ NDEF off=") + String(offset) + " len=" + String(le));
                    if (ndefLen == 0) {
                        // No NDEF data available yet
                        sendOK();
                    } else {
                        uint16_t avail = ndefLen - offset;
                        if (offset >= ndefLen) {
                            sendOK();
                        } else {
                            uint8_t readLen = (le < avail) ? le : avail;
                            sendDataOK(ndefBuf + offset, readLen);
                        }
                    }
                } else {
                    sendFileNotFound();
                }
                continue;
            }

            // UPDATE BINARY (INS=0xD6) — deny writes
            if (ins == 0xD6) {
                sendResponse(nullptr, 0, 0x69, 0x82);  // Security status not satisfied
                continue;
            }

            // Unknown command — return OK to keep session alive
            sendOK();
        }

        // Reset PN532 and recover I2C bus before re-entering target mode
        pn532RecoverBus();
        pn532SAMConfigRaw();
        // Expected APDUs for a complete NDEF read:
        //   4 setup (SELECT App, SELECT CC, READ CC, SELECT NDEF)
        // + 1 READ NDEF length (2 bytes)
        // + ceil(ndefLen / 59) READ NDEF data chunks
        // Phone max read per APDU is 59 bytes (Le=0x3B).
        uint8_t expectedApdus = 4 + 1 + ((ndefLen + 58) / 59);
        if (apduCount > 0 && apduCount < expectedApdus) {
            // Phone started ISO-DEP NDEF reading but disconnected mid-session.
            // Stay out of Target Mode for 2s so phone's NFC stack detects
            // "tag gone" and resets — prevents pointless reconnect loops.
            LOG_INFO("NFC", String("Session incomplete: ") + String(apduCount) +
                            "/" + String(expectedApdus) +
                            " APDUs — cooldown 2s for phone NFC reset");
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            LOG_INFO("NFC", "Phone disconnected — returning to Target Mode");
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    LOG_INFO("NFC", "Card emulation task ending");
    s_emulTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool nfcCardEmulationInit()
{
    LOG_INFO("NFC", "Initializing NFC Card Emulation (Target Mode)...");
    LOG_INFO("NFC", "  I2C: SDA=GPIO18, SCL=GPIO17  (shared with Touch controller)");
    LOG_INFO("NFC", String("  IRQ: GPIO") + String(PIN_NFC_IRQ) + " (active LOW, INPUT_PULLUP)");

    // Headless: Wire may not be initialized yet
    #if !ENABLE_DISPLAY
    Wire.begin(18, 17);
    #endif

    // I2C probe with retries — PN532 may need time after power-on or bus reset
    uint8_t probeResult = 0xFF;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(0x24);
        probeResult = Wire.endTransmission();
        if (probeResult == 0) {
            LOG_INFO("NFC", String("PN532 found at I2C 0x24 (attempt ") + String(attempt) + ")");
            break;
        }
        LOG_INFO("NFC", String("I2C probe attempt ") + String(attempt) + "/3 failed (error " + String(probeResult) + "), retrying...");
        delay(500);  // Give PN532 time to recover from stuck I2C bus
    }
    if (probeResult != 0) {
        LOG_INFO("NFC", String("No device at I2C 0x24 after 3 attempts \u2013 PN532 not connected"));
        return false;
    }

    // Configure IRQ pin: active LOW with pull-up. Must be done before
    // PN532 constructor (which only sets INPUT, not INPUT_PULLUP).
    pinMode(PIN_NFC_IRQ, INPUT_PULLUP);

    s_emulNfc = new Adafruit_PN532(PIN_NFC_IRQ, /* rst= */ -1, &Wire);

    if (!s_emulNfc->begin()) {
        LOG_ERROR("NFC", "PN532 begin() failed for card emulation");
        delete s_emulNfc;
        s_emulNfc = nullptr;
        return false;
    }

    uint32_t versiondata = s_emulNfc->getFirmwareVersion();
    if (!versiondata) {
        LOG_ERROR("NFC", "PN532 firmware version not readable");
        delete s_emulNfc;
        s_emulNfc = nullptr;
        return false;
    }

    LOG_INFO("NFC", String("Found PN532 v") +
                        String((versiondata >> 24) & 0xFF) + ", fw v" +
                        String((versiondata >> 16) & 0xFF) + "." +
                        String((versiondata >> 8) & 0xFF));

    // Configure SAM for target mode
    if (!s_emulNfc->SAMConfig()) {
        LOG_ERROR("NFC", "SAMConfig failed for card emulation");
        delete s_emulNfc;
        s_emulNfc = nullptr;
        return false;
    }

    s_emulRunning = true;
    nfcConfig.emulationActive = true;

    BaseType_t res = xTaskCreatePinnedToCore(
        emulation_task_code,
        "NFCEmulation",
        8192,
        nullptr,
        1,              // Priority 1 – equal to other peripheral tasks
        &s_emulTaskHandle,
        0               // Core 0
    );

    if (res != pdTRUE || s_emulTaskHandle == nullptr) {
        LOG_ERROR("NFC", "Failed to create emulation FreeRTOS task");
        delete s_emulNfc;
        s_emulNfc = nullptr;
        s_emulRunning = false;
        nfcConfig.emulationActive = false;
        return false;
    }

    LOG_INFO("NFC", "✓ Card emulation ready (Target Mode)");
    return true;
}

void nfcCardEmulationStop()
{
    LOG_INFO("NFC", "Stopping card emulation...");
    s_emulRunning = false;
    nfcConfig.emulationActive = false;

    // Wait for task to finish (with timeout)
    for (int i = 0; i < 50 && s_emulTaskHandle != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_emulTaskHandle != nullptr) {
        LOG_WARN("NFC", "Emulation task did not exit cleanly — deleting");
        vTaskDelete(s_emulTaskHandle);
        s_emulTaskHandle = nullptr;
    }

    if (s_emulNfc) {
        delete s_emulNfc;
        s_emulNfc = nullptr;
    }

    LOG_INFO("NFC", "Card emulation stopped");
}

bool nfcCardEmulationIsActive()
{
    return s_emulRunning && s_emulTaskHandle != nullptr;
}

#endif // ENABLE_NFC
