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
  char lightning[400] = "";     // Main Lightning URL/QR code (BOLT11 invoices in Mini-PoS mode can exceed 300 chars)
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
// VENDING MACHINE SENSOR INPUTS
// T-Display-S3: Single sensor on GPIO 2
// ESP32 Dev Headless: Two sensors on GPIO 22 and GPIO 23
// ============================================================================

struct LightBarrierConfig {
  // Sensor 1 — GPIO 2 (T-Display-S3) or GPIO 22 (headless)
  String mode = "no";  // "no" (disabled), "yes" (stop action on trigger), "monitor" (block next payment), "level" (level monitoring), "relay" (relay output synced with pin 12)
  bool enabled = false;         // true when mode == "yes"
  bool monitoring = false;      // true when mode == "monitor"
  bool levelMonitoring = false; // true when mode == "level"
  bool relayOutput = false;     // true when mode == "relay" (headless: GPIO acts as relay output, synced with pin 12)
  bool blocked = false;         // runtime: product output currently blocked (monitor mode)
  bool binEmpty = false;        // runtime: supply bin is empty (level monitoring mode)
  unsigned long minActionTime = 2000; // Minimum 2 seconds before light barrier can stop action

  // Sensor 2 — GPIO 23 (headless only)
  String mode2 = "no";
  bool enabled2 = false;
  bool monitoring2 = false;
  bool levelMonitoring2 = false;
  bool relayOutput2 = false;    // true when mode2 == "relay"
  bool blocked2 = false;
  bool binEmpty2 = false;

  bool isActive() const { return enabled || monitoring || levelMonitoring; }
  bool isActive2() const { return enabled2 || monitoring2 || levelMonitoring2; }
  bool isAnyActive() const { return isActive() || isActive2(); }
  // True when any sensor condition blocks payments
  bool isAnyBlocking() const { return blocked || blocked2 || binEmpty || binEmpty2; }
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
  // Pin 13 / Pin 10 configured as external relay (not servo) in servo mode
  bool pin13IsRelay = false;
  bool pin10IsRelay = false;
  // Helper: channel is active if servo params are non-zero OR pin is in relay mode
  bool servo1Active() const { return servo1Start != 0 || servo1End != 0 || pin13IsRelay; }
  bool servo2Active() const { return (servo2Speed != 0 && servo2Speed != 90) || pin10IsRelay; }
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
// C3 FLEX CHANNEL CONFIGURATION (ESP32-C3-21-1 only)
// GPIO6 (PIN_FLEX_CH01) and GPIO7 (PIN_FLEX_CH02) can each be independently
// configured as secondary actuator or sensor inputs.
//   relay    — secondary relay output, fires together with GPIO4
//   servo180 — 180° positional servo (0°–180°)
//   servo360 — 360° continuous servo (speed + duration)
//   yes      — sensor: stop relay action when triggered (INPUT_PULLUP, active LOW)
//   monitor  — sensor: block next payment until path is cleared
//   level    — sensor: block payments when bin is empty (HIGH = empty / no product)
// ============================================================================
#ifdef BOARD_ESP32C3_21_1
struct C3FlexChannelConfig {
  // ── GPIO6 (flex channel 1) ──────────────────────────────────────────
  String gpio6Mode = "no";
  bool gpio6Relay        = false;
  bool gpio6Servo180     = false;
  bool gpio6Servo360     = false;
  bool gpio6SensorStop   = false;   // mode == "yes"
  bool gpio6SensorMonitor = false;  // mode == "monitor"
  bool gpio6SensorLevel  = false;   // mode == "level"
  // Servo params — GPIO6 as 180° servo
  int gpio6S180Start    = 0;
  int gpio6S180End      = 0;
  int gpio6S180Duration = 0;
  // Servo params — GPIO6 as 360° servo
  int gpio6S360Speed    = 0;
  int gpio6S360Duration = 0;
  // Runtime sensor state — GPIO6
  bool gpio6Blocked  = false;   // monitor mode: product blocking output
  bool gpio6BinEmpty = false;   // level mode: supply bin empty

