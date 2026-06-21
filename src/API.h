#ifndef API_H
#define API_H

#include <Arduino.h>

/**
 * API.h - Server Communication Module
 * 
 * This module handles all external API calls:
 * - Switch label fetching from LNbits server
 * - Bitcoin price and block height from CoinGecko/Mempool.space
 * - Periodic updates for labels and BTC data
 */

/**
 * Fetch switch labels and configuration from LNbits server.
 * Updates productLabels global struct with pin-specific labels.
 * Also extracts and updates currency configuration.
 */
void fetchSwitchLabels();

#if ENABLE_BITCOIN_DATA
/**
 * Fetch Bitcoin price and block height from external APIs.
 * - Price from CoinGecko (uses configured currency)
 * - Block height from mempool.space
 * Updates bitcoinData global struct.
 */
void fetchBitcoinData();

/**
 * Periodically update Bitcoin ticker display.
 * Only updates if ticker is active and update interval has passed.
 * Calls fetchBitcoinData() and refreshes display.
 */
void updateBitcoinTicker();
#endif // ENABLE_BITCOIN_DATA

/**
 * Periodically update switch labels from server.
 * Retries if initial load failed or when update interval passes.
 */
void updateSwitchLabels();

/**
 * Mini-PoS: request a Lightning invoice from the zapbox_extension for the
 * given normalized amount string (e.g. "5.00"). On success fills
 * miniPosState (paymentHash, amountLine, invoicePending) and updates the
 * lightning QR buffer with the BOLT11 payment request.
 * On failure sets miniPosState.infoMsg and returns false.
 */
bool requestMiniPosInvoice(const String &amountStr);

/**
 * Mini-PoS: fetch amount of the last settled payment ("Last Pay" button).
 * Returns true and fills amountOut (e.g. "23.50") when a paid entry exists.
 */
bool fetchMiniPosLastPay(String &amountOut);

/**
 * Authy (LNURL-auth): request a fresh auth LNURL (single-use k1) from the
 * zapbox_extension and place it in the QR/NFC buffer. Optionally returns the
 * expected action ("auth" or "register") via actionOut. Returns false on error.
 */
bool requestAuthLnurl(String *actionOut = nullptr);

#endif // API_H
