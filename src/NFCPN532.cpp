#include "NFCPN532.h"

NFCPN532::NFCPN532(TwoWire &wire, int sda, int scl, int irq, int rst)
    : _wire(&wire), _sda(sda), _scl(scl), _irq(irq), _rst(rst), _initialized(false) {
}

bool NFCPN532::begin() {
    Serial.println("[NFC] Initializing PN532 NFC reader...");
    
    // Note: I2C already initialized by Touch controller
    // No need to call _wire->begin() again as it shares the same bus
    
    // Setup IRQ pin
    if (_irq >= 0) {
        pinMode(_irq, INPUT_PULLUP);
        Serial.printf("[NFC] IRQ pin configured: GPIO %d\n", _irq);
    }
    
    // Setup and perform hardware reset
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);
        delay(10);
        digitalWrite(_rst, LOW);
        delay(50);  // Min 20ms reset pulse
        digitalWrite(_rst, HIGH);
        delay(50);  // Wait for PN532 to boot
        Serial.printf("[NFC] Hardware reset performed: GPIO %d\n", _rst);
    }
    
    // Give PN532 time to initialize
    delay(100);
    
    // Try to get firmware version to verify communication
    uint32_t version = getFirmwareVersion();
    if (version == 0) {
        Serial.println("[NFC] ERROR: Failed to communicate with PN532");
        Serial.println("[NFC] Check wiring: SDA=GPIO18, SCL=GPIO17, IRQ=GPIO1, RST=GPIO2");
        return false;
    }
    
    // Print firmware version
    Serial.printf("[NFC] Found PN532 with firmware version: %d.%d\n", 
                  (version >> 8) & 0xFF, version & 0xFF);
    
    // Configure SAM (Security Access Module)
    if (!SAMConfig()) {
        Serial.println("[NFC] ERROR: Failed to configure SAM");
        return false;
    }
    
    _initialized = true;
    Serial.println("[NFC] PN532 initialized successfully");
    return true;
}

uint32_t NFCPN532::getFirmwareVersion() {
    uint8_t cmd[] = {PN532_COMMAND_GETFIRMWAREVERSION};
    writeCommand(cmd, sizeof(cmd));
    
    // Wait for response
    if (!waitForAck(500)) {
        Serial.println("[NFC] No ACK for GetFirmwareVersion");
        return 0;
    }
    
    // Wait for data ready
    uint32_t timeout = millis() + 1000;
    while (!isReady()) {
        if (millis() > timeout) {
            Serial.println("[NFC] Timeout waiting for firmware version");
            return 0;
        }
        delay(10);
    }
    
    // Read response
    uint8_t response[12];
    uint8_t len = readData(response, sizeof(response));
    
    if (len < 6 || response[0] != PN532_PN532TOHOST || 
        response[1] != (PN532_COMMAND_GETFIRMWAREVERSION + 1)) {
        Serial.println("[NFC] Invalid firmware version response");
        return 0;
    }
    
    // Return version as uint32_t: IC.Ver.Rev.Support
    return ((uint32_t)response[2] << 24) | ((uint32_t)response[3] << 16) |
           ((uint32_t)response[4] << 8) | response[5];
}

bool NFCPN532::SAMConfig() {
    // SAM configuration: Normal mode, timeout 50ms, use IRQ pin
    uint8_t cmd[] = {
        PN532_COMMAND_SAMCONFIGURATION,
        0x01,  // Normal mode
        0x14,  // Timeout 50ms * 20 = 1s
        0x01   // Use IRQ pin
    };
    
    writeCommand(cmd, sizeof(cmd));
    
    if (!waitForAck(500)) {
        Serial.println("[NFC] No ACK for SAMConfig");
        return false;
    }
    
    // Wait for response
    uint32_t timeout = millis() + 1000;
    while (!isReady()) {
        if (millis() > timeout) {
            Serial.println("[NFC] Timeout waiting for SAMConfig response");
            return false;
        }
        delay(10);
    }
    
    // Read response (should be simple ACK)
    uint8_t response[8];
    readData(response, sizeof(response));
    
    Serial.println("[NFC] SAM configured successfully");
    return true;
}