  // ── GPIO7 (flex channel 2) ──────────────────────────────────────────
  String gpio7Mode = "no";
  bool gpio7Relay        = false;
  bool gpio7Servo180     = false;
  bool gpio7Servo360     = false;
  bool gpio7SensorStop   = false;
  bool gpio7SensorMonitor = false;
  bool gpio7SensorLevel  = false;
  int gpio7S180Start    = 0;
  int gpio7S180End      = 0;
  int gpio7S180Duration = 0;
  int gpio7S360Speed    = 0;
  int gpio7S360Duration = 0;
  bool gpio7Blocked  = false;
  bool gpio7BinEmpty = false;

  // ── Helpers ─────────────────────────────────────────────────────────
  bool isAnyBlocking() const { return gpio6Blocked || gpio6BinEmpty || gpio7Blocked || gpio7BinEmpty; }
  bool isAnySensor()   const { return gpio6SensorStop || gpio6SensorMonitor || gpio6SensorLevel ||
                                      gpio7SensorStop || gpio7SensorMonitor || gpio7SensorLevel; }
  bool isAnyActor()    const { return gpio6Relay || gpio6Servo180 || gpio6Servo360 ||
                                      gpio7Relay || gpio7Servo180 || gpio7Servo360; }
};
extern C3FlexChannelConfig c3FlexConfig;
#endif  // BOARD_ESP32C3_21_1

// ============================================================================
// TOUCH 3.5 FLEX CHANNEL CONFIGURATION (JC3248W535C only)
// CH02–CH06 (GPIO 6, 7, 14, 15, 16) can each be independently configured:
//   relay / servo180 / servo360  → payment actor (relay HIGH on payment)
//   ambient-light               → mirrors display backlight state
//   sensor-stop / -monitor / -level → sensor input (INPUT_PULLUP)
//   off                         → not used
// ============================================================================
#ifdef BOARD_JC3248W535C
struct T35AmbientConfig {
  // Ambient-light flags (backlight sync)
  bool gpio6Ambient  = false;  // CH02
  bool gpio7Ambient  = false;  // CH03
  bool gpio14Ambient = false;  // CH04
  bool gpio15Ambient = false;  // CH05
  bool gpio16Ambient = false;  // CH06
  bool anyEnabled() const {
    return gpio6Ambient || gpio7Ambient || gpio14Ambient || gpio15Ambient || gpio16Ambient;
  }

  // Relay/servo actor flags (used for OFA: fire together with CH01)
  bool gpio6Actor  = false;
  bool gpio7Actor  = false;
  bool gpio14Actor = false;
  bool gpio15Actor = false;
  bool gpio16Actor = false;
  bool anyActor() const {
    return gpio6Actor || gpio7Actor || gpio14Actor || gpio15Actor || gpio16Actor;
  }

  // Activation mode
  bool oneForAll = false;      // true: CH01 payment fires all actor channels simultaneously

