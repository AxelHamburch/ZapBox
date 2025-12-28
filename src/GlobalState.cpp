/**
 * @file GlobalState.cpp
 * @brief Global state variable definitions
 * 
 * Provides definitions for all global state structs declared in GlobalState.h
 */

#include "GlobalState.h"

// WiFi & Network Configuration
WifiConfig wifiConfig;

// Display & Theme Settings
DisplayConfig displayConfig;

// Lightning Payment Configuration
LightningConfig lightningConfig;

// Power Management & Screensaver
PowerConfig powerConfig;

// External LED Button State
ExternalButtonState externalButtonState;

// Special Modes & Waveform Control
SpecialModeConfig specialModeConfig;

// Multi-Channel Control & BTC Ticker
MultiChannelConfig multiChannelConfig;

// Bitcoin Data & Ticker
BitcoinData bitcoinData;

// Multi-Product Labels
ProductLabels productLabels;

// Network Error Tracking & Status
NetworkStatus networkStatus;

// Touch Input State
TouchState touchState;

// Product Selection & Timeout Tracking
ProductSelectionState productSelectionState;

// Payment Status
PaymentStatus paymentStatus;

// Activity Tracking for Screensaver
ActivityTracking activityTracking;

// Utility Constants
const char* BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
