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
 * expected action ("auth" or "register") via actionOut. The HTTP status code
 * is returned via httpOut when given (e.g. 403 = Identities disabled
 * server-side). Returns false on error.
 */
bool requestAuthLnurl(String *actionOut = nullptr, int *httpOut = nullptr);

/**
 * Ring-Login: verify NTAG 424 DNA tap via zapbox_extension NFC-auth endpoint.
 * Calls GET /<apiPath>/api/v1/nfc/auth/<deviceId>?external_id=&p=&c=[&pin=]
 * Returns true when the server responds {"status":"OK"} (relay already triggered
 * by the extension via WebSocket). Sets errorOut to a short error description
 * on failure (e.g. "Unknown identity", "Wrong PIN").
 */
bool requestNfcAuth(const String &externalId, const String &p, const String &c,
                    const String &pin, String *errorOut = nullptr);

/**
 * Ring-Login teach: enrol an NTAG 424 card via the zapbox_extension teach endpoint.
 * Requires an open teach session on the server.
 * Calls GET /<apiPath>/api/v1/nfc/teach/<deviceId>?external_id=&p=&c=
 * Returns true on success.
 */
bool requestNfcTeach(const String &externalId, const String &p, const String &c);

#endif // API_H