  // Derived: total number of independent payment channels (CH01 + relay/servo CH02-CH06)
  int paymentChannelCount = 1;
};
extern T35AmbientConfig t35AmbientConfig;
#endif  // BOARD_JC3248W535C

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
#ifdef BOARD_ESP32C3_21_1
    case 4:  return 0;  // ESP32-C3-21-1: GPIO4 = primary relay (CH01)
#endif
#ifdef BOARD_JC3248W535C
    case 5:  return 0;  // CH01
    case 6:  return 1;  // CH02
    case 7:  return 2;  // CH03
    case 14: return 3;  // CH04
    case 15: return 4;  // CH05
    case 16: return 5;  // CH06
#endif
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
    // Virtual IOExpander pins (PCF8574 P0–P7) reuse indices 4–11 on T-Display-S3
    // (GPIO 19/22/23/25/26/27/32/33 are unused on T-Display-S3)
    case 200: return 4;
    case 201: return 5;
    case 202: return 6;
    case 203: return 7;
    case 204: return 8;
    case 205: return 9;
    case 206: return 10;
    case 207: return 11;
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
  unsigned long lastServerPingTime = 0; // set when server sends WStype_PING to us
  unsigned long wsConnectedTime    = 0; // set when WebSocket TCP connection is established
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
// I/O EXPANDER CONFIGURATION (PCF8574 — T-Display-S3 only)
// ============================================================================

struct IOExpanderChannelConfig {
  String mode = "off";   // "off" | "relay"
};

struct IOExpanderConfig {
  bool enabled = false;                   // True when PCF8574 is present and configured
  IOExpanderChannelConfig channels[8];    // virtual pins 200–207 → P0–P7
};

extern IOExpanderConfig ioExpanderConfig;

// ============================================================================
// NFC MODE CONFIGURATION
// ============================================================================

struct NfcConfig {
  volatile bool emulationActive = false;  // True when card emulation task is running
  volatile bool boltcardActive = false;   // True when bolt card reader task is running
  volatile bool nfcSessionActive = false; // True during active APDU exchange (suppresses internet checks)
  volatile unsigned long pn532PauseUntil = 0; // millis() deadline while PN532 polling is paused (FD phone detection)
  // NT3H2111 NFC UID (7 bytes from Block 0) — used to filter out self-detection by PN532
  uint8_t nt3hNfcUid[7] = {0};
  bool nt3hNfcUidKnown = false;
};

extern NfcConfig nfcConfig;

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

// ============================================================================
// PIN PAD STATE (BoltCard PIN entry per LUD pinLimit proposal)
// ============================================================================

struct PinPadState {
    bool     active      = false;  // PIN pad screen is currently shown
    char     digits[5]   = {0};   // entered digits, null-terminated
    int      numDigits   = 0;     // 0–4
    int      attemptNum  = 0;     // failed attempts so far (shown from 1)
    int      maxAttempts = 3;
    long     amountSat   = 0;
    String   sessionId;
    String   errorMsg;
    bool     showError   = false;
    uint32_t errorStart  = 0;
    uint32_t activatedAt = 0;     // millis() when PIN pad was shown (for device-side timeout)
    bool     blocked          = false;  // card locked after maxAttempts failures
    bool     submitted        = false;  // PIN submitted to server, awaiting response
    uint32_t submittedAt      = 0;      // millis() when PIN was submitted
    bool     pendingShown     = false;  // PENDING screen drawn after submit delay
};

extern PinPadState pinPadState;

// ============================================================================
// MINI-POS MODE (Touch 3.5 only — amount entry → invoice → QR/NFC payment)
// ============================================================================

struct MiniPosConfig {
  bool   enabled = false;      // multiControl == "minipos"
  String currency = "EUR";     // ISO code or "Sat" for satoshi
  bool   decimal = true;       // true: amounts forced to 2 decimal places
  String invoiceKey = "";      // LNbits wallet Invoice/Read key (32 chars)
};

extern MiniPosConfig miniPosConfig;

struct MiniPosState {
  bool   inputActive = false;        // amount entry screen is shown
  char   amount[9] = {0};            // entered amount, max 7 chars + null
  int    numChars = 0;
  bool   amountLocked = false;       // true while "Last Pay" amount shown in orange
  uint32_t lockUntil = 0;            // millis() when the orange lock expires
  String infoMsg;                    // transient message ("No history", errors)
  uint32_t infoUntil = 0;            // millis() when infoMsg expires
  // Pending invoice (QR screen)
  bool   invoicePending = false;
  String paymentHash;
  String amountLine;                 // e.g. "23.50 EUR" — third label line on QR screen
  uint32_t invoiceCreatedAt = 0;     // millis() — for INVOICE_TIMEOUT

  void resetInput() {
    memset(amount, 0, sizeof(amount));
    numChars = 0;
    amountLocked = false;
    lockUntil = 0;
    infoMsg = "";
    infoUntil = 0;
  }
  void resetInvoice() {
    invoicePending = false;
    paymentHash = "";
    amountLine = "";
    invoiceCreatedAt = 0;
  }
};

extern MiniPosState miniPosState;

#endif // GLOBAL_STATE_H
