#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Global I2C bus mutex.
// All I2C devices share one Wire bus (SDA=GPIO18, SCL=GPIO17):
//   PCF8574 IOExpander @ 0x20  (main loop)
//   PCF8575 IOExpander @ 0x21  (main loop, 16 channels — A0 to VDD)
//   PN532 NFC reader   @ 0x24  (NFCBoltCard FreeRTOS task)
//   CST816S touch      @ 0x15  (main loop, if present)
//
// Note: PCF8574 and PCF8575 share the same 0x20-0x27 address range, so the
// PCF8575 must not be jumpered to 0x20 while a PCF8574 is in use.
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
