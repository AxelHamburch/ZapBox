#include "TouchAXS15231B.h"

// I²C address (7-bit) of the AXS15231B touch controller
#define AXS_ADDR 0x3B

// Native panel dimensions (the touch sensor reports raw values in this range)
#define PANEL_W 320
#define PANEL_H 480

// Read window: 1 touch point => 1*6 + 2 = 8 response bytes.
// The 11-byte command sequence is fixed; the embedded length matches.
static const uint8_t kReadCmd[11] = {
  0xB5, 0xAB, 0xA5, 0x5A,
  0x00, 0x00, 0x00, 0x08,    // length high/low (= 8 bytes)
  0x00, 0x00, 0x00
};

TouchAXS15231B::TouchAXS15231B(TwoWire &wire, int sda, int scl, int rst, int irq)
  : _wire(&wire), _sda(sda), _scl(scl), _rst(rst), _irq(irq),
    _initialized(false), _x(0), _y(0), _points(0), _gesture(0),
    _lastReadMs(0) {}

bool TouchAXS15231B::probe() {
  _wire->beginTransmission(AXS_ADDR);
  return (_wire->endTransmission() == 0);
}

bool TouchAXS15231B::begin() {
  _wire->begin(_sda, _scl, 400000);   // 400 kHz on the touch-only bus

  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(20);
    digitalWrite(_rst, HIGH);
    delay(50);
  }
  if (_irq >= 0) {
    pinMode(_irq, INPUT);
  }

  if (!probe()) {
    Serial.printf("[TOUCH] AXS15231B not found at 0x%02X (SDA=%d SCL=%d)\n",
                  AXS_ADDR, _sda, _scl);
    return false;
  }

  Serial.printf("[TOUCH] AXS15231B found at 0x%02X (SDA=%d SCL=%d)\n",
                AXS_ADDR, _sda, _scl);
  _initialized = true;
  return true;
}

bool TouchAXS15231B::readTouchData() {
  if (!_initialized) return false;

  // Cache for ~10 ms so available()/isPressed()/getX()/getY() don't each
  // re-trigger an I²C transaction within a single processing tick.
  unsigned long now = millis();
  if (now - _lastReadMs < 10) return _points > 0;
  _lastReadMs = now;

  // Send the 11-byte read command
  _wire->beginTransmission(AXS_ADDR);
  _wire->write(kReadCmd, sizeof(kReadCmd));
  if (_wire->endTransmission() != 0) {
    _points = 0;
    return false;
  }

  // Read 8-byte response
  uint8_t buf[8] = {0};
  uint8_t got = _wire->requestFrom(AXS_ADDR, (uint8_t)sizeof(buf));
  if (got != sizeof(buf)) {
    _points = 0;
    return false;
  }
  for (uint8_t i = 0; i < got && _wire->available(); i++) {
    buf[i] = _wire->read();
  }

  _gesture = buf[0];
  _points  = buf[1];

  if (_points == 0 || _points > 2) {
    _points = 0;
    return false;
  }

  // Native portrait coords from the sensor
  uint16_t tx = ((buf[2] & 0x0F) << 8) | buf[3];
  uint16_t ty = ((buf[4] & 0x0F) << 8) | buf[5];

  if (tx >= PANEL_W) tx = PANEL_W - 1;
  if (ty >= PANEL_H) ty = PANEL_H - 1;

  // Rotate 90° CCW to match DisplayTouch.cpp's putPixel mapping:
  //   landscape lx = native ty
  //   landscape ly = (PANEL_W - 1) - native tx
  _x = ty;
  _y = (PANEL_W - 1) - tx;
  return true;
}

bool TouchAXS15231B::available() {
  if (!_initialized) return false;
  // Without a usable INT line we always poll the bus.
  return readTouchData();
}

bool TouchAXS15231B::isPressed() {
  if (!_initialized) return false;
  readTouchData();
  return _points > 0;
}

uint8_t TouchAXS15231B::getGesture() {
  if (!_initialized) return 0;
  readTouchData();
  return _gesture;
}

uint16_t TouchAXS15231B::getX() {
  if (!_initialized) return 0;
  readTouchData();
  return _x;
}

uint16_t TouchAXS15231B::getY() {
  if (!_initialized) return 0;
  readTouchData();
  return _y;
}

uint8_t TouchAXS15231B::getTouchPoints() {
  if (!_initialized) return 0;
  readTouchData();
  return _points;
}
