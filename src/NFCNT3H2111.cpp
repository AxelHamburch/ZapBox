#include "NFCNT3H2111.h"

#ifdef ENABLE_NFC

#include <Arduino.h>
#include <Wire.h>
#include "GlobalState.h"
#include "Log.h"
#include "PinConfig.h"

// ─── Constants ───────────────────────────────────────────────────────────────

static constexpr uint8_t  NT3H_ADDR        = 0x55;  // 7-bit I²C address
static constexpr uint8_t  NT3H_BLOCK_SIZE  = 16;    // bytes per I²C block
static constexpr uint8_t  NT3H_USER_START  = 0x01;  // first user-memory block
static constexpr uint32_t NT3H_WRITE_DELAY = 20;    // ms — NT3H2111 needs ≤5 ms per 4-byte NFC page; 16-byte I²C block = 4 pages → ≤20 ms

// Factory-programmed Capability Container (block 0, bytes 12–15)
// E1 = NFC Forum magic, 10 = v1.0, 6D = 872 bytes (109×8), 00 = no access cond.
static const uint8_t CC_EXPECTED[4] = {0xE1, 0x10, 0x6D, 0x00};

// ─── Module state ────────────────────────────────────────────────────────────

static bool initialized    = false;
static char lastWritten[300] = "";  // last URL written to chip

// ─── I²C helpers ─────────────────────────────────────────────────────────────

/** Write 16 bytes to one NT3H2111 block. */
static bool nt3hWriteBlock(uint8_t block, const uint8_t data[NT3H_BLOCK_SIZE]) {
    Wire.beginTransmission(NT3H_ADDR);
    Wire.write(block);
    Wire.write(data, NT3H_BLOCK_SIZE);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_WARN("NT3H", String("writeBlock b=") + String(block) + " err=" + String(err));
        return false;
    }
    delay(NT3H_WRITE_DELAY);
    return true;
}

/** Read 16 bytes from one NT3H2111 block.
 * Uses two separate I²C transactions (WRITE block addr + STOP, then READ)
 * instead of repeated-start, which is unreliable on some ESP32 Wire builds. */
static bool nt3hReadBlock(uint8_t block, uint8_t out[NT3H_BLOCK_SIZE]) {
    // Transaction 1: write block address, then STOP
    Wire.beginTransmission(NT3H_ADDR);
    Wire.write(block);
    if (Wire.endTransmission(true) != 0) return false;  // true = STOP
    delayMicroseconds(200);  // brief settle before re-addressing
    // Transaction 2: read 16 bytes
    uint8_t n = Wire.requestFrom((uint8_t)NT3H_ADDR, (uint8_t)NT3H_BLOCK_SIZE);
    if (n != NT3H_BLOCK_SIZE) return false;
    for (uint8_t i = 0; i < NT3H_BLOCK_SIZE; i++) out[i] = Wire.read();
    return true;
}

// ─── NDEF builder ─────────────────────────────────────────────────────────────

/**
 * Build an NDEF TLV payload for a URI record and write it to the chip.
 *
 * Memory layout written starting at block NT3H_USER_START (0x01):
 *
 *   [03] [len]  D1 01 [payloadLen] 55 00 [uri_bytes…]  [FE]
 *    ^    ^     \___________ NDEF URI record __________/  ^
 *    |    |                                                |
 *  NDEF  length                                       Terminator
 *  TLV
 *
 * URI Identifier Code 0x00 = no prefix abbreviation (full URI stored,
 * e.g. "lightning:LNURL1dp68…").
 */
