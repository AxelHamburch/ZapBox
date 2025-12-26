#pragma once

#include <Arduino.h>
#include <Wire.h>

// PN532 I2C Address
#define PN532_I2C_ADDRESS 0x24

// PN532 Commands
#define PN532_COMMAND_GETFIRMWAREVERSION 0x02
#define PN532_COMMAND_SAMCONFIGURATION 0x14
#define PN532_COMMAND_INLISTPASSIVETARGET 0x4A
#define PN532_COMMAND_INDATAEXCHANGE 0x40
#define PN532_COMMAND_INRELEASE 0x52

// PN532 Frame identifiers
#define PN532_PREAMBLE 0x00
#define PN532_STARTCODE1 0x00
#define PN532_STARTCODE2 0xFF
#define PN532_POSTAMBLE 0x00
#define PN532_HOSTTOPN532 0xD4
#define PN532_PN532TOHOST 0xD5

// Card types
#define PN532_MIFARE_ISO14443A 0x00

// Response codes
#define PN532_ACK_WAIT_TIME 100  // milliseconds
#define PN532_DEFAULT_WAIT_TIME 1000  // milliseconds

class NFCPN532 {
public:
    /**
     * @brief Constructor for PN532 NFC reader
     * @param wire I2C interface (Wire object)
     * @param sda I2C SDA pin (shared with Touch)
     * @param scl I2C SCL pin (shared with Touch)
     * @param irq Interrupt pin (GPIO 1)
     * @param rst Reset pin (GPIO 2)
     */
    NFCPN532(TwoWire &wire, int sda, int scl, int irq, int rst);
    
    /**
     * @brief Initialize PN532 NFC reader
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Get firmware version of PN532
     * @return Firmware version as uint32_t (0 if error)
     */
    uint32_t getFirmwareVersion();
    
    /**
     * @brief Configure PN532 for card reading
     * @return true if configuration successful, false otherwise
     */
    bool SAMConfig();
    
    /**
     * @brief Read passive target (NFC card/tag)
     * @param uid Buffer to store UID (must be at least 7 bytes)
     * @param uidLength Pointer to store UID length
     * @param timeout Timeout in milliseconds (default: 1000ms)
     * @return true if card detected, false otherwise
     */
    bool readPassiveTargetID(uint8_t *uid, uint8_t *uidLength, uint16_t timeout = 1000);
    
    /**
     * @brief Check if IRQ pin is triggered (card present)
     * @return true if IRQ active (LOW), false otherwise
     */
    bool isCardPresent();
    
    /**
     * @brief Hardware reset of PN532
     */
    void reset();
    
    /**
     * @brief Check if PN532 is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() { return _initialized; }

private:
    TwoWire *_wire;
    int _sda;
    int _scl;
    int _irq;
    int _rst;
    bool _initialized;
    
    /**
     * @brief Write command to PN532
     * @param cmd Command buffer
     * @param cmdlen Command length
     */
    void writeCommand(const uint8_t *cmd, uint8_t cmdlen);
    
    /**
     * @brief Read data from PN532
     * @param buff Buffer to store data
     * @param len Length to read
     * @return Number of bytes read
     */
    uint8_t readData(uint8_t *buff, uint8_t len);
    
    /**
     * @brief Wait for ACK from PN532
     * @param timeout Timeout in milliseconds
     * @return true if ACK received, false otherwise
     */
    bool waitForAck(uint16_t timeout = PN532_ACK_WAIT_TIME);
    
    /**
     * @brief Check if data is ready to read
     * @return true if data ready, false otherwise
     */
    bool isReady();
    
    /**
     * @brief Calculate DCS (Data Checksum)
     * @param data Data buffer
     * @param len Data length
     * @return Checksum byte
     */
    uint8_t calculateDCS(const uint8_t *data, uint8_t len);
};
