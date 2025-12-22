#include "TouchCST816S.h"

TouchCST816S::TouchCST816S(TwoWire &wire, int sda, int scl, int rst, int irq)
    : _wire(&wire), _sda(sda), _scl(scl), _rst(rst), _irq(irq), _addr(0), _initialized(false) {
}

bool TouchCST816S::begin() {
    // Initialize I2C
    _wire->begin(_sda, _scl);
    
    // CRITICAL: Reset the touch controller FIRST
    // gpio_hold_dis is needed if coming from deep sleep
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(30);  // Min 20ms
        digitalWrite(_rst, HIGH);
        delay(50);  // Wait for chip to be ready
    }
    
    // Setup interrupt pin if provided
    if (_irq >= 0) {
        pinMode(_irq, INPUT);
    }
    
    // Scan for touch controller address
    if (probeAddress(CST816_SLAVE_ADDRESS)) {
        _addr = CST816_SLAVE_ADDRESS;
        Serial.printf("[TOUCH] Found CST816 at 0x%02X\n", _addr);
    } else if (probeAddress(CST328_SLAVE_ADDRESS)) {
        _addr = CST328_SLAVE_ADDRESS;
        Serial.printf("[TOUCH] Found CST328 at 0x%02X\n", _addr);
    } else {
        Serial.println("[TOUCH] Touch controller not found (non-touch version)");
        return false;
    }
    
    // Read chip ID for verification
    uint8_t chipID = readByte(CST8xx_REG_CHIP_ID);
    Serial.printf("[TOUCH] Chip ID: 0x%02X\n", chipID);
    
    _initialized = true;
    
    // IMPORTANT: Disable auto-sleep to keep touch responsive
    disableAutoSleep();
    
    Serial.println("[TOUCH] Touch controller initialized successfully");
    return true;
}

bool TouchCST816S::probeAddress(uint8_t addr) {
    _wire->beginTransmission(addr);
    return (_wire->endTransmission() == 0);
}

bool TouchCST816S::available() {
    if (!_initialized) return false;
    
    // Check IRQ pin if available (more efficient)
    if (_irq >= 0) {
        return digitalRead(_irq) == LOW; // IRQ is active low
    }
    
    // Otherwise check touch count
    return isPressed();
}

bool TouchCST816S::isPressed() {
    if (!_initialized) return false;
    uint8_t touchNum = readByte(CST8xx_REG_TOUCH_NUM);
    // Some chips return 0xFF when not touched
    return (touchNum > 0 && touchNum != 0xFF);
}

uint8_t TouchCST816S::getGesture() {
    if (!_initialized) return GESTURE_NONE;
    return readByte(CST8xx_REG_GESTURE);
}

uint16_t TouchCST816S::getX() {
    if (!_initialized) return 0;
    uint8_t xh = readByte(CST8xx_REG_XPOS_HIGH);
    uint8_t xl = readByte(CST8xx_REG_XPOS_LOW);
    return ((xh & 0x0F) << 8) | xl;
}

uint16_t TouchCST816S::getY() {
    if (!_initialized) return 0;
    uint8_t yh = readByte(CST8xx_REG_YPOS_HIGH);
    uint8_t yl = readByte(CST8xx_REG_YPOS_LOW);
    return ((yh & 0x0F) << 8) | yl;
}

uint8_t TouchCST816S::getTouchPoints() {
    if (!_initialized) return 0;
    uint8_t touchNum = readByte(CST8xx_REG_TOUCH_NUM);
    // Filter out invalid values
    if (touchNum == 0xFF) return 0;
    return touchNum;
}

void TouchCST816S::disableAutoSleep() {
    if (!_initialized) return;
    // Some CST816 chips have auto-sleep, disable it
    writeByte(CST8xx_REG_DIS_AUTOSLEEP, 0x01);
    delay(5);
    Serial.println("[TOUCH] Auto-sleep disabled");
}

uint8_t TouchCST816S::readByte(uint8_t reg) {
    if (!_addr) return 0;
    
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) {
        return 0;
    }
    
    _wire->requestFrom(_addr, (uint8_t)1);
    if (_wire->available()) {
        return _wire->read();
    }
    return 0;
}

void TouchCST816S::writeByte(uint8_t reg, uint8_t value) {
    if (!_addr) return;
    
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}
