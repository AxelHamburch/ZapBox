#pragma once

/**
 * @file NFCCardEmulation.h
 * @brief NFC Card Emulation module for ZapBox (PN532 Target Mode)
 *
 * Enables the PN532 to act as a passive NFC tag (ISO 14443-3A / Type 4 Tag),
 * allowing smartphones to read the current LNURLp payment link via NFC tap.
 *
 * The PN532 is put into Target Mode using the TgInitAsTarget command.
 * When a phone taps, the module responds to NFC Forum Type 4 Tag APDUs
 * and serves an NDEF URI record containing the same LNURLp link that is
 * shown on the QR code display.
 *
 * Hardware requirements:
 *   - Same PN532 module as Bolt Card reader (shared I2C bus)
 *   - No additional wiring needed
 *
 * The module is gated by the ENABLE_NFC preprocessor flag.
 *
 * Architecture:
 *   - Separate FreeRTOS task on Core 0 (same as Bolt Card task)
 *   - Uses Adafruit_PN532 AsTarget() / getDataTarget() / setDataTarget()
 *   - LNURLp payload is read from lightningConfig.lightning (global)
 *   - Thread-safe: only reads global state, does not call display functions
 *
 * Modes:
 *   "emulation" — Always active, serves LNURLp to phones
 *   "both"      — Active when QR screen is shown, paused for Bolt Card screen
 */

#ifdef ENABLE_NFC

#include <Arduino.h>

/**
 * @brief Initialize NFC Card Emulation.
 *
 * Creates PN532 instance (if not already created by Bolt Card module),
 * starts FreeRTOS task for Target Mode loop.
 *
 * Must be called AFTER Wire.begin() (touch controller or headless init).
 *
 * @return true if PN532 found and emulation task started, false otherwise
 */
bool nfcCardEmulationInit();

/**
 * @brief Stop NFC Card Emulation and release PN532.
 *
 * Deletes the FreeRTOS task and resets PN532 to allow mode switching
 * (e.g., from emulation to Bolt Card reader in "both" mode).
 */
void nfcCardEmulationStop();

/**
 * @brief Check if card emulation is currently running.
 * @return true if emulation task is active
 */
bool nfcCardEmulationIsActive();

#endif // ENABLE_NFC
