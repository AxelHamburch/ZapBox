#pragma once

/**
 * IOExpander.h — PCF8574 / PCF8575 I2C GPIO expanders (relay channels only)
 *
 * PCF8574 @ 0x20: 8 relay output channels.
 * PCF8575 @ 0x21: 16 relay output channels.
 * Both share the existing I2C bus (SDA=GPIO18, SCL=GPIO17) with PN532 NFC and touch.
 *
 * Both chip families answer in the same 0x20-0x27 address range, so the PCF8575 must
 * NOT sit on 0x20 while a PCF8574 is in use. 0x21 = A0 to VDD, A1/A2 to GND
 * (0x42 in the 8-bit notation used by many datasheets).
 *
 * Virtual pin mapping (LNbits → expander):
 *   PCF8574:  200 → P0  … 207 → P7
 *   PCF8575:  300 → P00 … 307 → P07, 308 → P10 … 315 → P17
 *
 * Relay logic is configurable per expander via ioExpanderConfig.activeHigh /
 * ioExpander16Config.activeHigh:
 *   active-LOW  (default): LOW = relay on,  ports start HIGH
 *   active-HIGH:           HIGH = relay on, ports start LOW
 * Active-HIGH additionally avoids the brief relay flicker at power-up, because the
 * I2C glitch that can occur before init pulls the ports LOW.
 *
 * Wire.begin() must have been called before initIOExpander() / initIOExpander16().
 */

// ---- PCF8574 (8 channels, virtual pins 200-207) ----------------------------

// Initialize PCF8574 — call after Wire.begin(). Does nothing if expander is disabled.
void initIOExpander();

// Activate relay channel ch (0–7 = virtual pins 200–207).
void activateExpanderChannel(int ch);

// Deactivate relay channel ch (0–7).
void deactivateExpanderChannel(int ch);

// ---- PCF8575 (16 channels, virtual pins 300-315) ---------------------------

// Initialize PCF8575 — call after Wire.begin() and after initIOExpander(), so the
// address-collision check can see whether a PCF8574 is already active.
void initIOExpander16();

// Activate relay channel ch (0–15 = virtual pins 300–315).
void activateExpander16Channel(int ch);

// Deactivate relay channel ch (0–15).
void deactivateExpander16Channel(int ch);
