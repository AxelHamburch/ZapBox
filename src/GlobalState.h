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
  String deepSleep = "off";     // Deep sleep mode: "off", "freeze", "light"
  String activationTime = "5";  // Screensaver activation time in minutes
  unsigned long activationTimeoutMs = 0;  // Screensaver timeout in milliseconds
  String deepSleepTime = "30";  // Deep sleep activation time in minutes
  unsigned long deepSleepTimeoutMs = 0;   // Deep sleep timeout in milliseconds
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
  bool enabled = false; // Whether external button is enabled (replaces onboard buttons)
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
  String mode = "off";        // "off", "duo", "quattro", "servo"
  String btcTickerMode = "off"; // "off", "always", "selecting"
  volatile bool btcTickerActive = false; // volatile for multi-threaded WebSocket access
  volatile int currentProduct = -1;    // -1 = selection screen, 1-4 = product number (volatile for multi-context access)
};

extern MultiChannelConfig multiChannelConfig;

// ============================================================================
// VENDING MACHINE LIGHT BARRIER (GPIO 2)
// ============================================================================

struct LightBarrierConfig {
  String mode = "no";  // "no" (disabled), "yes" (stop action on trigger)
  bool enabled = false; // Parsed boolean for easy checking
  unsigned long minActionTime = 2000; // Minimum 2 seconds before light barrier can stop action
};

extern LightBarrierConfig lightBarrierConfig;

// ============================================================================
// SERVO MOTOR CONFIGURATION (Servo multi-channel mode)
// ============================================================================

struct ServoConfig {
  // Servo 1 — positional 0-180° on Pin 13 (independent channel)
  int servo1Start = 0;       // Start angle 0-180° (rest position)
  int servo1End = 0;         // End angle 0-180° (active position)
  int servo1Duration = 0;    // Sweep duration ms (0 = max speed)
  // Servo 2 — continuous rotation (360°/multiturn) on Pin 10 (independent channel)
  int servo2Speed = 0;       // Speed value 0-180 (90=stop, <90=CCW, >90=CW)
  int servo2Duration = 0;    // Spin duration ms
  // Relay activation in servo mode
  String relayMode = "one-for-all"; // "one-for-all" (default), "relay1", "both", "off"
  // Helper: servo is active if parameters are non-zero
  bool servo1Active() const { return servo1Start != 0 || servo1End != 0; }
  bool servo2Active() const { return servo2Speed != 0 && servo2Speed != 90; }
  bool relay1Active() const { return relayMode != "off"; }
  bool relay2Active() const { return relayMode == "both"; }
  // One For All mode: Pin 12 triggers all channels simultaneously
  bool oneForAll() const { return relayMode == "one-for-all"; }
  // Count active channels in servo mode
  // In One For All mode always 1 (only Pin 12 QR is shown)
  int activeChannelCount() const {
    if (oneForAll()) return 1;
    int count = 0;
    if (relay1Active()) count++;
    if (servo1Active()) count++;
    if (servo2Active()) count++;
    if (relay2Active()) count++;
    return count;
  }
  // Map product number (1-based) to pin, skipping inactive channels
  // In One For All mode always returns Pin 12
  int productToPin(int product) const {
    if (oneForAll()) return 12;
    const int pins[] = {12, 13, 10, 11};
    const bool active[] = {relay1Active(), servo1Active(), servo2Active(), relay2Active()};
    int count = 0;
    for (int i = 0; i < 4; i++) {
      if (active[i]) {
        count++;
        if (count == product) return pins[i];
      }
    }
    return 12; // fallback
  }
};

extern ServoConfig servoConfig;

// ============================================================================
// CHANNEL 4 AMBIENT LIGHT (GPIO 11 on T-Display-S3 only)
// ============================================================================

struct Channel4AmbientConfig {
  String mode = "normal";    // "normal" (default 4-channel behavior), "ambient" (backlight sync)
  bool enabled = false;      // Parsed boolean: true if ambient mode is active
};

extern Channel4AmbientConfig channel4AmbientConfig;

// ============================================================================
// EXTENSION / API PATH CONFIGURATION
// ============================================================================