static bool nt3hWriteNdef(const char* uri) {
    size_t uriLen     = strlen(uri);
    size_t payloadLen = 1 + uriLen;        // identifier code byte + URI
    size_t recordLen  = 4 + payloadLen;    // D1 + 01 + [payloadLen] + 55 + payload

    if (uriLen > 285) {
        LOG_WARN("NT3H", "URI too long (>285 chars) — skipping NDEF write");
        return false;
    }

    // Flat buffer: max 1 + 1 + (4 + 1 + 285) + 1 = 293 bytes
    uint8_t buf[310] = {};
    size_t  pos = 0;

    // NDEF Message TLV header
    buf[pos++] = 0x03;
    if (recordLen <= 254) {
        buf[pos++] = (uint8_t)recordLen;
    } else {
        buf[pos++] = 0xFF;
        buf[pos++] = (uint8_t)(recordLen >> 8);
        buf[pos++] = (uint8_t)(recordLen & 0xFF);
    }

    // NDEF URI Record
    buf[pos++] = 0xD1;               // MB=1 ME=1 SR=1 IL=0 TNF=001 (Well Known)
    buf[pos++] = 0x01;               // Type Length = 1
    buf[pos++] = (uint8_t)payloadLen;
    buf[pos++] = 0x55;               // Record Type = 'U' (URI)
    buf[pos++] = 0x00;               // URI Identifier Code: no abbreviation
    memcpy(buf + pos, uri, uriLen);
    pos += uriLen;

    // Terminator TLV
    buf[pos++] = 0xFE;

    // Write in 16-byte blocks
    size_t numBlocks = (pos + NT3H_BLOCK_SIZE - 1) / NT3H_BLOCK_SIZE;
    for (size_t i = 0; i < numBlocks; i++) {
        uint8_t blk[NT3H_BLOCK_SIZE] = {};
        size_t  off    = i * NT3H_BLOCK_SIZE;
        size_t  toCopy = (pos - off < (size_t)NT3H_BLOCK_SIZE) ? (pos - off) : (size_t)NT3H_BLOCK_SIZE;
        memcpy(blk, buf + off, toCopy);
        if (!nt3hWriteBlock((uint8_t)(NT3H_USER_START + i), blk)) return false;
    }

    LOG_INFO("NT3H", String("NDEF written: ") + String(pos) + " B, " + String(numBlocks) + " block(s)");

    // Readback verification: read block 0x01 and log first 8 bytes
    uint8_t verify[NT3H_BLOCK_SIZE] = {};
    if (nt3hReadBlock(NT3H_USER_START, verify)) {
        char hexbuf[48];
        snprintf(hexbuf, sizeof(hexbuf),
                 "%02X %02X %02X %02X %02X %02X %02X %02X",
                 verify[0], verify[1], verify[2], verify[3],
                 verify[4], verify[5], verify[6], verify[7]);
        LOG_INFO("NT3H", String("Readback blk01[0..7]: ") + String(hexbuf));
        // First byte must be 0x03 (NDEF TLV tag)
        if (verify[0] != 0x03) {
            LOG_WARN("NT3H", String("Readback MISMATCH: expected 0x03, got 0x") + String(verify[0], HEX));
        } else {
            LOG_INFO("NT3H", "Readback OK: NDEF TLV tag 0x03 confirmed");
        }
    } else {
        LOG_WARN("NT3H", "Readback failed (I2C read error)");
    }

    return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool nfcNT3H2111Init() {
    // If the touch controller was not found (non-touch board), Wire may not have
    // been initialized yet. Re-calling Wire.begin() on an already-running bus is
    // harmless on ESP32 Arduino core, so we do it unconditionally to be safe.
#if !ENABLE_DISPLAY
    Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL, 400000);
#endif

    // Probe chip with up to 3 attempts (50 ms apart).
    // The NT3H2111 may need a moment after power-on before it ACKs on I²C.
    uint8_t probe = 0xFF;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(NT3H_ADDR);
        probe = Wire.endTransmission();
        if (probe == 0) break;
        LOG_INFO("NT3H", String("Probe attempt ") + String(attempt) + "/3 failed (err=" + String(probe) + ") — retrying...");
        delay(50);
    }
    if (probe != 0) {
        LOG_WARN("NT3H", String("NT3H2111 not found at I\xC2\xB2\x43 0x55 (err=") + String(probe) + ")");
        return false;
    }
    LOG_INFO("NT3H", "NT3H2111 detected at I\xC2\xB2\x43 0x55");

    // Read block 0: verify Capability Container (bytes 12–15) and log raw bytes
    uint8_t blk0[NT3H_BLOCK_SIZE] = {};
    if (nt3hReadBlock(0x00, blk0)) {
        char hexbuf[64];
        snprintf(hexbuf, sizeof(hexbuf),
                 "%02X %02X %02X %02X %02X %02X %02X %02X | CC: %02X %02X %02X %02X",
                 blk0[0], blk0[1], blk0[2], blk0[3],
                 blk0[4], blk0[5], blk0[6], blk0[7],
                 blk0[12], blk0[13], blk0[14], blk0[15]);
        LOG_INFO("NT3H", String("Block 0: ") + String(hexbuf));
        bool ccOk = (blk0[12] == CC_EXPECTED[0] &&
                     blk0[13] == CC_EXPECTED[1] &&
                     blk0[14] == CC_EXPECTED[2] &&
                     blk0[15] == CC_EXPECTED[3]);
        if (ccOk) {
            LOG_INFO("NT3H", "CC OK: E1 10 6D 00");
        } else {
            LOG_WARN("NT3H", "CC missing — writing E1 10 6D 00 to block 0 bytes 12-15");
            // Preserve UID/lock/OTP bytes (0-11), only patch the CC (12-15).
            blk0[12] = CC_EXPECTED[0];  // E1 — NFC Forum magic
            blk0[13] = CC_EXPECTED[1];  // 10 — version 1.0
            blk0[14] = CC_EXPECTED[2];  // 6D — 872 bytes user memory
            blk0[15] = CC_EXPECTED[3];  // 00 — no access conditions
            if (nt3hWriteBlock(0x00, blk0)) {
                // Readback to confirm
                uint8_t verifyCc[NT3H_BLOCK_SIZE] = {};
                if (nt3hReadBlock(0x00, verifyCc) &&
                    verifyCc[12] == CC_EXPECTED[0] &&
                    verifyCc[13] == CC_EXPECTED[1] &&
                    verifyCc[14] == CC_EXPECTED[2] &&
                    verifyCc[15] == CC_EXPECTED[3]) {
                    LOG_INFO("NT3H", "CC written and verified OK");
                } else {
                    LOG_WARN("NT3H", "CC write readback FAILED — tag may still not work");
                }
            } else {
                LOG_WARN("NT3H", "CC write FAILED");
            }
        }
    } else {
        LOG_WARN("NT3H", "Block 0 read FAILED — I2C issue?");
    }

    initialized = true;

    // Write initial NDEF if the LNURL is already available
    const char* lnurl = lightningConfig.lightning;
    if (strlen(lnurl) > 0) {
        if (nt3hWriteNdef(lnurl)) {
            strncpy(lastWritten, lnurl, sizeof(lastWritten) - 1);
            lastWritten[sizeof(lastWritten) - 1] = '\0';
        }
    } else {
        LOG_INFO("NT3H", "LNURL not yet available — NDEF will be written once lightning URL is set");
    }

    return true;
}

void nfcNT3H2111Stop() {
    initialized = false;
    lastWritten[0] = '\0';
    LOG_INFO("NT3H", "NT3H2111 stopped");
}

void nfcNT3H2111UpdateIfChanged() {
    if (!initialized) return;

    const char* current = lightningConfig.lightning;
    if (strlen(current) == 0) return;
    if (strncmp(current, lastWritten, sizeof(lastWritten) - 1) == 0) return;

    LOG_INFO("NT3H", "LNURL changed — updating NDEF");
    if (nt3hWriteNdef(current)) {
        strncpy(lastWritten, current, sizeof(lastWritten) - 1);
        lastWritten[sizeof(lastWritten) - 1] = '\0';
    }
}

#endif // ENABLE_NFC
