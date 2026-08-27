#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Global I2C bus mutex.
// All I2C devices share one Wire bus (SDA=GPIO18, SCL=GPIO17):
//   PCF8574 IOExpander @ 0x20  (main loop)
//   PCF8575 IOExpander @ 0x21  (main loop, 16 channels — A0 to VDD)
//   MCP23017 IOExpander@ 0x22  (main loop, 16 channels — A1 to VDD)
//   PN532 NFC reader   @ 0x24  (NFCBoltCard FreeRTOS task)
//   CST816S touch      @ 0x15  (main loop, if present)
//
// Note: PCF8574, PCF8575 and MCP23017 all answer in the same 0x20-0x27 range,
// which the PN532 also reaches into at 0x24. No two expanders may be jumpered
// to the same address, and none of them to 0x24.
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

// ---- Module status summary -------------------------------------------------
//
// Each I2C driver reports its probe result once during init; the "ZapBox ready!"
// banner prints the collected results as a ✅/❌ list. No extra bus traffic —
// the information is already known from initialisation.

// Report the outcome of a module probe. addr = 7-bit I2C address.
void i2cReportModule(const char *name, uint8_t addr, bool present);

// Print one line per known module (✅ found / ❌ not found / – not configured).
void i2cPrintModuleSummary();
