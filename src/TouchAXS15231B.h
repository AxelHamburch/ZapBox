#pragma once

#include <Arduino.h>
#include <Wire.h>

// Gesture codes — these mirror TouchCST816S.h so call sites can stay generic.
// AXS15231B reports gesture codes in the first response byte; values are the
// same convention used by CST816 family (single click, swipes, long press).
#ifndef GESTURE_NONE
#define GESTURE_NONE          0x00
#define GESTURE_SWIPE_DOWN    0x01
#define GESTURE_SWIPE_UP      0x02
#define GESTURE_SWIPE_LEFT    0x03
#define GESTURE_SWIPE_RIGHT   0x04
#define GESTURE_SINGLE_CLICK  0x05
#define GESTURE_DOUBLE_CLICK  0x0B
#define GESTURE_LONG_PRESS    0x0C
#endif

// I²C touch driver for the AXS15231B controller used on JC3248W535C.
// The chip sits on an INTERNAL I²C bus (SDA=GPIO 4, SCL=GPIO 8) — we drive
// it with the secondary `Wire1` peripheral so the primary `Wire` (SDA=18,
// SCL=17) stays free for NFC modules.
//
// Public API matches TouchCST816S so existing call sites in main.cpp /
// Navigation.cpp can use either driver via `#ifdef`.

class TouchAXS15231B {
public:
  TouchAXS15231B(TwoWire &wire, int sda, int scl, int rst = -1, int irq = -1);

  bool begin();
  bool available();
  bool isPressed();
  uint8_t  getGesture();
  uint16_t getX();
  uint16_t getY();
  uint8_t  getTouchPoints();

private:
  bool probe();
  // Reads + parses touch data, caching the result for a few ms so back-to-back
  // calls (available/isPressed/getX/getY) don't hammer the bus.
  bool readTouchData();
  // Re-initializes Wire1 after persistent I²C failures (sensor occasionally
  // gets the bus stuck if we read too fast or partially).
  void attemptRecovery();

  TwoWire *_wire;
  int _sda;
  int _scl;
  int _rst;
  int _irq;
  bool _initialized;

  uint16_t _x;
  uint16_t _y;
  uint8_t  _points;
  uint8_t  _gesture;
  unsigned long _lastReadMs;
  unsigned long _lastTouchMs;  // millis() of the last read that saw a real touch
  unsigned long _errCount;
  bool _wasPressed;          // edge-detection state for available()
  bool _releaseEdgePending;  // set true when a press→release transition is observed
};
