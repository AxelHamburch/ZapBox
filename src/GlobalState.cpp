/**
 * @file GlobalState.cpp
 * @brief Global state variable definitions
 * 
 * Provides definitions for all global state structs declared in GlobalState.h
 */

#include "GlobalState.h"
#include "Log.h"

// Log suppression flag (used during config mode to keep serial clean)
namespace Log { bool suppressed = false; }

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

// Vending Machine Light Barrier
LightBarrierConfig lightBarrierConfig;

// Channel 4 Ambient Light
Channel4AmbientConfig channel4AmbientConfig;

// C3 Flex Channel Configuration (GPIO6/GPIO7 — ESP32-C3-21-1 only)
#ifdef BOARD_ESP32C3_21_1
C3FlexChannelConfig c3FlexConfig;
#endif

// Touch 3.5 Ambient Light Configuration (JC3248W535C only)
#ifdef BOARD_JC3248W535C
T35AmbientConfig t35AmbientConfig;
#endif

// Servo Motor Configuration
ServoConfig servoConfig;

// Extension / API Path Configuration
ExtensionConfig extensionConfig;

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

// Payment Queue
PaymentQueue paymentQueue;

// Activity Tracking for Screensaver
ActivityTracking activityTracking;

// I/O Expander Configuration (PCF8574)
IOExpanderConfig ioExpanderConfig;

// NFC Mode Configuration
NfcConfig nfcConfig;

// Utility Constants
const char* BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

// PIN Pad State
PinPadState pinPadState;

// Mini-PoS Mode (Touch 3.5)
MiniPosConfig miniPosConfig;
MiniPosState miniPosState;
