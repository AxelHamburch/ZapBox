#pragma once

/**
 * IOExpander.h — PCF8574 I2C GPIO expander (relay channels only)
 *
 * Adds 8 relay output channels via a PCF8574 at I2C address 0x20.
 * Shares the existing I2C bus (SDA=GPIO18, SCL=GPIO17) with PN532 NFC and touch.
 *
 * Virtual pin mapping (LNbits → PCF8574):
 *   200 → P0,  201 → P1,  202 → P2,  203 → P3
 *   204 → P4,  205 → P5,  206 → P6,  207 → P7
 *
 * Relay logic: active LOW (LOW = relay on, HIGH = relay off).
 * Wire.begin() must have been called before initIOExpander().
 */

// Initialize PCF8574 — call after Wire.begin(). Does nothing if expander is disabled.
void initIOExpander();

// Activate relay channel ch (0–7 = virtual pins 200–207). Pulls PCF8574 Px LOW.
void activateExpanderChannel(int ch);

// Deactivate relay channel ch (0–7). Returns PCF8574 Px HIGH.
void deactivateExpanderChannel(int ch);
