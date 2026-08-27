#pragma once

/**
 * IOExpander.h — PCF8574 / PCF8575 / MCP23017 I2C GPIO expanders (relay channels only)
 *
 * PCF8574  @ 0x20: 8 relay output channels.
 * PCF8575  @ 0x21: 16 relay output channels.
 * MCP23017 @ 0x22: 16 relay output channels.
 * All share the existing I2C bus (SDA=GPIO18, SCL=GPIO17) with PN532 NFC and touch.
 *
 * All three chips answer in the same 0x20-0x27 address range and the PN532 sits at
 * 0x24, so the addresses must not overlap. Defaults:
 *   PCF8574  0x20 — A0/A1/A2 to GND
 *   PCF8575  0x21 — A0 to VDD, A1/A2 to GND       (0x42 in 8-bit notation)
 *   MCP23017 0x22 — A1 to VDD, A0/A2 to GND       (0x44 in 8-bit notation)
 * Each init function refuses to bring its chip up on a colliding address.
 *
 * Virtual pin mapping (LNbits → expander):
 *   PCF8574:  200 → P0   … 207 → P7
 *   PCF8575:  300 → P00  … 307 → P07, 308 → P10  … 315 → P17
 *   MCP23017: 400 → GPA0 … 407 → GPA7, 408 → GPB0 … 415 → GPB7
 *
 * Relay logic is configurable per expander via ioExpanderConfig.activeHigh /
 * ioExpander16Config.activeHigh / mcp23017Config.activeHigh:
 *   active-LOW  (default): LOW = relay on,  ports start HIGH
 *   active-HIGH:           HIGH = relay on, ports start LOW
 *
 * On the PCF857x, active-HIGH additionally avoids the brief relay flicker at
 * power-up, because the I2C glitch that can occur before init pulls the ports LOW.
 * It is however electrically marginal there: the quasi-bidirectional ports source
 * only ~100 µA in the HIGH state, which collapses under the load of a relay board
 * input. The MCP23017 has real push-pull outputs and no such limitation — and no
 * power-up flicker either, since its ports start as high-impedance inputs.
 *
 * Wire.begin() must have been called before any of the init functions.
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

// ---- MCP23017 (16 channels, virtual pins 400-415) --------------------------

// Initialize MCP23017 — call after Wire.begin() and after initIOExpander() /
// initIOExpander16(), so the address-collision check can see which of the other
// expanders actually came up.
void initIOExpanderMCP();

// Activate relay channel ch (0–15 = virtual pins 400–415).
void activateExpanderMCPChannel(int ch);

// Deactivate relay channel ch (0–15).
void deactivateExpanderMCPChannel(int ch);
