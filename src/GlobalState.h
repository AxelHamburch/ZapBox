#ifndef GLOBAL_STATE_H
#define GLOBAL_STATE_H

#include <Arduino.h>

/**
 * @file GlobalState.h
 * @brief Centralized global state management with organized data structures
 * 
 * Organizes all global variables into semantic groups to improve code clarity,
 * maintainability, and reduce the cognitive load of tracking scattered state.
 */

// ============================================================================
// WIFI & NETWORK CONFIGURATION
// ============================================================================

struct WifiConfig {
  String ssid = "";
  String wifiPassword = "";
  String switchStr = "";
  static constexpr const char* lightningPrefix = "lightning:";
};

extern WifiConfig wifiConfig;

// ============================================================================
// DISPLAY & THEME SETTINGS
// ============================================================================

struct DisplayConfig {
  String orientation = "h";  // "h" for horizontal, "v" for vertical
  String theme = "black-white";
};

extern DisplayConfig displayConfig;

// ============================================================================
// LIGHTNING PAYMENT CONFIGURATION
// ============================================================================

struct LightningConfig {
  char lightning[300] = "";     // Main Lightning URL/QR code
  String thresholdKey = "";     // Optional threshold mode key
  String thresholdAmount = "";  // Threshold amount in sats
  String thresholdPin = "";     // GPIO pin for threshold
  String thresholdTime = "";    // Threshold timeout
  String thresholdLnurl = "";   // Alternative LNURL for threshold mode
};

extern LightningConfig lightningConfig;

// ============================================================================
// POWER MANAGEMENT & SCREENSAVER
// ============================================================================

struct PowerConfig {
  String screensaver = "off";   // Screensaver mode: "off", "on", etc.
  String deepSleep = "off";     // Deep sleep mode: "off", "on", etc.
  String activationTime = "5";  // Activation time in minutes
  unsigned long activationTimeoutMs = 0;  // Calculated timeout in milliseconds
  unsigned long lastWakeUpTime = 0;  // Track when device woke up from screensaver
};

extern PowerConfig powerConfig;

// ============================================================================
// EXTERNAL LED BUTTON STATE & CONFIGURATION
// ============================================================================

struct ExternalButtonState {
  bool pressed = false;
  bool holdActionFired = false;
  uint8_t clickCount = 0;
  unsigned long sequenceStart = 0;
  unsigned long pressStartTime = 0;
  unsigned long lastChange = 0;
};

struct ExternalButtonConfig {
  static constexpr unsigned long DEBOUNCE_MS = 50;
  static constexpr unsigned long TRIPLE_WINDOW_MS = 2000;
  static constexpr unsigned long HELP_HOLD_MS = 2000;
  static constexpr unsigned long CONFIG_HOLD_MS = 3000;
  static constexpr unsigned long CONFIG_EXIT_GUARD_MS = 2000; // Minimum time before button/touch can exit config
};

extern ExternalButtonState externalButtonState;

// ============================================================================
// SPECIAL MODES & WAVEFORM CONTROL
// ============================================================================

struct SpecialModeConfig {
  String mode = "standard";  // "standard", "frequency", "brightness", etc.
  float frequency = 1.0;     // Frequency multiplier for waveform mode
  float dutyCycleRatio = 1.0; // Duty cycle for PWM modes
};

extern SpecialModeConfig specialModeConfig;

// ============================================================================
// MULTI-CHANNEL CONTROL & BTC TICKER
// ============================================================================

struct MultiChannelConfig {
  String mode = "off";        // "off", "duo", "quattro"
  String btcTickerMode = "off"; // "off", "always", "selecting"
  volatile bool btcTickerActive = false; // volatile for multi-threaded WebSocket access
  volatile int currentProduct = -1;    // -1 = selection screen, 1-4 = product number (volatile for multi-context access)
};

extern MultiChannelConfig multiChannelConfig;

// ============================================================================
// BITCOIN DATA & TICKER
// ============================================================================

struct BitcoinData {
  String price = "Loading...";      // Current BTC price
  String blockHigh = "...";          // Block height or other metric
  unsigned long lastUpdate = 0;      // Last update timestamp
};

extern BitcoinData bitcoinData;

// ============================================================================
// MULTI-PRODUCT LABELS
// ============================================================================

struct ProductLabels {
  String label12 = "";  // Label for pin 12 / product 1
  String label13 = "";  // Label for pin 13 / product 2
  String label10 = "";  // Label for pin 10 / product 3
  String label11 = "";  // Label for pin 11 / product 4
  unsigned long lastUpdate = 0;
};

extern ProductLabels productLabels;

// ============================================================================
// NETWORK ERROR TRACKING & STATUS
// ============================================================================

struct NetworkStatus {
  struct ErrorCounts {
    uint8_t wifi = 0;
    uint8_t internet = 0;
    uint8_t server = 0;
    uint8_t websocket = 0;
  } errors;

  struct Confirmation {
    bool wifi = false;
    bool internet = false;
    bool server = false;
    bool websocket = false;
  } confirmed;

  unsigned long lastPingTime = 0;
  unsigned long lastPongTime = 0;
  bool waitingForPong = false;
};

extern NetworkStatus networkStatus;

// ============================================================================
// TOUCH INPUT STATE
// ============================================================================

struct TouchState {
  bool available = false;
  bool pressed = false;
  unsigned long lastTime = 0;
  unsigned long pressStartTime = 0;
  uint8_t clickCount = 0;
};

extern TouchState touchState;

// ============================================================================
// PRODUCT SELECTION & TIMEOUT TRACKING
// ============================================================================

struct ProductSelectionState {
  unsigned long showTime = 0;  // Timestamp when product selection started
};

extern ProductSelectionState productSelectionState;

// ============================================================================
// PAYMENT STATUS
// ============================================================================

struct PaymentStatus {
  bool paid = false;  // Whether a payment has been received
};

extern PaymentStatus paymentStatus;

// ============================================================================
// ACTIVITY TRACKING FOR SCREENSAVER
// ============================================================================

struct ActivityTracking {
  unsigned long lastActivityTime = 0;  // Last user activity (button/touch)
};

extern ActivityTracking activityTracking;

// ============================================================================
// UTILITY CONSTANTS
// ============================================================================

extern const char* BECH32_CHARSET;

#endif // GLOBAL_STATE_H