struct ExtensionConfig {
  // API path base: "bitcoinswitch" (classic) or "zapbox" (zapbox_extension)
  // Controls which LNbits extension the device communicates with.
  // Set to "zapbox" when zapbox_extension is installed on the server.
  String apiPath = "bitcoinswitch";
  // NFC state – written by the NFC task (Core 0), read by loop() (Core 1).
  // Must be volatile to prevent the compiler from caching stale values in registers.
  volatile bool nfcPaymentPending = false;           // True while waiting for LNURLW invoice settlement
  volatile unsigned long nfcPaymentPendingStart = 0; // Timestamp when NFC payment was initiated (for timeout)
  volatile bool nfcPaymentFailed = false;            // True when HTTP POST to server failed – triggers NO LUCK screen
  volatile bool nfcExtensionMismatch = false;         // True when NFC tap detected but extension is not zapbox
  char nfcErrorDetail[128] = "";                     // Detail message from last NFC HTTP error (shown after NO LUCK screen)
};

extern ExtensionConfig extensionConfig;

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
  // Labels stored in array by RELAY_CHANNEL_PINS order (see PinConfig.h):
  //   index 0=GPIO12(CH01), 1=GPIO13(CH02)
  //   index 2=CH03 (GPIO10 on T-Display-S3 / GPIO14 on esp32dev)
  //   index 3=CH04 (GPIO11 on T-Display-S3 / GPIO16 on esp32dev)
  //   index 4=GPIO19(CH05), 5=GPIO22(CH06), 6=GPIO23(CH07), 7=GPIO25(CH08)
  //   index 8=GPIO26(CH09), 9=GPIO27(CH10), 10=GPIO32(CH11), 11=GPIO33(CH12)
  //   indices 4-11 only on esp32dev
  String labels[12] = {"", "", "", "", "", "", "", "", "", "", "", ""};
  int durations[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Action time per pin in ms (0 = not configured)
  unsigned long lastUpdate = 0;
};

extern ProductLabels productLabels;

// Helper function to convert GPIO pin to ProductLabels array index.
// Order matches RELAY_CHANNEL_PINS in PinConfig.h.
// CH03/CH04 GPIOs differ per board (see PinConfig.h for details).
//   GPIO 12 → 0 (CH01)   GPIO 13 → 1 (CH02)
//   T-Display-S3: GPIO 10 → 2 (CH03), GPIO 11 → 3 (CH04)
//   esp32dev:     GPIO 14 → 2 (CH03), GPIO 16 → 3 (CH04)
//   GPIO 19 → 4 (CH05)   GPIO 22 → 5 (CH06)
//   GPIO 23 → 6 (CH07)   GPIO 25 → 7 (CH08)
//   GPIO 26 → 8 (CH09)   GPIO 27 → 9 (CH10)
//   GPIO 32 → 10 (CH11)  GPIO 33 → 11 (CH12)  ← esp32dev only
// Returns -1 if pin is not a relay channel.
inline int getPinIndex(int pin) {
  switch (pin) {
    case 12: return 0;
    case 13: return 1;
#if ENABLE_DISPLAY
    case 10: return 2;  // CH03 on T-Display-S3 (ESP32-S3)
    case 11: return 3;  // CH04 on T-Display-S3 (ESP32-S3)
#else
    case 14: return 2;  // CH03 on esp32dev (GPIO10/11 = internal flash!)
    case 16: return 3;  // CH04 on esp32dev
#endif
    case 19: return 4;
    case 22: return 5;
    case 23: return 6;
    case 25: return 7;
    case 26: return 8;
    case 27: return 9;
    case 32: return 10;
    case 33: return 11;
    default: return -1;
  }
}

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
// PAYMENT QUEUE
// ============================================================================

#include <queue>

struct PaymentQueueItem {
  String payload;           // Payment payload (e.g., "12-5000" or JSON)
  unsigned long timestamp;  // When this payment was received
};

struct PaymentQueue {
  std::queue<PaymentQueueItem> queue;  // Queue of pending payments
  bool processing = false;              // Flag to indicate payment is being processed
  
  // Add a payment to the queue
  void enqueue(const String& payload) {
    PaymentQueueItem item;
    item.payload = payload;
    item.timestamp = millis();
    queue.push(item);
    Serial.printf("[QUEUE] Payment enqueued. Queue size: %d\n", queue.size());
  }
  
  // Get next payment from queue (returns empty string if queue is empty)
  String dequeue() {
    if (queue.empty()) {
      return "";
    }
    PaymentQueueItem item = queue.front();
    queue.pop();
    Serial.printf("[QUEUE] Payment dequeued. Queue size: %d\n", queue.size());
    return item.payload;
  }
  
  // Check if queue has pending payments
  bool hasPending() const {
    return !queue.empty();
  }
  
  // Get current queue size
  size_t size() const {
    return queue.size();
  }
};

extern PaymentQueue paymentQueue;

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
