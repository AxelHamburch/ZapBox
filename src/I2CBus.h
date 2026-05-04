#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Global I2C bus mutex.
// All I2C devices share one Wire bus (SDA=GPIO18, SCL=GPIO17):
//   PCF8574 IOExpander @ 0x20  (main loop)
//   PN532 NFC reader   @ 0x24  (NFCBoltCard FreeRTOS task)
//   CST816S touch      @ 0x15  (main loop, if present)
//
// Take this mutex before any Wire transaction and release immediately after.
// Do NOT hold it during delays, waitForCardRemoval(), or readPassiveTargetID().

extern SemaphoreHandle_t i2cBusMutex;

// Call once before Wire.begin() (in setup(), before touch/NFC init).
void i2cBusInit();

// Take the mutex. Blocks up to 500 ms. Returns true on success.
bool i2cTake();

// Release the mutex.
void i2cGive();