bool NFCPN532::readPassiveTargetID(uint8_t *uid, uint8_t *uidLength, uint16_t timeout) {
    // InListPassiveTarget command for ISO14443A (Mifare) cards
    uint8_t cmd[] = {
        PN532_COMMAND_INLISTPASSIVETARGET,
        0x01,  // Max 1 card
        PN532_MIFARE_ISO14443A  // Card type
    };
    
    writeCommand(cmd, sizeof(cmd));
    
    if (!waitForAck(500)) {
        return false;
    }
    
    // Wait for card detection (with timeout)
    uint32_t endTime = millis() + timeout;
    while (!isReady()) {
        if (millis() > endTime) {
            return false;  // No card found
        }
        delay(10);
    }
    
    // Read response
    uint8_t response[20];
    uint8_t len = readData(response, sizeof(response));
    
    // Check response validity
    if (len < 7 || response[0] != PN532_PN532TOHOST ||
        response[1] != (PN532_COMMAND_INLISTPASSIVETARGET + 1)) {
        return false;
    }
    
    // Number of tags found
    uint8_t numTargets = response[2];
    if (numTargets != 1) {
        return false;
    }
    
    // Extract UID length and UID
    *uidLength = response[6];  // UID length at byte 6
    
    if (*uidLength > 7) {
        *uidLength = 7;  // Limit to max 7 bytes
    }
    
    // Copy UID
    for (uint8_t i = 0; i < *uidLength; i++) {
        uid[i] = response[7 + i];
    }
    
    return true;
}

bool NFCPN532::isCardPresent() {
    if (_irq < 0) {
        return false;  // No IRQ pin configured
    }
    
    // PN532 IRQ is active LOW when data is ready
    return (digitalRead(_irq) == LOW);
}

void NFCPN532::reset() {
    if (_rst < 0) {
        return;  // No reset pin configured
    }
    
    Serial.println("[NFC] Performing hardware reset...");
    digitalWrite(_rst, LOW);
    delay(50);
    digitalWrite(_rst, HIGH);
    delay(100);  // Wait for PN532 to reinitialize
}

// Private methods

void NFCPN532::writeCommand(const uint8_t *cmd, uint8_t cmdlen) {
    uint8_t checksum;
    
    // Calculate command checksum (DCS)
    checksum = PN532_HOSTTOPN532;
    for (uint8_t i = 0; i < cmdlen; i++) {
        checksum += cmd[i];
    }
    checksum = ~checksum + 1;  // Two's complement
    
    // Build frame
    _wire->beginTransmission(PN532_I2C_ADDRESS);
    
    // Preamble and start code
    _wire->write(PN532_PREAMBLE);
    _wire->write(PN532_STARTCODE1);
    _wire->write(PN532_STARTCODE2);
    
    // Length and length checksum
    uint8_t length = cmdlen + 1;  // +1 for TFI
    _wire->write(length);
    _wire->write(~length + 1);  // Length checksum (LCS)
    
    // TFI (Frame Identifier)
    _wire->write(PN532_HOSTTOPN532);
    
    // Data
    for (uint8_t i = 0; i < cmdlen; i++) {
        _wire->write(cmd[i]);
    }
    
    // Data checksum
    _wire->write(checksum);
    
    // Postamble
    _wire->write(PN532_POSTAMBLE);
    
    _wire->endTransmission();
}

uint8_t NFCPN532::readData(uint8_t *buff, uint8_t len) {
    // Request data from PN532
    _wire->requestFrom(PN532_I2C_ADDRESS, len + 1);  // +1 for ready byte
    
    // Skip ready byte (0x01)
    _wire->read();
    
    // Read actual data
    uint8_t bytesRead = 0;
    while (_wire->available() && bytesRead < len) {
        buff[bytesRead++] = _wire->read();
    }
    
    return bytesRead;
}

bool NFCPN532::waitForAck(uint16_t timeout) {
    const uint8_t ackFrame[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t ackBuff[6];
    
    uint32_t endTime = millis() + timeout;
    
    while (millis() < endTime) {
        if (isReady()) {
            // Read ACK frame
            readData(ackBuff, sizeof(ackBuff));
            
            // Verify ACK
            bool isAck = true;
            for (uint8_t i = 0; i < 6; i++) {
                if (ackBuff[i] != ackFrame[i]) {
                    isAck = false;
                    break;
                }
            }
            
            if (isAck) {
                return true;
            }
        }
        delay(5);
    }
    
    return false;
}

bool NFCPN532::isReady() {
    if (_irq >= 0) {
        // Use IRQ pin if available (more efficient)
        return (digitalRead(_irq) == LOW);
    } else {
        // Fallback: Poll I2C status byte
        _wire->requestFrom(PN532_I2C_ADDRESS, 1);
        if (_wire->available()) {
            uint8_t status = _wire->read();
            return (status == 0x01);  // Ready status
        }
        return false;
    }
}

uint8_t NFCPN532::calculateDCS(const uint8_t *data, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum + 1;  // Two's complement
}
