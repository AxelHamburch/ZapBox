// ============================================================
//  NT3H2111 Minimal I2C Test
//  Pinout: SDA=GPIO18, SCL=GPIO17, VCC=3.3V, GND=GND
//  I2C address: 0x55 (7-bit)
// ============================================================
#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN     18
#define SCL_PIN     17
#define NT3H_ADDR   0x55
#define NT3H_ADDR_DEFAULT_BYTE 0xAA
#define BLOCK_SIZE  16

static const char* kLightningLnurl =
    "lightning:LNURL1DP68GURN8GHJ7V33D45K7TNNWPSKXEF00FSHQCN00QHKZURF9AMRZTMVDE6HYMP02UMRX3Z8V9PKXDNJ2PN9JKTCFEGHW468VAHR7URFDC7NZVSPUC5HR";

// ---- helpers -----------------------------------------------

static const char* wireErr(uint8_t e) {
    switch (e) {
        case 0: return "OK";
        case 1: return "buffer overflow";
        case 2: return "NACK on address";
        case 3: return "NACK on data";
        case 4: return "other error";
        case 5: return "timeout";
        default: return "unknown";
    }
}

static bool probeAddr(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

static uint8_t scanI2CAll(uint8_t found[128]) {
    uint8_t cnt = 0;
    for (uint8_t a = 0; a < 128; a++) {
        if (probeAddr(a)) {
            found[cnt++] = a;
            if (cnt >= 128) break;
        }
    }
    return cnt;
}

static bool readBlockAtAddr(uint8_t addr, uint8_t block, uint8_t out[BLOCK_SIZE]) {
    // Step 1: write the block address
    Wire.beginTransmission(addr);
    Wire.write(block);
    uint8_t e = Wire.endTransmission(true);
    if (e != 0) {
        Serial.printf("  [readBlock] addr=0x%02X block=0x%02X err=%d (%s)\n",
                      addr, block, e, wireErr(e));
        return false;
    }
    delayMicroseconds(200);
    // Step 2: read 16 bytes
    uint8_t n = Wire.requestFrom((uint8_t)addr, (uint8_t)BLOCK_SIZE);
    if (n != BLOCK_SIZE) {
        Serial.printf("  [readBlock] got %d bytes, expected %d\n", n, BLOCK_SIZE);
        return false;
    }
    for (int i = 0; i < BLOCK_SIZE; i++) out[i] = Wire.read();
    return true;
}

static bool readBlock(uint8_t block, uint8_t out[BLOCK_SIZE]) {
    return readBlockAtAddr(NT3H_ADDR, block, out);
}

static bool writeBlockAtAddr(uint8_t addr, uint8_t block, const uint8_t data[BLOCK_SIZE]) {
    Wire.beginTransmission(addr);
    Wire.write(block);
    Wire.write(data, BLOCK_SIZE);
    uint8_t e = Wire.endTransmission(true);
    Serial.printf("  [writeBlock] addr=0x%02X block=0x%02X err=%d (%s)\n",
                  addr, block, e, wireErr(e));
    delay(5);  // NT3H2111 EEPROM write cycle ≤ 5 ms
    return (e == 0);
}

static bool writeBlock(uint8_t block, const uint8_t data[BLOCK_SIZE]) {
    return writeBlockAtAddr(NT3H_ADDR, block, data);
}

static bool writeBytesToBlocksAtAddr(uint8_t addr, uint8_t startBlock, const uint8_t* data, size_t len) {
    size_t offset = 0;
    uint8_t block = startBlock;
    while (offset < len) {
        uint8_t chunk[BLOCK_SIZE] = {};
        size_t n = len - offset;
        if (n > BLOCK_SIZE) n = BLOCK_SIZE;
        memcpy(chunk, data + offset, n);
        if (!writeBlockAtAddr(addr, block, chunk)) return false;
        offset += n;
        block++;
    }
    return true;
}

static bool writeBytesToBlocks(uint8_t startBlock, const uint8_t* data, size_t len) {
    return writeBytesToBlocksAtAddr(NT3H_ADDR, startBlock, data, len);
}

static bool readBytesFromBlocksAtAddr(uint8_t addr, uint8_t startBlock, uint8_t* out, size_t len) {
    size_t offset = 0;
    uint8_t block = startBlock;
    while (offset < len) {
        uint8_t chunk[BLOCK_SIZE] = {};
        if (!readBlockAtAddr(addr, block, chunk)) return false;
        size_t n = len - offset;
        if (n > BLOCK_SIZE) n = BLOCK_SIZE;
        memcpy(out + offset, chunk, n);
        offset += n;
        block++;
    }
    return true;
}

static bool readBytesFromBlocks(uint8_t startBlock, uint8_t* out, size_t len) {
    return readBytesFromBlocksAtAddr(NT3H_ADDR, startBlock, out, len);
}

static bool looksLikeNTAG(uint8_t addr, uint8_t blk0[BLOCK_SIZE]) {
    if (!readBlockAtAddr(addr, 0x00, blk0)) return false;
    if (blk0[0] != 0x04) return false;
    if (blk0[8] != 0x44) return false;
    return true;
}

static bool recoverAddrToDefault55(uint8_t fromAddr) {
    if (fromAddr == NT3H_ADDR) return true;

    uint8_t blk0[BLOCK_SIZE] = {};
    if (!readBlockAtAddr(fromAddr, 0x00, blk0)) return false;

    // Byte 0 configures the I2C address from I2C perspective.
    blk0[0] = NT3H_ADDR_DEFAULT_BYTE;

    Serial.printf("[RECOVER] Restoring NTAG I2C addr from 0x%02X to 0x%02X ...\n", fromAddr, NT3H_ADDR);
    if (!writeBlockAtAddr(fromAddr, 0x00, blk0)) return false;

    delay(10);
    return probeAddr(NT3H_ADDR);
}

static void hexdump(const char* label, const uint8_t* buf, int len) {
    Serial.print(label);
    for (int i = 0; i < len; i++) Serial.printf(" %02X", buf[i]);
    Serial.print("  |  ");
    for (int i = 0; i < len; i++)
        Serial.print((buf[i] >= 0x20 && buf[i] < 0x7F) ? (char)buf[i] : '.');
    Serial.println();
}

// ---- setup (runs once) -------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println("\n================================================");
    Serial.println("  NT3H2111 Minimal I2C Test");
    Serial.printf("  SDA=GPIO%d  SCL=GPIO%d  addr=0x%02X\n", SDA_PIN, SCL_PIN, NT3H_ADDR);
    Serial.println("================================================\n");

    // Explicitly force GPIO pull-ups BEFORE Wire.begin.
    // ESP32 internal pull-ups (~47kΩ) are marginal; this ensures they are active.
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delay(10);
    Wire.begin(SDA_PIN, SCL_PIN, 100000UL);   // 100 kHz standard mode
    delay(200);

    // ---- 1. Bus scan ----------------------------------------
    Serial.println("[SCAN] Scanning I2C bus 0x00 .. 0x7F ...");
    uint8_t found[128] = {};
    uint8_t foundCount = scanI2CAll(found);
    for (uint8_t i = 0; i < foundCount; i++) {
        Serial.printf("       Device found: 0x%02X\n", found[i]);
    }
    if (foundCount == 0) {
        Serial.println("       !! No I2C devices found at all !!");
        Serial.println("       >> Check SDA/SCL wiring and pull-up resistors.");
        Serial.println("       >> Retrying in loop...");
        return;
    }

    // Auto-detect NTAG and recover to default 0x55 if needed.
    bool ntagAtDefault = probeAddr(NT3H_ADDR);
    if (!ntagAtDefault) {
        Serial.println("[RECOVER] NTAG not at 0x55, trying to find and restore default address...");
        for (uint8_t i = 0; i < foundCount; i++) {
            uint8_t cand = found[i];
            uint8_t blk0cand[BLOCK_SIZE] = {};
            if (!looksLikeNTAG(cand, blk0cand)) continue;
            Serial.printf("       NTAG candidate detected at 0x%02X\n", cand);
            if (recoverAddrToDefault55(cand)) {
                Serial.println("       ✓ Recovered to default address 0x55");
                ntagAtDefault = true;
                break;
            }
        }
    }

    // ---- 2. Probe NT3H2111 specifically ---------------------
    Serial.printf("\n[PROBE] Probing 0x%02X ...\n", NT3H_ADDR);
    uint8_t probeErr = probeAddr(NT3H_ADDR) ? 0 : 2;
    if (probeErr != 0) {
        Serial.printf("        FAIL — err=%d (%s)\n", probeErr, wireErr(probeErr));
        Serial.println("        Hint: address may still be changed or module/bus is unstable.");
        return;
    }
    Serial.println("        OK — NT3H2111 found!\n");

    // ---- 3. Read block 0 (UID + Lock + CC) ------------------
    Serial.println("[READ]  Block 0 (UID / Lock bytes / CC):");
    uint8_t blk0[BLOCK_SIZE] = {};
    if (readBlock(0x00, blk0)) {
        hexdump("        ", blk0, BLOCK_SIZE);
        Serial.printf("  UID:  %02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                      blk0[0], blk0[1], blk0[3], blk0[4], blk0[5], blk0[6], blk0[7]);
        Serial.printf("  Lock: %02X %02X %02X %02X\n",
                      blk0[8], blk0[9], blk0[10], blk0[11]);
        Serial.printf("  CC:   %02X %02X %02X %02X\n",
                      blk0[12], blk0[13], blk0[14], blk0[15]);
    } else {
        Serial.println("        FAIL");
    }

    // ---- 4. Ensure CC bytes (NFC Forum Type 2) --------------
    Serial.println("\n[CC]    Setting CC bytes in block 0 to E1 10 6D 00 ...");
    uint8_t blk0cc[BLOCK_SIZE] = {};
    if (!readBlock(0x00, blk0cc)) {
        Serial.println("        FAIL (cannot read block 0)");
        return;
    }
    blk0cc[12] = 0xE1;
    blk0cc[13] = 0x10;
    blk0cc[14] = 0x6D;
    blk0cc[15] = 0x00;
    blk0cc[0] = NT3H_ADDR_DEFAULT_BYTE; // Never overwrite I2C address accidentally.
    if (!writeBlock(0x00, blk0cc)) {
        Serial.println("        FAIL (cannot write CC bytes)");
        return;
    }
    uint8_t blk0verify[BLOCK_SIZE] = {};
    if (!readBlock(0x00, blk0verify)) {
        Serial.println("        FAIL (cannot verify block 0)");
        return;
    }
    Serial.printf("        CC verify: %02X %02X %02X %02X\n",
                  blk0verify[12], blk0verify[13], blk0verify[14], blk0verify[15]);

    // ---- 5. Build NDEF URI record ----------------------------
    Serial.println("\n[NDEF]  Building URI record for provided lightning LNURL ...");
    size_t uriLen = strlen(kLightningLnurl);
    size_t payloadLen = 1 + uriLen;      // 0x00 prefix byte + full URI string
    size_t ndefLen = 4 + payloadLen;     // D1 01 PL 55 + payload
    if (ndefLen > 0xFE) {
        Serial.printf("        FAIL (NDEF too large: %u bytes)\n", (unsigned)ndefLen);
        return;
    }

    uint8_t ndef[256] = {};
    size_t p = 0;
    ndef[p++] = 0x03;                    // NDEF TLV
    ndef[p++] = (uint8_t)ndefLen;        // TLV length
    ndef[p++] = 0xD1;                    // MB=1, ME=1, SR=1, TNF=Well Known
    ndef[p++] = 0x01;                    // Type length: 1 ('U')
    ndef[p++] = (uint8_t)payloadLen;     // Payload length
    ndef[p++] = 0x55;                    // Type 'U' (URI)
    ndef[p++] = 0x00;                    // URI prefix: none, full URI follows
    memcpy(ndef + p, kLightningLnurl, uriLen);
    p += uriLen;
    ndef[p++] = 0xFE;                    // Terminator TLV

    Serial.printf("        URI length: %u bytes\n", (unsigned)uriLen);
    Serial.printf("        NDEF/TLV bytes to write: %u\n", (unsigned)p);

    // ---- 6. Write NDEF bytes starting at user memory block 1-
    Serial.println("\n[WRITE] Writing NDEF to blocks starting at 0x01 ...");
    if (!writeBytesToBlocks(0x01, ndef, p)) {
        Serial.println("        FAIL (writeBytesToBlocks)");
        return;
    }

    // ---- 7. Read back and verify -----------------------------
    Serial.println("\n[VERIFY] Reading back and comparing ...");
    uint8_t verify[256] = {};
    if (!readBytesFromBlocks(0x01, verify, p)) {
        Serial.println("        FAIL (readBytesFromBlocks)");
        return;
    }
    if (memcmp(verify, ndef, p) == 0) {
        Serial.println("        ✓ WRITE SUCCESS — NDEF readback matches!");
    } else {
        Serial.println("        ✗ WRITE FAILED — readback mismatch.");
        for (size_t i = 0; i < p; i++) {
            if (verify[i] != ndef[i]) {
                Serial.printf("        First diff at byte %u: wrote=%02X read=%02X\n",
                              (unsigned)i, ndef[i], verify[i]);
                break;
            }
        }
        return;
    }

    // ---- 8. Read config register (block 0x38) ---------------
    Serial.println("\n[CFG]   Config register (block 0x38):");
    uint8_t cfg[BLOCK_SIZE] = {};
    if (readBlock(0x38, cfg)) {
        hexdump("        ", cfg, BLOCK_SIZE);
        Serial.printf("  NC_REG (byte 0): 0x%02X\n", cfg[0]);
        Serial.printf("  NS_REG (byte 8): 0x%02X", cfg[8]);
        if (cfg[8] & 0x40) Serial.print("  << RF_LOCKED");
        if (cfg[8] & 0x80) Serial.print("  << I2C_LOCKED");
        Serial.println();
    } else {
        Serial.println("        FAIL (config reg read error)");
    }

    Serial.println("\n================================================");
    Serial.println("  Test complete. Will probe again every 5s.");
    Serial.println("================================================\n");
}

// ---- loop (re-probe & re-test every 5 seconds) -------------

void loop() {
    delay(5000);
    Serial.println("[LOOP]  Full scan 0x00..0x7F and probe 0x55 ...");
    uint8_t found[128] = {};
    uint8_t n = scanI2CAll(found);
    Serial.printf("        devices=%u", n);
    if (n > 0) {
        Serial.print(" [");
        for (uint8_t i = 0; i < n; i++) {
            Serial.printf("0x%02X", found[i]);
            if (i + 1 < n) Serial.print(", ");
        }
        Serial.print("]");
    }
    Serial.println();
    uint8_t e = probeAddr(NT3H_ADDR) ? 0 : 2;
    Serial.printf("        probe 0x55 err=%d (%s)\n", e, wireErr(e));
}
