#pragma once

/**
 * @file NFCNT3H2111.h
 * @brief NT3H2111 NTAG I²C NFC tag driver for ZapBox (emulation mode)
 *
 * Writes the current LNURLp into the NT3H2111's NDEF user memory over I²C,
 * so any smartphone can read it with a passive NFC tap — no PN532 needed.
 *
 * The NT3H2111 (MikroE NFC Tag 2 Click) is a genuine NTAG I²C chip that
 * presents as an NFC Forum Type 2 Tag (ISO 14443-3A, SAK = 0x00).
 * Unlike PN532 Target Mode emulation it is compatible with all Lightning
 * wallets including Wallet of Satoshi (which rejects the PN532 Type 4 Tag).
 *
 * Hardware wiring (T-Display-S3):
 *   VCC → 3.3 V
 *   GND → GND
 *   SDA → GPIO 18  (shared I²C bus, address 0x55)
 *   SCL → GPIO 17
 *   FD  → not connected  (field-detect output, open-drain — not required)
 *
 * The module is gated by the ENABLE_NFC build flag and requires
 * nfcConfig.mode == "emulation" at runtime.
 *
 * There is no FreeRTOS task — the NT3H2111 holds the NDEF data autonomously
 * in its internal memory after one I²C write.  nfcNT3H2111UpdateIfChanged()
 * re-writes only when lightningConfig.lightning changes (e.g. product switch).
 */

#ifdef ENABLE_NFC

#include <Arduino.h>

/**
 * @brief Detect NT3H2111 on I²C, verify the Capability Container, and
 *        write the current LNURLp as an NDEF URI record.
 * @return true  if the chip was found and the initial NDEF was written.
 * @return false if the chip could not be reached at I²C address 0x55.
 */
bool nfcNT3H2111Init();

/**
 * @brief Release internal state (no hardware action required).
 *        Call when switching away from emulation mode.
 */
void nfcNT3H2111Stop();

/**
 * @brief Re-write the NDEF if lightningConfig.lightning has changed since
 *        the last write.  No-op (and very cheap) when nothing has changed.
 *        Call from the main loop on every iteration.
 */
void nfcNT3H2111UpdateIfChanged();

#endif // ENABLE_NFC
