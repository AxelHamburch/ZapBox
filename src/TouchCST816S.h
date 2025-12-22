#pragma once

#include <Arduino.h>
#include <Wire.h>

// Possible I2C addresses for CST816/CST328
#define CST816_SLAVE_ADDRESS 0x15
#define CST328_SLAVE_ADDRESS 0x5A

// Registers
#define CST8xx_REG_STATUS      0x00
#define CST8xx_REG_GESTURE     0x01
#define CST8xx_REG_TOUCH_NUM   0x02
#define CST8xx_REG_XPOS_HIGH   0x03
#define CST8xx_REG_XPOS_LOW    0x04
#define CST8xx_REG_YPOS_HIGH   0x05
#define CST8xx_REG_YPOS_LOW    0x06
#define CST8xx_REG_DIS_AUTOSLEEP 0xFE
#define CST8xx_REG_CHIP_ID     0xA7

// Gesture IDs
#define GESTURE_NONE 0x00
#define GESTURE_SWIPE_DOWN 0x01
#define GESTURE_SWIPE_UP 0x02
#define GESTURE_SWIPE_LEFT 0x03
#define GESTURE_SWIPE_RIGHT 0x04
#define GESTURE_SINGLE_CLICK 0x05
#define GESTURE_DOUBLE_CLICK 0x0B
#define GESTURE_LONG_PRESS 0x0C

class TouchCST816S {
private:
    TwoWire *_wire;
    int _sda;
    int _scl;
    int _rst;
    int _irq;
    uint8_t _addr;
    bool _initialized;

public:
    TouchCST816S(TwoWire &wire, int sda, int scl, int rst, int irq);
    bool begin();
    bool available();
    bool isPressed();
    uint8_t getGesture();
    uint16_t getX();
    uint16_t getY();
    uint8_t getTouchPoints();
    void disableAutoSleep();
    
private:
    uint8_t readByte(uint8_t reg);
    void writeByte(uint8_t reg, uint8_t value);
    bool probeAddress(uint8_t addr);
};
