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

/**
 * Periodically update switch labels from server.
 * Retries if initial load failed or when update interval passes.
 */
void updateSwitchLabels();

#endif // API_H
