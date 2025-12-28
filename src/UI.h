#ifndef UI_H
#define UI_H

#include <Arduino.h>

// Wake device from screensaver/deep sleep modes
bool wakeFromPowerSavingMode();

// Check if device is ready to receive payments (for LED state)
bool isReadyForReceive();

// Update ready LED based on device state
void updateReadyLed();

// Redraw appropriate QR screen based on current mode/product
void redrawQRScreen();

#endif // UI_H
