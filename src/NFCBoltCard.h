#pragma once

/**
 * @file NFCBoltCard.h
 * @brief NFC Bolt Card reader module for ZapBox (PN532 + NTAG424 DNA)
 *
 * Enables Bolt Card NFC payments on the ZapBox (T-Display-S3).
 *
 * Hardware requirements:
 *   - PN532 NFC module connected via I2C (shared bus with Touch controller)
 *     SDA = GPIO 18, SCL = GPIO 17  (same as TouchCST816S)
 *     I²C address = 0x24
 *   - NFC IRQ output connected to GPIO 1 (PIN_NFC_IRQ)
 *     IRQ is active LOW, pulled high internally by the library
 *
 * The module is gated by the ENABLE_NFC preprocessor flag.
 * Set -DENABLE_NFC=1 in platformio.ini [env:lilygo-t-display-s3] to activate.
 * Default is 0 (disabled) – zero code compiled in, zero impact on normal operation.
 *
 * Architecture:
 *   - Separate FreeRTOS task on Core 0, priority 1 (equal to Task1 button handler)
 *   - Library uses digitalRead(GPIO1) for IRQ-based card detection – no I2C polling
 *     during the wait phase, keeping the shared I2C bus mostly free for Touch
 *   - Only on card detection the I2C bus is briefly used (~5 ms) for NTAG424 reads
 *   - ESP32 Arduino Wire library serializes concurrent I2C access internally via mutex
 *
 * Payment flow (Bolt Card / LNURLW):
 *   1. User taps a Bolt Card (NTAG424 DNA featuring lnurlw://)
 *   2. ZapBox reads the card and extracts the LNURLW
 *   3. ZapBox sends WebSocket event to server:
 *        {"event":"lnurlw", "lnurlw":"lnurlw://...", "pin":<activePin>}
 *   4. Server creates a Lightning invoice for <pin>, calls LNURLW callback
 *   5. Card wallet pays the invoice → server sends normal "paid" WS event
 *
 * Server side:
 *   The bitcoinswitch_extension WebSocket handler must be extended to handle
 *   the "lnurlw" event (analogous to partytap_extension/views_ws.py).
 *   See Network.cpp nfcLnurlwReceived() for the exact WS message format.
 */

#ifdef ENABLE_NFC

#include <Arduino.h>

/**
 * @brief Initialize the NFC Bolt Card reader.
 *
 * Must be called AFTER Touch controller is initialized (Wire.begin() must have
 * already been called with GPIO17/18, which TouchCST816S does in its begin()).
 *
 * Creates the NFC FreeRTOS task. If PN532 hardware is not found, initialization
 * fails silently and returns false – normal operation is unaffected.
 *
 * @return true if PN532 found and task started, false otherwise
 */
bool nfcBoltCardInit();

/**
 * @brief Stop the NFC Bolt Card reader and release PN532.
 *
 * Deletes the FreeRTOS task and frees the PN532 instance.
 * Used by "both" mode to switch between card reading and card emulation.
 */
void nfcBoltCardStop();

/**
 * @brief Callback invoked by the NFC task when a valid LNURLW is read.
 *
 * Implemented in Network.cpp. Sends the appropriate WebSocket event to the
 * LNbits bitcoinswitch_extension server so it can perform the LNURLW withdraw.
 *
 * @param lnurlw Full LNURLW string, e.g. "lnurlw://lnbits.server/withdraw/..."
 */
void nfcLnurlwReceived(const String &lnurlw);

#endif // ENABLE_NFC
