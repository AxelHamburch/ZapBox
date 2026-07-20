#ifndef UI_H
#define UI_H

#include <Arduino.h>

// Wake device from screensaver/deep sleep modes
bool wakeFromPowerSavingMode();

// Check if device is ready to receive payments (for LED state)
bool isReadyForReceive();

// Update ready LED based on device state
void updateReadyLed();

// LED button output (PIN_LED_BUTTON_LED).
// All writes to the LED pin must go through these helpers: the pin is driven by
// PWM (analogWrite), and a raw digitalWrite would leave the PWM channel running.
void ledSetLevel(uint8_t level);   // 0 = off … 255 = full brightness
inline void ledSetOn(bool on) { ledSetLevel(on ? 255 : 0); }

// Redraw appropriate QR screen based on current mode/product
void redrawQRScreen();

// Decide and show initial screen after all connections are confirmed
void showInitialScreenAfterConnections();

// Handle screensaver and deep sleep checks inside the payment loop
void handlePowerSavingChecks();

#endif // UI_H
