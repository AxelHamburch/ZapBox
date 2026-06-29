#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <WebSocketsClient.h>
#include <HTTPClient.h>
#include <OneButton.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "FFat.h"
#include <Wire.h>
#include <vector>

#include "PinConfig.h"
#include "Display.h"
#include "SerialConfig.h"

#ifdef ENABLE_DISPLAY
  #ifdef BOARD_JC3248W535C
    #include "TouchAXS15231B.h"
  #else
    #include "TouchCST816S.h"
  #endif
#endif

#include "DeviceState.h"
#include "GlobalState.h"
#include "Payment.h"
#include "Input.h"
#include "Network.h"
#include "UI.h"
#include "Utils.h"
#include "API.h"
#include "Navigation.h"
#include "ServoControl.h"
#include "IOExpander.h"
#include "I2CBus.h"
#include "Log.h"

// NFC modules (optional feature, gated by ENABLE_NFC build flag)
#if ENABLE_NFC
#include "NFCBoltCard.h"
#include "NFCNT3H2111.h"
#endif

#define FORMAT_ON_FAIL true
#define PARAM_FILE "/config.json"

// loop() is a very large function and the Mini-PoS flow renders QR version 11
// (BOLT11 invoices) from inside the touch handler — the QRCode library uses
// sizeable stack VLAs for that. The default 8 KB loopTask stack overflows
// (observed: stack canary panic right after invoice creation), so raise it.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

TaskHandle_t Task1;

String qrFormat = "bech32"; // "bech32" or "lud17"

// External LED button (PIN_LED_BUTTON_LED / PIN_LED_BUTTON_SW)
bool readyLedState = false; // Track current LED state to avoid redundant writes
bool initializationActive = true; // Startup/initialization phase flag for LED control

// Buttons
OneButton leftButton(PIN_BUTTON_1, true);
#if ENABLE_DISPLAY && !defined(BOARD_JC3248W535C)
// PIN_BUTTON_2 = GPIO 14, which is SPI Flash IO1 on ESP32-C3-WROOM-02.
// Declaring this globally causes OneButton's constructor to call pinMode(14, ...)
// BEFORE setup() runs, crashing the C3 before any serial output.
// JC3248W535C is touch-only — no physical right button.
OneButton rightButton(PIN_BUTTON_2, true);
#endif

#ifdef ENABLE_DISPLAY
// Touch controller (only for T-Display-S3)
#ifdef BOARD_JC3248W535C
// AXS15231B touch sits on the module's INTERNAL I²C bus (SDA=4, SCL=8). It
// uses Wire1 so the external Wire (SDA=18, SCL=17) stays free for NFC.
TouchAXS15231B touch(Wire1, 4, 8, /*rst=*/-1, /*irq=*/-1);
#else
TouchCST816S touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, PIN_TOUCH_RES, PIN_TOUCH_INT);
#endif
#endif

// Variables that remain here (not migrated to GlobalState)
String currency = "USD"; // Currency from config, default USD
bool labelsLoadedSuccessfully = false; // Track if labels were successfully fetched
bool labelsValidationAttempted = false; // Track if label fetch was attempted (regardless of success)
String payloadStr;
String lnbitsServer;
String deviceId;
unsigned long configModeStartTime = 0; // Track when config mode started for touch exit
bool firstLoop = true; // Track first loop iteration
byte currentErrorType = 0; // 0=none, 1=WiFi (highest), 2=Internet, 3=Server, 4=WebSocket (lowest)
bool onErrorScreen = false; // Track if error screen is displayed (synchronized with DeviceState)
unsigned long lastInternetCheck = 0; // Track when we last checked Internet connectivity
byte consecutiveWebSocketFailures = 0; // Track consecutive WebSocket failures to detect Internet issues
unsigned long labels404NextRetry = 0; // Next time to retry fetchSwitchLabels() after 404 (0 = not in 404 state)
byte labels404RetryCount = 0;         // How many label-fetch retries have been attempted after 404
bool needsQRRedraw = false; // Flag to trigger QR redraw after WiFi recovery
bool gestureHandledThisTouch = false; // Track if gesture was already handled in current touch session
unsigned long lastNavigationTime = 0; // Track time of last navigation for timeout-based reset
unsigned long TOUCH_DOUBLE_CLICK_MS = 1000; // 1 second window for second click (non-const for extern linkage)
const unsigned long TOUCH_LONG_PRESS_MS = 3000;  // 3 seconds for long press
const unsigned long BTC_UPDATE_INTERVAL = 300000; // 5 minutes in milliseconds
const unsigned long LABEL_UPDATE_INTERVAL = 300000; // 5 minutes in milliseconds
const unsigned long GRACE_PERIOD_MS = 1000;  // 1 second grace period after wake-up (reduced from 5s for better UX)

// Product timeout: configurable via platformio.ini build flag PRODUCT_TIMEOUT
// Default 10 seconds for testing, use 60 seconds for production
// Used when: Non-first product shown → timeout → back to Product No.1 (Ticker-OFF/Selecting)
//            Product/QR shown → timeout → back to BTC Ticker (Ticker-Always)
#ifndef PRODUCT_TIMEOUT
#define PRODUCT_TIMEOUT 10000
#endif

// BTC Ticker timeout: configurable via platformio.ini build flag BTCTICKER_TIMEOUT
// Default 10 seconds
// Used when: Ticker shown → timeout → back to QR (only in 'selecting' mode)
#ifndef BTCTICKER_TIMEOUT
#define BTCTICKER_TIMEOUT 10000
#endif

// Mini-PoS invoice timeout: configurable via platformio.ini build flag INVOICE_TIMEOUT
// Used when: Invoice QR shown → no payment within timeout → back to amount entry
#ifndef INVOICE_TIMEOUT
#define INVOICE_TIMEOUT 180000
#endif

// Mini-PoS UI timing
static const uint32_t MINIPOS_LASTPAY_LOCK_MS = 5000;  // orange readonly phase
static const uint32_t MINIPOS_INFO_MS         = 2000;  // transient info messages

// Mini-PoS helpers (defined before loop(), used from setup() as well)
static void miniPosIdleNfcTag();
// Mode-select helper: called when the user taps a mode button on the selection screen
static void applyModeSelection(int selected);

const unsigned long PRODUCT_SELECTION_DELAY = PRODUCT_TIMEOUT; // Time to return to product selection
const unsigned long BTC_TICKER_TIMEOUT_DELAY = BTCTICKER_TIMEOUT; // Time to hide ticker in selecting mode

// Multi-Channel-Control product navigation
// 0 = "Select the product" screen
// 1 = Product 1 (Pin 12)
// 2 = Product 2 (Pin 13)
// 3 = Product 3 (Pin 10)
// 4 = Product 4 (Pin 11)
// NOTE: currentProduct is now in multiChannelConfig.currentProduct (volatile)
int maxProducts = 1; // Will be set based on multiChannelConfig.mode

StateManager deviceState;  // Global state machine instance

// NFC screen state – file-level so processNormalPayment() can reset them
// when a successful payment clears the pending/NO LUCK screens.
#if ENABLE_NFC
static bool          nfcPendingScreenShown  = false;
static bool          nfcNoLuckScreenShown   = false;
static unsigned long nfcNoLuckStart         = 0;
static bool          nfcErrorDetailShown    = false;
static unsigned long nfcErrorDetailStart    = 0;
static bool          nfcNotSupportedShown   = false;
static unsigned long nfcNotSupportedStart   = 0;
#endif

WebSocketsClient webSocket;

//////////////////FORWARD DECLARATIONS///////////////////

void reportMode();
void configMode();
void showHelp();
void startAuthTeachEntry();   // Authy: open 6-digit teach PIN pad (Touch 3.5" only)
void authyShowStart();        // Authy: IDENTITY TRIGGER idle screen (also called from UI.cpp)
static void authyShowQR();    // Authy: open the identity-trigger QR
static void endAuthTeach();   // Authy: end teach session (restart on T-Display-S3)
void onNextButtonLongPress(); // Input.cpp: NEXT long-press handler (Config Mode)

//////////////////HELPERS///////////////////

void readFiles()
{
  File paramFile = FFat.open(PARAM_FILE, "r");
  if (paramFile)
  {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, paramFile.readString());

    const JsonObject maRoot0 = doc[0];
    const char *maRoot0Char = maRoot0["value"];
    wifiConfig.ssid = maRoot0Char;
    LOG_DEBUG("Config", "SSID: " + wifiConfig.ssid);

    const JsonObject maRoot1 = doc[1];
    const char *maRoot1Char = maRoot1["value"];
    wifiConfig.wifiPassword = maRoot1Char;
    LOG_DEBUG("Config", "WiFi pass: " + wifiConfig.wifiPassword);

    const JsonObject maRoot2 = doc[2];
    const char *maRoot2Char = maRoot2["value"];
    wifiConfig.switchStr = maRoot2Char;
    
    // Parse WebSocket URL (works with both ws:// and wss://)
    int protocolIndex = wifiConfig.switchStr.indexOf("://");
    LOG_DEBUG("Config", String("switchStr='") + wifiConfig.switchStr + String("', protocolIndex=") + String(protocolIndex));
    
    if (protocolIndex == -1) {
      LOG_ERROR("Config", "Invalid switchStr: " + wifiConfig.switchStr);
      lnbitsServer = "";
      deviceId = "";
    } else {
      int domainIndex = wifiConfig.switchStr.indexOf("/", protocolIndex + 3);
      LOG_DEBUG("Config", String("domainIndex=") + String(domainIndex));
      
      if (domainIndex == -1) {
        LOG_ERROR("Config", "Invalid switchStr: " + wifiConfig.switchStr);
        lnbitsServer = "";
        deviceId = "";
      } else {
        int uidLength = 22; // Length of device ID
        lnbitsServer = wifiConfig.switchStr.substring(protocolIndex + 3, domainIndex);
        deviceId = wifiConfig.switchStr.substring(wifiConfig.switchStr.length() - uidLength);
        
        LOG_DEBUG("Config", String("Extracted server from index ") + String(protocolIndex + 3) + String(" to ") + String(domainIndex));
      }
    }

    LOG_INFO("Config", "Socket: " + wifiConfig.switchStr);
    LOG_INFO("Config", "LNbits server: " + lnbitsServer);
    LOG_INFO("Config", "Switch device ID: " + deviceId);

    const JsonObject maRoot3 = doc[3];
    if (!maRoot3.isNull()) {
      const char *maRoot3Char = maRoot3["value"];
      if (maRoot3Char != nullptr) {
        qrFormat = String(maRoot3Char);
        qrFormat.trim();
      }
    }
    if (qrFormat.length() == 0) {
      qrFormat = "bech32"; // Default
    }
    LOG_INFO("Config", "QR Format: " + qrFormat);

    // Screen displayConfig.orientation configuration (maRoot4)
    // Available options:
    // "h"  = horizontal (button right)
    // "v"  = vertical (button bottom)
    // "hi" = horizontal inverse (button left)
    // "vi" = vertical inverse (button top)
    const JsonObject maRoot4 = doc[4];
    if (!maRoot4.isNull()) {
      const char *maRoot4Char = maRoot4["value"];
      if (maRoot4Char != nullptr) {
        displayConfig.orientation = maRoot4Char;
      }
    }
    if (displayConfig.orientation.length() == 0) {
      displayConfig.orientation = "h"; // Default
    }
    LOG_INFO("Config", "Screen orientation: " + displayConfig.orientation);

    const JsonObject maRoot5 = doc[5];
    if (!maRoot5.isNull()) {
      const char *maRoot5Char = maRoot5["value"];
      if (maRoot5Char != nullptr) {
        displayConfig.theme = maRoot5Char;
      }
    }
    LOG_INFO("Config", "Theme: " + displayConfig.theme);

    // Read threshold configuration (optional)
    const JsonObject maRoot6 = doc[6];
    if (!maRoot6.isNull()) {
      const char *maRoot6Char = maRoot6["value"];
      if (maRoot6Char != nullptr) {
        lightningConfig.thresholdKey = maRoot6Char;
      }
    }

    const JsonObject maRoot7 = doc[7];
    if (!maRoot7.isNull()) {
      const char *maRoot7Char = maRoot7["value"];
      if (maRoot7Char != nullptr) {
        lightningConfig.thresholdAmount = maRoot7Char;
      }
    }

    const JsonObject maRoot8 = doc[8];
    if (!maRoot8.isNull()) {
      const char *maRoot8Char = maRoot8["value"];
      if (maRoot8Char != nullptr) {
        lightningConfig.thresholdPin = maRoot8Char;
      }
    }

    const JsonObject maRoot9 = doc[9];
    if (!maRoot9.isNull()) {
      const char *maRoot9Char = maRoot9["value"];
      if (maRoot9Char != nullptr) {
        lightningConfig.thresholdTime = maRoot9Char;
      }
    }

    const JsonObject maRoot10 = doc[10];
    if (!maRoot10.isNull()) {
      const char *maRoot10Char = maRoot10["value"];
      if (maRoot10Char != nullptr) {
        lightningConfig.thresholdLnurl = maRoot10Char;
      }
    }

    // Read special mode configuration (Index 11-13)
    const JsonObject maRoot11 = doc[11];
    if (!maRoot11.isNull()) {
      const char *maRoot11Char = maRoot11["value"];
      if (maRoot11Char != nullptr) {
        specialModeConfig.mode = maRoot11Char;
      }
    }

    const JsonObject maRoot12 = doc[12];
    if (!maRoot12.isNull()) {
      const char *maRoot12Char = maRoot12["value"];
      if (maRoot12Char != nullptr) {
        specialModeConfig.frequency = String(maRoot12Char).toFloat();
        if (specialModeConfig.frequency < 0.1) specialModeConfig.frequency = 0.1;
        if (specialModeConfig.frequency > 10.0) specialModeConfig.frequency = 10.0;
      }
    }

    const JsonObject maRoot13 = doc[13];
    if (!maRoot13.isNull()) {
      const char *maRoot13Char = maRoot13["value"];
      if (maRoot13Char != nullptr) {
        specialModeConfig.dutyCycleRatio = String(maRoot13Char).toFloat();
        if (specialModeConfig.dutyCycleRatio < 0.1) specialModeConfig.dutyCycleRatio = 0.1;
        if (specialModeConfig.dutyCycleRatio > 10.0) specialModeConfig.dutyCycleRatio = 10.0;
      }
    }

    // Read powerConfig.screensaver and deep sleep configuration (optional, indices 14-16)
    const JsonObject maRoot14 = doc[14];
    if (!maRoot14.isNull()) {
      const char *maRoot14Char = maRoot14["value"];
      if (maRoot14Char != nullptr) {
        powerConfig.screensaver = maRoot14Char;
      }
    }

    const JsonObject maRoot15 = doc[15];
    if (!maRoot15.isNull()) {
      const char *maRoot15Char = maRoot15["value"];
      if (maRoot15Char != nullptr) {
        powerConfig.deepSleep = maRoot15Char;
      }
    }

    const JsonObject maRoot16 = doc[16];
    if (!maRoot16.isNull()) {
      const char *maRoot16Char = maRoot16["value"];
      if (maRoot16Char != nullptr) {
        powerConfig.activationTime = maRoot16Char;
        // Validate activation time (1-120 minutes)
        int actTime = String(powerConfig.activationTime).toInt();
        if (actTime < 1) powerConfig.activationTime = "1";
        if (actTime > 120) powerConfig.activationTime = "120";
      }
    }

    // Read multi-channel-control configuration (index 17)
    const JsonObject maRoot17 = doc[17];
    if (!maRoot17.isNull()) {
      const char *maRoot17Char = maRoot17["value"];
      if (maRoot17Char != nullptr) {
        multiChannelConfig.mode = maRoot17Char;
      }
    }

    // Mini-PoS mode is transported via multiControl ("minipos"). The device
    // then behaves like single-channel, so normalize mode back to "off" and
    // set the dedicated flag instead.
    if (multiChannelConfig.mode == "minipos") {
      miniPosConfig.enabled = true;
      multiChannelConfig.mode = "off";
      LOG_INFO("Config", "Mini-PoS mode ENABLED (single-channel behavior, amount entry screen)");
    }

    // Authy mode (LNURL-auth) is transported via multiControl ("authy"). The
    // device shows an auth QR and triggers the relay when a known wallet
    // identifies itself. Like Mini-PoS, it behaves single-channel, so normalize
    // mode back to "off" and set the dedicated flag instead.
    if (multiChannelConfig.mode == "authy") {
      authyConfig.enabled = true;
      multiChannelConfig.mode = "off";
      LOG_INFO("Config", "Authy mode ENABLED (LNURL-auth identification, relay trigger)");
    }

    // Read BTC-Ticker configuration (index 18)
    const JsonObject maRoot18 = doc[18];
    if (!maRoot18.isNull()) {
      const char *maRoot18Char = maRoot18["value"];
      if (maRoot18Char != nullptr) {
        multiChannelConfig.btcTickerMode = maRoot18Char;
        // Normalize human-friendly values from installer
        String modeNorm = multiChannelConfig.btcTickerMode;
        modeNorm.trim();
        modeNorm.toLowerCase();
        // Strip all non-letter characters to be robust against UI formatting
        String lettersOnly = "";
        for (size_t i = 0; i < modeNorm.length(); ++i) {
          char c = modeNorm.charAt(i);
          if ((c >= 'a' && c <= 'z')) lettersOnly += c;
        }
        // Map variants to canonical values
        if (lettersOnly.indexOf("always") != -1) {
          multiChannelConfig.btcTickerMode = "always";
        } else if (lettersOnly.indexOf("selecting") != -1 || lettersOnly.indexOf("select") != -1) {
          multiChannelConfig.btcTickerMode = "selecting";
        } else if (lettersOnly == "off") {
          multiChannelConfig.btcTickerMode = "off";
        }
        LOG_INFO("Config", "BTC-Ticker mode normalized: " + multiChannelConfig.btcTickerMode);
      }
    } else {
      LOG_WARN("Config", "Index 18 (btcTickerMode) not found in config - using default: " + multiChannelConfig.btcTickerMode);
    }

    // Read light barrier configuration (index 19)
    const JsonObject maRoot19 = doc[19];
    if (!maRoot19.isNull()) {
      const char *maRoot19Char = maRoot19["value"];
      if (maRoot19Char != nullptr) {
        String lightBarrierSetting = String(maRoot19Char);
        lightBarrierSetting.toLowerCase();
        lightBarrierSetting.trim();
        lightBarrierConfig.mode = lightBarrierSetting;
        lightBarrierConfig.enabled         = (lightBarrierSetting == "yes");
        lightBarrierConfig.monitoring      = (lightBarrierSetting == "monitor");
        lightBarrierConfig.levelMonitoring = (lightBarrierSetting == "level");
        lightBarrierConfig.relayOutput     = (lightBarrierSetting == "relay");
      }
      #if ENABLE_DISPLAY
      LOG_INFO("Config", String("Light barrier (GPIO 2): ") + (lightBarrierConfig.enabled ? "STOP mode" : lightBarrierConfig.monitoring ? "MONITOR mode" : lightBarrierConfig.levelMonitoring ? "LEVEL MONITORING mode" : "DISABLED"));
      #else
      LOG_INFO("Config", String("Sensor 1 (GPIO 22): ") + (lightBarrierConfig.enabled ? "STOP mode" : lightBarrierConfig.monitoring ? "MONITOR mode" : lightBarrierConfig.levelMonitoring ? "LEVEL MONITORING mode" : lightBarrierConfig.relayOutput ? "RELAY OUTPUT mode" : "DISABLED"));
      #endif
    }

    // Read sensor 2 configuration (index 31, headless only — GPIO 23)
    #if !ENABLE_DISPLAY
    const JsonObject maRoot31 = doc[31];
    if (!maRoot31.isNull()) {
      const char *maRoot31Char = maRoot31["value"];
      if (maRoot31Char != nullptr) {
        String sensor2Setting = String(maRoot31Char);
        sensor2Setting.toLowerCase();
        sensor2Setting.trim();
        lightBarrierConfig.mode2 = sensor2Setting;
        lightBarrierConfig.enabled2         = (sensor2Setting == "yes");
        lightBarrierConfig.monitoring2      = (sensor2Setting == "monitor");
        lightBarrierConfig.levelMonitoring2 = (sensor2Setting == "level");
        lightBarrierConfig.relayOutput2     = (sensor2Setting == "relay");
      }
      LOG_INFO("Config", String("Sensor 2 (GPIO 23): ") + (lightBarrierConfig.enabled2 ? "STOP mode" : lightBarrierConfig.monitoring2 ? "MONITOR mode" : lightBarrierConfig.levelMonitoring2 ? "LEVEL MONITORING mode" : lightBarrierConfig.relayOutput2 ? "RELAY OUTPUT mode" : "DISABLED"));
    }
    #endif

    // Read currency configuration (index 20)
    const JsonObject maRoot20 = doc[20];
    if (!maRoot20.isNull()) {
      const char *maRoot20Char = maRoot20["value"];
      if (maRoot20Char != nullptr) {
        currency = String(maRoot20Char);
        LOG_DEBUG("Config", "Read currency from config (before processing): " + currency);
        currency.toUpperCase();
        if (currency.length() == 0 || currency.length() > 3) {
          LOG_WARN("Config", "Invalid currency length, using default USD");
          currency = "USD";
        }
        LOG_INFO("Config", "Final currency value: " + currency);
      }
    } else {
      LOG_DEBUG("Config", "Index 20 (currency) not found in config - using default: " + currency);
    }

    // Read external LED button configuration (index 21)
    const JsonObject maRoot21 = doc[21];
    if (!maRoot21.isNull()) {
      const char *maRoot21Char = maRoot21["value"];
      if (maRoot21Char != nullptr) {
        String buttonSetting = String(maRoot21Char);
        buttonSetting.toLowerCase();
        buttonSetting.trim();
        externalButtonState.enabled = (buttonSetting == "yes");
      }
      LOG_INFO("Config", String("External LED button: ") + (externalButtonState.enabled ? "ENABLED" : "DISABLED"));
    }

    // Read channel 4 ambient light configuration (index 22)
    const JsonObject maRoot22 = doc[22];
    if (!maRoot22.isNull()) {
      const char *maRoot22Char = maRoot22["value"];
      if (maRoot22Char != nullptr) {
        String ambientSetting = String(maRoot22Char);
        ambientSetting.toLowerCase();
        ambientSetting.trim();
        channel4AmbientConfig.mode = ambientSetting;
        channel4AmbientConfig.enabled = (ambientSetting == "ambient");
      }
      LOG_INFO("Config", String("Channel 4 ambient light: ") + (channel4AmbientConfig.enabled ? "ENABLED" : "DISABLED (normal)"));
    }

    // Read deep sleep activation time (index 23)
    const JsonObject maRoot23 = doc[23];
    if (!maRoot23.isNull()) {
      const char *maRoot23Char = maRoot23["value"];
      if (maRoot23Char != nullptr) {
        powerConfig.deepSleepTime = maRoot23Char;
        int dsTime = String(powerConfig.deepSleepTime).toInt();
        if (dsTime < 1) powerConfig.deepSleepTime = "1";
        if (dsTime > 120) powerConfig.deepSleepTime = "120";
      }
    }

    // Extension API path (bitcoinswitch vs. zapbox) is auto-detected at runtime in API.cpp
    // No manual config needed - ZapBox tries bitcoinswitch first, falls back to zapbox on 404.

    // Indices 18-20 removed (lnurl13, lnurl10, lnurl11 - now auto-generated)

    // Read servo configuration (new format: indices 24-36, only relevant when multiControl == "servo")
    if (doc.size() > 24) {
      auto readInt = [&](int idx, int minVal, int maxVal, int def) -> int {
        const JsonObject obj = doc[idx];
        if (!obj.isNull()) {
          const char *v = obj["value"];
          if (v != nullptr) {
            int val = String(v).toInt();
            if (val < minVal) return minVal;
            if (val > maxVal) return maxVal;
            return val;
          }
        }
        return def;
      };
      auto readStr = [&](int idx) -> String {
        const JsonObject obj = doc[idx];
        if (!obj.isNull()) {
          const char *v = obj["value"];
          if (v != nullptr) { String s = String(v); s.toLowerCase(); s.trim(); return s; }
        }
        return String("");
      };

#if ENABLE_DISPLAY
      // Display version: Index 24 = pin13Mode string ("servo180"/"servo360"/"relay"/"off")
      // Servo 1 params (Pin 13 positional) at indices 25/26/27
      // "relay" = Pin 13 acts as normal relay (external relay connected)
      String pin13Mode = readStr(24);
      servoConfig.pin13IsRelay = (pin13Mode == "relay");
      if (pin13Mode == "servo180") {
        servoConfig.servo1Start    = readInt(25, 0, 180, 0);
        servoConfig.servo1End      = readInt(26, 0, 180, 0);
        servoConfig.servo1Duration = readInt(27, 0, 10000, 0);
      } else {
        servoConfig.servo1Start = 0;
        servoConfig.servo1End   = 0;
        servoConfig.servo1Duration = 0;
      }

      // Index 30: relay activation mode ("one-for-all"/"relay1"/"both"/"off")
      {
        String sr = readStr(30);
        if (sr.length() > 0) servoConfig.relayMode = sr;
      }

      // Index 31: pin10Mode — "servo360", "servo180", "relay", or "off"
      // Pin 10 is a continuous 360° servo → only "servo360" activates servo2
      // "relay" = Pin 10 acts as normal relay (external relay connected)
      if (doc.size() > 31) {
        String pin10Mode = readStr(31);
        servoConfig.pin10IsRelay = (pin10Mode == "relay");
        if (pin10Mode == "servo360") {
          servoConfig.servo2Speed    = readInt(35, 0, 180, 0);
          servoConfig.servo2Duration = readInt(36, 0, 10000, 0);
        } else {
          servoConfig.servo2Speed    = 0;
          servoConfig.servo2Duration = 0;
        }
      }

      if (multiChannelConfig.mode == "servo") {
        LOG_INFO("Config", "=== SERVO CONFIGURATION ===");
        if (servoConfig.pin13IsRelay)
          LOG_INFO("Config", "Pin 13: RELAY (external relay) [ACTIVE]");
        else
          LOG_INFO("Config", String("Servo 1 (Pin 13, positional): Start=") + String(servoConfig.servo1Start) + " End=" + String(servoConfig.servo1End) + " Duration=" + String(servoConfig.servo1Duration) + "ms" + (servoConfig.servo1Active() ? " [ACTIVE]" : " [inactive]"));
        if (servoConfig.pin10IsRelay)
          LOG_INFO("Config", "Pin 10: RELAY (external relay) [ACTIVE]");
        else
          LOG_INFO("Config", String("Servo 2 (Pin 10, continuous): Speed=") + String(servoConfig.servo2Speed) + " Duration=" + String(servoConfig.servo2Duration) + "ms" + (servoConfig.servo2Active() ? " [ACTIVE]" : " [inactive]"));
        LOG_INFO("Config", String("Relay mode: ") + servoConfig.relayMode + " (OneForAll=" + (servoConfig.oneForAll() ? "ON" : "OFF") + " Relay1=" + (servoConfig.relay1Active() ? "ON" : "OFF") + " Relay2=" + (servoConfig.relay2Active() ? "ON" : "OFF") + ")");
        LOG_INFO("Config", String("Active channels: ") + String(servoConfig.activeChannelCount()));
        LOG_INFO("Config", "===========================");
      }
#else
      // Headless version (esp32dev / esp32-c3-21-1):
#ifdef BOARD_ESP32C3_21_1
      // ESP32-C3-21-1: GPIO6 and GPIO7 flex channel modes (indices 24/25)
      // and their servo params (indices 26-35).
      {
        auto parseFlexMode = [](const String& m, bool& relay, bool& s180, bool& s360,
                                 bool& stop, bool& mon, bool& lvl) {
          relay = (m == "relay");
          s180  = (m == "servo180");
          s360  = (m == "servo360");
          stop  = (m == "yes");
          mon   = (m == "monitor");
          lvl   = (m == "level");
        };
        c3FlexConfig.gpio6Mode = readStr(24);
        parseFlexMode(c3FlexConfig.gpio6Mode, c3FlexConfig.gpio6Relay, c3FlexConfig.gpio6Servo180,
                      c3FlexConfig.gpio6Servo360, c3FlexConfig.gpio6SensorStop,
                      c3FlexConfig.gpio6SensorMonitor, c3FlexConfig.gpio6SensorLevel);
        if (c3FlexConfig.gpio6Servo180) {
          c3FlexConfig.gpio6S180Start    = readInt(26, 0, 180, 0);
          c3FlexConfig.gpio6S180End      = readInt(27, 0, 180, 0);
          c3FlexConfig.gpio6S180Duration = readInt(28, 0, 10000, 0);
        }
        if (c3FlexConfig.gpio6Servo360) {
          c3FlexConfig.gpio6S360Speed    = readInt(29, 0, 180, 0);
          c3FlexConfig.gpio6S360Duration = readInt(30, 0, 10000, 0);
        }
        c3FlexConfig.gpio7Mode = readStr(25);
        parseFlexMode(c3FlexConfig.gpio7Mode, c3FlexConfig.gpio7Relay, c3FlexConfig.gpio7Servo180,
                      c3FlexConfig.gpio7Servo360, c3FlexConfig.gpio7SensorStop,
                      c3FlexConfig.gpio7SensorMonitor, c3FlexConfig.gpio7SensorLevel);
        if (c3FlexConfig.gpio7Servo180) {
          c3FlexConfig.gpio7S180Start    = readInt(31, 0, 180, 0);
          c3FlexConfig.gpio7S180End      = readInt(32, 0, 180, 0);
          c3FlexConfig.gpio7S180Duration = readInt(33, 0, 10000, 0);
        }
        if (c3FlexConfig.gpio7Servo360) {
          c3FlexConfig.gpio7S360Speed    = readInt(34, 0, 180, 0);
          c3FlexConfig.gpio7S360Duration = readInt(35, 0, 10000, 0);
        }
        LOG_INFO("Config", String("C3 GPIO6 mode: ") + c3FlexConfig.gpio6Mode);
        LOG_INFO("Config", String("C3 GPIO7 mode: ") + c3FlexConfig.gpio7Mode);
      }
#else
      // Classic headless (esp32dev): Index 17 = mode ("servo180"/"servo360").
      // Servo 1 params (Pin 12, 180° positional) at indices 24/25/26.
      // Servo 2 params (Pin 12, 360° continuous) at indices 27/28.
      if (multiChannelConfig.mode == "servo180") {
        servoConfig.servo1Start    = readInt(24, 0, 180, 0);
        servoConfig.servo1End      = readInt(25, 0, 180, 0);
        servoConfig.servo1Duration = readInt(26, 0, 10000, 0);
        servoConfig.servo2Speed    = 0;
        servoConfig.servo2Duration = 0;
        LOG_INFO("Config", "=== HEADLESS SERVO CONFIGURATION ===");
        LOG_INFO("Config", String("180° Servo (Pin 12): Start=") + String(servoConfig.servo1Start) + " End=" + String(servoConfig.servo1End) + " Duration=" + String(servoConfig.servo1Duration) + "ms" + (servoConfig.servo1Active() ? " [ACTIVE]" : " [inactive]"));
        LOG_INFO("Config", "=====================================");
      } else if (multiChannelConfig.mode == "servo360") {
        servoConfig.servo1Start    = 0;
        servoConfig.servo1End      = 0;
        servoConfig.servo1Duration = 0;
        servoConfig.servo2Speed    = readInt(27, 0, 180, 0);
        servoConfig.servo2Duration = readInt(28, 0, 10000, 0);
        LOG_INFO("Config", "=== HEADLESS SERVO CONFIGURATION ===");
        LOG_INFO("Config", String("360° Servo (Pin 12): Speed=") + String(servoConfig.servo2Speed) + " Duration=" + String(servoConfig.servo2Duration) + "ms" + (servoConfig.servo2Active() ? " [ACTIVE]" : " [inactive]"));
        LOG_INFO("Config", "=====================================");
      } else {
        servoConfig.servo1Start = 0;
        servoConfig.servo1End   = 0;
        servoConfig.servo1Duration = 0;
        servoConfig.servo2Speed    = 0;
        servoConfig.servo2Duration = 0;
      }
#endif  // BOARD_ESP32C3_21_1
#endif  // ENABLE_DISPLAY

      // Index 30: relay activation mode — for Display and headless esp32dev only
      // (On C3, idx 30 = gpio6S360Duration — relay mode not applicable for single-channel)
      #ifndef BOARD_ESP32C3_21_1
      {
        String sr = readStr(30);
        if (sr.length() > 0) servoConfig.relayMode = sr;
      }
      #endif
    }

    // GPIO 3 (T-Display-S3) / GPIO 46 (JC3248W535C) / GPIO 34 (headless) — always FD for NT3H2111 (config[37] ignored)

    // Read I/O Expander configuration
    // Index 38 = ioExpander enable flag ("no" | "yes" | "yes2")
    // Indices 39-46 = legacy channel-mode placeholders (ignored; PCF8574 channels are relay-only,
    //   activated directly by LNbits virtual-pin assignments 200-207)
    #if ENABLE_DISPLAY
    {
      bool expanderEnabled = false;
      const JsonObject enableObj = doc[38];
      if (!enableObj.isNull()) {
        const char* val = enableObj["value"];
        if (val != nullptr) {
          String s = String(val); s.toLowerCase(); s.trim();
          expanderEnabled = (s == "yes" || s == "yes2" || s == "true" || s == "1");
        }
      }
      ioExpanderConfig.enabled = expanderEnabled;
      // Mark all 8 channels as relay (activation driven by LNbits, not local config)
      for (int ch = 0; ch < 8; ch++) {
        ioExpanderConfig.channels[ch].mode = expanderEnabled ? "relay" : "off";
      }
      LOG_INFO("Config", String("I/O Expander (PCF8574): ") + (expanderEnabled ? "ENABLED (pins 200-207 → relay)" : "disabled"));
    }
    #endif

    // Read Touch 3.5 flex channel config (indices 47-53, JC3248W535C only)
    // 47=gpio5Mode(CH01) 48=gpio6Mode(CH02) 49=gpio7Mode(CH03) 50=gpio14Mode(CH04)
    // 51=gpio15Mode(CH05) 52=gpio16Mode(CH06) 53=t35ActivationMode
    #ifdef BOARD_JC3248W535C
    {
      auto readGpioMode = [&](int idx) -> String {
        const JsonObject obj = doc[idx];
        if (obj.isNull()) return "off";
        const char* v = obj["value"];
        return v ? String(v) : "off";
      };
      auto isActor = [](const String& m) {
        return m == "relay" || m == "servo180" || m == "servo360";
      };

      String m5  = readGpioMode(47); // CH01 (GPIO 5) — primary channel
      String m6  = readGpioMode(48);
      String m7  = readGpioMode(49);
      String m14 = readGpioMode(50);
      String m15 = readGpioMode(51);
      String m16 = readGpioMode(52);
      String act = readGpioMode(53); // t35ActivationMode: "off" | "one-for-all"

      t35AmbientConfig.gpio6Ambient  = (m6  == "ambient-light");
      t35AmbientConfig.gpio7Ambient  = (m7  == "ambient-light");
      t35AmbientConfig.gpio14Ambient = (m14 == "ambient-light");
      t35AmbientConfig.gpio15Ambient = (m15 == "ambient-light");
      t35AmbientConfig.gpio16Ambient = (m16 == "ambient-light");

      t35AmbientConfig.gpio6Actor  = isActor(m6);
      t35AmbientConfig.gpio7Actor  = isActor(m7);
      t35AmbientConfig.gpio14Actor = isActor(m14);
      t35AmbientConfig.gpio15Actor = isActor(m15);
      t35AmbientConfig.gpio16Actor = isActor(m16);

      t35AmbientConfig.oneForAll = (act == "one-for-all");

      // Per-channel servo configuration (indices 58-87: 6 channels × 5 params).
      //   58-62 CH01(GPIO5)  63-67 CH02(GPIO6)  68-72 CH03(GPIO7)
      //   73-77 CH04(GPIO14) 78-82 CH05(GPIO15) 83-87 CH06(GPIO16)
      // Param order per channel: S180Start, S180End, S180Duration, S360Speed, S360Duration
      {
        auto readInt = [&](int idx) -> int {
          const JsonObject o = doc[idx];
          if (o.isNull()) return 0;
          const char* v = o["value"];
          return v ? String(v).toInt() : 0;
        };
        auto setupServoCh = [&](int chIdx, const String& mode, int base) {
          auto& sc = t35AmbientConfig.servo[chIdx];
          sc.servo180    = (mode == "servo180");
          sc.servo360    = (mode == "servo360");
          sc.s180Start    = readInt(base + 0);
          sc.s180End      = readInt(base + 1);
          sc.s180Duration = readInt(base + 2);
          sc.s360Speed    = readInt(base + 3);
          sc.s360Duration = readInt(base + 4);
        };
        setupServoCh(0, m5,  58);
        setupServoCh(1, m6,  63);
        setupServoCh(2, m7,  68);
        setupServoCh(3, m14, 73);
        setupServoCh(4, m15, 78);
        setupServoCh(5, m16, 83);
      }

      // Count actual payment channels: CH01 always, plus each relay/servo channel
      int extra = (t35AmbientConfig.gpio6Actor  ? 1 : 0)
                + (t35AmbientConfig.gpio7Actor  ? 1 : 0)
                + (t35AmbientConfig.gpio14Actor ? 1 : 0)
                + (t35AmbientConfig.gpio15Actor ? 1 : 0)
                + (t35AmbientConfig.gpio16Actor ? 1 : 0);
      t35AmbientConfig.paymentChannelCount = 1 + extra;

      // If no CH02-CH06 channel is a relay/servo, treat device as single-channel
      // even when multiControl="duo" was selected in the installer (e.g. to
      // configure ambient-light on CH02 without adding a second payment channel).
      if (extra == 0 && multiChannelConfig.mode == "duo") {
        multiChannelConfig.mode = "off";
        LOG_INFO("Config", "T35: No extra payment channels — overriding multi-channel to single");
      }

      // Index 57: numerical product selection ("no" | "yes") — multi-channel only,
      // mutually exclusive with One for All (the installer enforces this too)
      String numSel = readGpioMode(57);
      t35AmbientConfig.numericSelect = (numSel == "yes" && (multiChannelConfig.mode == "duo" || multiChannelConfig.mode == "modeselect"));
      if (t35AmbientConfig.numericSelect && t35AmbientConfig.oneForAll) {
        t35AmbientConfig.numericSelect = false;
        LOG_INFO("Config", "T35: Numeric product selection disabled (One for All active)");
      }

      LOG_INFO("Config", String("T35 GPIO modes — 6:") + m6 + " 7:" + m7
               + " 14:" + m14 + " 15:" + m15 + " 16:" + m16
               + " | OFA:" + (t35AmbientConfig.oneForAll ? "yes" : "no")
               + " | numSel:" + (t35AmbientConfig.numericSelect ? "yes" : "no")
               + " | paymentCh:" + t35AmbientConfig.paymentChannelCount);
    }
    #endif

    // Read Mini-PoS configuration (indices 54-56, Touch 3.5 only)
    // 54=miniPosCurrency  55=miniPosDecimal(yes/no)  56=miniPosInvoiceKey
    if (miniPosConfig.enabled || multiChannelConfig.mode == "modeselect") {
      const JsonObject maRoot54 = doc[54];
      if (!maRoot54.isNull()) {
        const char *v = maRoot54["value"];
        if (v != nullptr && strlen(v) > 0) {
          miniPosConfig.currency = String(v);
          miniPosConfig.currency.trim();
        }
      }
      const JsonObject maRoot55 = doc[55];
      if (!maRoot55.isNull()) {
        const char *v = maRoot55["value"];
        if (v != nullptr) {
          String dec = String(v); dec.toLowerCase(); dec.trim();
          miniPosConfig.decimal = (dec != "no");
        }
      }
      const JsonObject maRoot56 = doc[56];
      if (!maRoot56.isNull()) {
        const char *v = maRoot56["value"];
        if (v != nullptr) {
          miniPosConfig.invoiceKey = String(v);
          miniPosConfig.invoiceKey.trim();
        }
      }
      // Mini-PoS owns the screen. Threshold mode is disabled; the BTC ticker
      // is only supported in "always" mode where it acts as a screensaver
      // (touch → amount entry, idle PRODUCT_TIMEOUT → back to ticker).
      if (multiChannelConfig.btcTickerMode != "always") {
        multiChannelConfig.btcTickerMode = "off";
      }
      lightningConfig.thresholdKey = "";
      LOG_INFO("Config", "=== MINI-POS CONFIGURATION ===");
      LOG_INFO("Config", "Currency: " + miniPosConfig.currency);
      LOG_INFO("Config", String("Decimal separator: ") + (miniPosConfig.decimal ? "YES (2 places)" : "NO"));
      LOG_INFO("Config", String("Invoice key: ") + (miniPosConfig.invoiceKey.length() > 0 ? "set (" + String(miniPosConfig.invoiceKey.length()) + " chars)" : "MISSING!"));
      LOG_INFO("Config", String("BTC-Ticker screensaver: ") + (multiChannelConfig.btcTickerMode == "always" ? "ON (always)" : "off"));
      LOG_INFO("Config", "==============================");
    }

    // Read Authy configuration.
    // Touch 3.5" installer sends indices 88-92.
    // T-Display-S3 installer sends indices 47-52 (same field order + teach PIN at 52).
    // Both blocks are parsed so mode-select → Authy gets the correct values.
#ifndef BOARD_JC3248W535C
    // T-Display-S3: indices 47-52
    // 47=authPin  48=authDuration  49=authLabel  50=authNtag424Pin(always "no")  51=authDualPage  52=authTeachPin
    {
      const JsonObject a47 = doc[47];
      if (!a47.isNull()) {
        const char *v = a47["value"];
        if (v != nullptr && strlen(v) > 0) {
          int p = String(v).toInt();
          if (p > 0) authyConfig.authPin = p;
        }
      }
      const JsonObject a48 = doc[48];
      if (!a48.isNull()) {
        const char *v = a48["value"];
        if (v != nullptr && strlen(v) > 0) {
          int d = String(v).toInt();
          if (d < 100) d = 100;
          if (d > 60000) d = 60000;
          authyConfig.authDuration = d;
        }
      }
      const JsonObject a49 = doc[49];
      if (!a49.isNull()) {
        const char *v = a49["value"];
        if (v != nullptr && strlen(v) > 0) {
          authyConfig.label = String(v);
        }
      }
      // index 50: authNtag424Pin — T-Display-S3 has no touch, always false
      authyConfig.ntag424Pin = false;
      const JsonObject a51 = doc[51];
      if (!a51.isNull()) {
        const char *v = a51["value"];
        if (v != nullptr) {
          authyConfig.dualPage = (String(v) == "yes");
        }
      }
      // index 52: authTeachPin — one-time teach PIN from installer
      const JsonObject a52 = doc[52];
      if (!a52.isNull()) {
        const char *v = a52["value"];
        if (v != nullptr && strlen(v) >= 6) {
          authyConfig.teachPin = String(v);
          if (authyConfig.enabled) {
            // Auto-start teach mode on first main-loop iteration once connected
            authyState.pendingTeachStart = true;
            LOG_INFO("Teach", String("Teach PIN set — will enter teach mode on connect (PIN: ") + String(v) + ")");
          }
        }
      }
    }
#endif
    // Touch 3.5": indices 88-92
    // 88=authPin  89=authDuration(ms)  90=authLabel  91=authNtag424Pin  92=authDualPage
    {
      const JsonObject maRoot88 = doc[88];
      if (!maRoot88.isNull()) {
        const char *v = maRoot88["value"];
        if (v != nullptr && strlen(v) > 0) {
          int p = String(v).toInt();
          if (p > 0) authyConfig.authPin = p;
        }
      }
      const JsonObject maRoot89 = doc[89];
      if (!maRoot89.isNull()) {
        const char *v = maRoot89["value"];
        if (v != nullptr && strlen(v) > 0) {
          int d = String(v).toInt();
          if (d < 100) d = 100;
          if (d > 60000) d = 60000;
          authyConfig.authDuration = d;
        }
      }
      const JsonObject maRoot90 = doc[90];
      if (!maRoot90.isNull()) {
        const char *v = maRoot90["value"];
        if (v != nullptr && strlen(v) > 0) {
          authyConfig.label = String(v);
        }
      }
      const JsonObject maRoot91 = doc[91];
      if (!maRoot91.isNull()) {
        const char *v = maRoot91["value"];
        if (v != nullptr) {
          authyConfig.ntag424Pin = (String(v) != "no");
        }
      }
      const JsonObject maRoot92 = doc[92];
      if (!maRoot92.isNull()) {
        const char *v = maRoot92["value"];
        if (v != nullptr) {
          authyConfig.dualPage = (String(v) == "yes");
        }
      }
    }
    // Authy mode-specific side effects (only when booting directly into Authy).
    // When entering via mode-select, applyModeSelection() applies these instead.
    if (authyConfig.enabled) {
      // Authy owns the screen like Mini-PoS: no fixed-amount threshold QR —
      // except in dual-page mode, where the classic payment page keeps the
      // normal threshold/product QR for the auth pin.
      if (multiChannelConfig.btcTickerMode != "always") {
        multiChannelConfig.btcTickerMode = "off";
      }
      if (!authyConfig.dualPage) {
        lightningConfig.thresholdKey = "";
      }
      LOG_INFO("Config", "=== AUTHY CONFIGURATION ===");
      LOG_INFO("Config", String("Auth pin: ") + String(authyConfig.authPin));
      LOG_INFO("Config", String("Activation time: ") + String(authyConfig.authDuration) + "ms");
      LOG_INFO("Config", String("Identity label: ") + authyConfig.label);
      LOG_INFO("Config", String("NTAG 424 PIN: ") + (authyConfig.ntag424Pin ? "yes" : "no"));
      LOG_INFO("Config", String("Dual page (payment): ") + (authyConfig.dualPage ? "yes" : "no"));
      LOG_INFO("Config", "===========================");
    }

    // Apply predefined mode settings
    if (specialModeConfig.mode == "blink") {
      specialModeConfig.frequency = 1.0;
      specialModeConfig.dutyCycleRatio = 1.0;
      LOG_INFO("Config", "Applied 'blink' preset: 1 Hz, 1:1");
    } else if (specialModeConfig.mode == "pulse") {
      specialModeConfig.frequency = 2.0;
      specialModeConfig.dutyCycleRatio = 0.25; // 1:4 = 0.25
      LOG_INFO("Config", "Applied 'pulse' preset: 2 Hz, 1:4");
    } else if (specialModeConfig.mode == "fast-blink") {
      specialModeConfig.frequency = 5.0;
      specialModeConfig.dutyCycleRatio = 1.0;
      LOG_INFO("Config", "Applied 'fast-blink' preset: 5 Hz, 1:1");
    }
    
    LOG_INFO("Config", "Special Mode: " + specialModeConfig.mode);
    LOG_INFO("Config", String("Frequency: ") + String(specialModeConfig.frequency) + String(" Hz"));
    LOG_INFO("Config", String("Duty Cycle Ratio: ") + String(specialModeConfig.dutyCycleRatio));

    // Display Multi-Channel-Control configuration
    String modeDesc = "Single (Pin 12 only)";
    if (multiChannelConfig.mode == "duo") modeDesc = "Duo (Pins 12, 13)";
    else if (multiChannelConfig.mode == "quattro") modeDesc = "Quattro (Pins 12, 13, 10, 11)";
    else if (multiChannelConfig.mode == "servo") modeDesc = "Servo (2 relay 12/11 + 2 servo 13/10)";
    else if (multiChannelConfig.mode == "servo180") modeDesc = "180° Servo (Pin 12)";
    else if (multiChannelConfig.mode == "servo360") modeDesc = "360° Servo (Pin 12)";
    LOG_INFO("Config", String("ZapBox Mode: ") + modeDesc);

    // Display BTC-Ticker configuration
    LOG_INFO("Config", "=== BTC-TICKER CONFIGURATION ===");
    LOG_INFO("Config", "BTC-Ticker Mode: " + multiChannelConfig.btcTickerMode);
    LOG_INFO("Config", "Currency: " + currency);
    LOG_INFO("Config", "===================================");

    // Display mode based on threshold configuration
    LOG_INFO("Config", "=== MODE CONFIGURATION ===");
    if (lightningConfig.thresholdKey.length() > 0) {
      LOG_INFO("Config", "THRESHOLD MODE");
      LOG_INFO("Config", "Threshold Key: " + lightningConfig.thresholdKey);
      LOG_INFO("Config", "Threshold Amount: " + lightningConfig.thresholdAmount + " sats");
      LOG_INFO("Config", "GPIO Pin: " + lightningConfig.thresholdPin);
      LOG_INFO("Config", "Control Time: " + lightningConfig.thresholdTime + " ms");
      
      // Process threshold LNURL (add lightning: prefix if not present)
      if (lightningConfig.thresholdLnurl.length() > 0) {
        lightningConfig.thresholdLnurl.trim(); // Remove leading/trailing whitespace
        
        if (lightningConfig.thresholdLnurl.startsWith("lightning:") || lightningConfig.thresholdLnurl.startsWith("LIGHTNING:")) {
          // Already has prefix, convert to lowercase lightning:
          if (lightningConfig.thresholdLnurl.startsWith("LIGHTNING:")) {
            strcpy(lightningConfig.lightning, "lightning:");
            strcat(lightningConfig.lightning, lightningConfig.thresholdLnurl.c_str() + 10);
          } else {
            strcpy(lightningConfig.lightning, lightningConfig.thresholdLnurl.c_str());
          }
        } else {
          // No prefix, add it (lowercase)
          strcpy(lightningConfig.lightning, "lightning:");
          strcat(lightningConfig.lightning, lightningConfig.thresholdLnurl.c_str());
        }
        Serial.print("Threshold LNURL: ");
        Serial.println(lightningConfig.thresholdLnurl);
        Serial.print("Threshold QR: ");
        Serial.println(lightningConfig.lightning);
      }
    } else {
      LOG_INFO("Config", "NORMAL MODE");
    }

    // Display powerConfig.screensaver and deep sleep configuration
    LOG_INFO("Config", "=== POWER SAVING CONFIGURATION ===");
    LOG_INFO("Config", "Screensaver: " + powerConfig.screensaver);
    LOG_INFO("Config", "Deep Sleep: " + powerConfig.deepSleep);
    LOG_INFO("Config", "Screensaver Activation Time: " + powerConfig.activationTime + " minutes");
    LOG_INFO("Config", "Deep Sleep Activation Time: " + powerConfig.deepSleepTime + " minutes");
    
    // Convert screensaver activation time from minutes to milliseconds
    int activationTimeMinutes = String(powerConfig.activationTime).toInt();
    powerConfig.activationTimeoutMs = activationTimeMinutes * 60 * 1000UL;
    LOG_INFO("Config", "Screensaver Timeout: " + String(powerConfig.activationTimeoutMs) + " ms");
    
    // Convert deep sleep activation time from minutes to milliseconds
    int deepSleepTimeMinutes = String(powerConfig.deepSleepTime).toInt();
    powerConfig.deepSleepTimeoutMs = deepSleepTimeMinutes * 60 * 1000UL;
    LOG_INFO("Config", "Deep Sleep Timeout: " + String(powerConfig.deepSleepTimeoutMs) + " ms");
    
    // Determine and display active power saving mode
    if (powerConfig.screensaver != "off" && powerConfig.deepSleep != "off") {
      LOG_INFO("Config", "⚡ POWER SAVING MODE: SCREENSAVER + DEEP SLEEP (" + powerConfig.deepSleep + ")");
      LOG_INFO("Config", "   Screensaver after " + powerConfig.activationTime + " min, then " + powerConfig.deepSleep + " sleep after " + powerConfig.deepSleepTime + " min");
      if (powerConfig.deepSleep == "light") {
        LOG_INFO("Config", "   Press BOOT, IO14 or LED button to wake up");
      } else {
        LOG_INFO("Config", "   Press BOOT or IO14 button to wake up");
      }
    } else if (powerConfig.screensaver != "off") {
      LOG_INFO("Config", "⚡ POWER SAVING MODE: SCREENSAVER");
      LOG_INFO("Config", "   Display backlight will turn off after " + powerConfig.activationTime + " minutes");
      LOG_INFO("Config", "   Press BOOT or IO14 button to wake up");
    } else if (powerConfig.deepSleep != "off") {
      LOG_INFO("Config", "⚡ POWER SAVING MODE: DEEP SLEEP (" + powerConfig.deepSleep + ")");
      LOG_INFO("Config", "   Device will enter " + powerConfig.deepSleep + " sleep after " + powerConfig.deepSleepTime + " minutes");
      if (powerConfig.deepSleep == "light") {
        LOG_INFO("Config", "   Press BOOT, IO14 or LED button to wake up");
      } else {
        LOG_INFO("Config", "   Press BOOT or IO14 button to wake up");
      }
    } else {
      LOG_INFO("Config", "⚡ POWER SAVING MODE: DISABLED");
      LOG_INFO("Config", "   Device will stay active continuously");
    }
    
    // Initialize last activity time
    activityTracking.lastActivityTime = millis();
    LOG_DEBUG("Config", "Last Activity Time initialized: " + String(activityTracking.lastActivityTime) + " ms");
    
    LOG_INFO("Config", "===================================");
  }
  else
  {
    Serial.println("Config file not found - using defaults");
    displayConfig.orientation = "h";
    strcpy(lightningConfig.lightning, "LIGHTNING:lnurl1dp68gurn8ghj7ctsdyhxkmmvwp5jucm0d9hkuegpr4r33");
    LOG_INFO("Config", "=== NORMAL MODE ===");
  }
  paramFile.close();
}

// ═══════════════════════════════════════════════════════════════════════════════════
// MODE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════════

void showHelp()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    LOG_DEBUG("Help", "Device woke up, not entering help mode");
    return; // Don't trigger help mode, just wake up
  }

  LOG_INFO("Help", "Help button pressed");
  deviceState.transition(DeviceState::HELP_SCREEN); // Set flag to interrupt WiFi reconnect loop

  // Disable product selection timer during help mode
  productSelectionState.showTime = 0;

  stepOneScreen();

  // Check for button press during first screen
  unsigned long screenStart = millis();
  while (millis() - screenStart < 1000 && deviceState.isInState(DeviceState::HELP_SCREEN)) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!deviceState.isInState(DeviceState::HELP_SCREEN)) break; // Button pressed in handleTouchButton()
  }
  if (!deviceState.isInState(DeviceState::HELP_SCREEN)) return; // Exit Help early

  stepTwoScreen();

  // Check for button press during second screen
  screenStart = millis();
  while (millis() - screenStart < 1000 && deviceState.isInState(DeviceState::HELP_SCREEN)) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!deviceState.isInState(DeviceState::HELP_SCREEN)) break;
  }
  if (!deviceState.isInState(DeviceState::HELP_SCREEN)) return; // Exit Help early

  stepThreeScreen();

  // Check for button press during third screen
  screenStart = millis();
  while (millis() - screenStart < 1000 && deviceState.isInState(DeviceState::HELP_SCREEN)) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!deviceState.isInState(DeviceState::HELP_SCREEN)) break;
  }

  deviceState.transition(DeviceState::READY); // Clear flag

  // Return to error screen if one was active, otherwise show QR screen
  if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
    // Check which error is active and show corresponding screen
    if (WiFi.status() != WL_CONNECTED) {
      wifiReconnectScreen();
    } else if (networkStatus.waitingForPong && (millis() - networkStatus.lastPingTime > 10000)) {
      internetReconnectScreen();
    } else if (!webSocket.isConnected()) {
      websocketReconnectScreen();
    }
  } else {
    // No error active, show QR screen
    redrawQRScreen();
    // Reset product selection timer
    productSelectionState.showTime = millis();
    deviceState.transition(DeviceState::READY);
  }
}

void configMode()
{
  // Print entry timestamp BEFORE suppressing logs so it always appears in serial output.
  // This lets us measure the exact delay from button press to configMode() invocation.
  Serial.printf("[CONFIG] configMode() called at %lu ms – entering config mode\n", millis());
  Serial.printf("[CONFIG] WiFi teardown will take ~2.7 s (delays: 2000+200+500 ms)\n");

  // Suppress ALL Log output globally FIRST - before WiFi.disconnect() triggers
  // async callbacks (e.g. WebSocket Disconnected) that write via LOG_INFO.
  Log::suppressed = true;

  // Suppress ESP-IDF internal logging (ssl_client, WiFiGeneric, etc.)
  // These bypass our Log:: system and write directly via esp_log_write().
  // The wildcard "*" only sets the DEFAULT level — tags that were already
  // registered with explicit levels (wifi, ssl_client) keep their old level.
  // So we must also suppress the known offenders by tag name.
  esp_log_level_set("*", ESP_LOG_NONE);
  esp_log_level_set("wifi", ESP_LOG_NONE);
  esp_log_level_set("WiFiGeneric.cpp", ESP_LOG_NONE);
  esp_log_level_set("ssl_client.cpp", ESP_LOG_NONE);
  esp_log_level_set("ssl_client", ESP_LOG_NONE);
  esp_log_level_set("WiFiSTA", ESP_LOG_NONE);
  esp_log_level_set("WiFiMulti", ESP_LOG_NONE);
  esp_log_level_set("WiFiGeneric", ESP_LOG_NONE); // Some IDF versions use tag without .cpp
  esp_log_level_set("wifi_init", ESP_LOG_NONE);
  esp_log_level_set("phy_init", ESP_LOG_NONE);

  // Set CONFIG_MODE state BEFORE WiFi teardown so that any in-progress HTTP
  // requests on Core 1 (e.g. fetchBitcoinData, fetchSwitchConfigurations) can
  // check the flag and abort.  Without this, WiFi.disconnect(true) frees the
  // mbedTLS/SSL resources while Core 1 is still inside http.GET(), causing a
  // LoadProhibited crash (EXCVADDR ~0x130).
  deviceState.transition(DeviceState::CONFIG_MODE);

  // Give in-progress HTTPS requests on Core 1 time to complete or fail.
  // fetchBitcoinData() uses http.setTimeout(5000) — the typical request
  // finishes in <2s, so 2 seconds is enough for most cases.  Even if the
  // request is still in flight, the graceful WiFi.disconnect(false) below
  // will send TCP RST and trigger a clean HTTP error instead of a crash.
  // Headless (ENABLE_DISPLAY=0): ENABLE_BITCOIN_DATA=0 → no HTTPS on Core 1,
  // so 300 ms is enough to let WebSocket / other tasks notice CONFIG_MODE.
#if ENABLE_DISPLAY
  delay(2000);
#else
  delay(300);
#endif

  // Step 1: Graceful WiFi disconnect (sends TCP FIN/RST to active sockets).
  // Using false = don't kill the radio yet, just close connections cleanly.
  WiFi.disconnect(false);
  delay(200); // Let TCP close / SSL shutdown propagate

  // Step 2: Now shut down the radio completely.
  WiFi.mode(WIFI_OFF);
  delay(500); // Let pending WiFi event callbacks drain completely

  // Set CONFIG_MODE state SILENTLY (DeviceState suppresses serial for CONFIG_MODE)
  Serial.printf("[CONFIG] Config screen now shown at %lu ms\n", millis());
  configModeScreen(); // Draw config screen
  configModeStartTime = millis();
  updateReadyLed();

  // Set touch controller pointer for SerialConfig to access
  extern void* touchControllerPtr;
  touchControllerPtr = (void*)&touch;

  // ALL serial output happens inside executeConfig() with proper USB CDC pacing.
  // Do NOT print anything here — setup() on Core 1 may still be writing to Serial.
  bool hasExistingData = (wifiConfig.ssid.length() > 0);
  configOverSerialPort(wifiConfig.ssid, wifiConfig.wifiPassword, hasExistingData);
}

void reportMode()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    LOG_DEBUG("Report", "Device woke up, not entering report mode");
    return; // Don't trigger report mode, just wake up
  }
  
  // Ignore if we just entered config mode (prevents triggering on button release)
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    LOG_DEBUG("Report", "Ignored - in config mode");
    return;
  }
  
  Serial.println("[BUTTON] Report mode button pressed");
  deviceState.transition(DeviceState::REPORT_SCREEN); // Set flag to interrupt WiFi reconnect loop
  
  // Disable product selection timer during report mode
  productSelectionState.showTime = 0;
  deviceState.transition(DeviceState::READY);
  
  Serial.println("[REPORT] Showing error report screen");
  errorReportScreen(networkStatus.errors.wifi, networkStatus.errors.internet, networkStatus.errors.server, networkStatus.errors.websocket);
  Serial.println("[REPORT] Error report shown, waiting 5s");
  vTaskDelay(pdMS_TO_TICKS(5000)); // First screen: 5 seconds
  
  Serial.println("[REPORT] Showing WiFi screen");
  wifiReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  
  Serial.println("[REPORT] Showing Internet screen");
  internetReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  
  Serial.println("[REPORT] Showing Server screen");
  serverReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  
  Serial.println("[REPORT] Showing WebSocket screen");
  websocketReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  
  Serial.println("[REPORT] Determining final screen to show");
  // Show appropriate screen based on current error state
  if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
    // Check which error is active and show corresponding screen (priority order)
    Serial.printf("[REPORT] Error active (type %d) - showing error screen\n", currentErrorType);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[REPORT] WiFi down - showing WiFi screen");
      wifiReconnectScreen();
    } else if (!checkInternetConnectivity()) {
      Serial.println("[REPORT] Internet down - showing Internet screen");
      internetReconnectScreen();
    } else if (networkStatus.waitingForPong && (millis() - networkStatus.lastPingTime > 10000)) {
      Serial.println("[REPORT] Server down - showing Server screen");
      serverReconnectScreen();
    } else if (!webSocket.isConnected()) {
      Serial.println("[REPORT] WebSocket down - showing WebSocket screen");
      websocketReconnectScreen();
    }
  } else {
    // No error active, show QR screen
    Serial.println("[REPORT] No errors - showing QR screen");
    redrawQRScreen();
    Serial.println("[REPORT] QR screen drawn successfully");
    // Reset product selection timer
    productSelectionState.showTime = millis();
  }
  
  Serial.println("[REPORT] Report mode complete, clearing flag");
  deviceState.transition(DeviceState::READY); // Clear flag AFTER showing final screen
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TASK - BUTTON HANDLER
// ═══════════════════════════════════════════════════════════════════════════════════

// Full Task1 handler: buttons, touch, external LED-button, and WiFi state
void Task1code(void *pvParameters)
{
  for (;;)
  {
    // Monitor WiFi state and update device state machine
    // Skip during CONFIG_MODE: WiFi radio may be OFF and serial config handles everything
    if (!deviceState.isInState(DeviceState::CONFIG_MODE)) {
      checkWiFiStatus();
    }

    // Direct GPIO monitoring for BOOT button – independent of OneButton timing
    // Logs exact press/release timestamps for diagnosing config-mode trigger delay
    {
      static bool lastBootState = HIGH;
      bool curBootState = (digitalRead(PIN_BUTTON_1) == LOW) ? LOW : HIGH;
      if (curBootState != lastBootState) {
        if (curBootState == LOW) {
          Serial.printf("[BUTTON] Boot button PRESSED  at %lu ms (hold 3 s for config mode)\n", millis());
        } else {
          Serial.printf("[BUTTON] Boot button RELEASED at %lu ms\n", millis());
        }
        lastBootState = curBootState;
      }
    }

    leftButton.tick();
#if ENABLE_DISPLAY && !defined(BOARD_JC3248W535C)
    rightButton.tick(); // T-Display-S3 only
#endif

    // Handle external LED-button (GPIO 44 input)
    handleExternalButton();
    checkExternalButtonHolds(); // Continuously check for hold actions
    handleConfigExitButtons();
    updateReadyLed();

    // Handle touch button if available
    if (touchState.available) {
      handleTouchButton();
    }

    vTaskDelay(pdMS_TO_TICKS(5)); // 5ms delay - faster response for touch button
  }
}

// ═══════════════════════════════════════════════════════════════════════════════════
// SETUP - INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════════════

void setup()
{
  Serial.setRxBufferSize(2048); // Increased for long JSON with LNURL
  Serial.begin(115200);

  int timer = 0;

#if PIN_POWER_ON >= 0
  // NOTE: GPIO 11-17 on ESP32-C3-WROOM-02 are SPI flash pins — never configure
  // them as GPIO output. PIN_POWER_ON is set to -1 for BOARD_ESP32C3_21_1.
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
#endif

  // External LED-button wiring: source 3.3V on LED pin when ready; input uses pull-up
  #if PIN_LED_BUTTON_LED >= 0
  pinMode(PIN_LED_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_LED_BUTTON_LED, LOW); // LED off until device is ready
  #endif
  #ifdef PIN_ONBOARD_LED
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  digitalWrite(PIN_ONBOARD_LED, LOW); // Onboard LED off until device is ready
  #endif
  #ifdef PIN_LED_BUTTON_SW
  pinMode(PIN_LED_BUTTON_SW, INPUT_PULLUP);
  #endif
  
  // Vending machine light barrier (NPN on GPIO 2)
  #ifdef PIN_LIGHT_BARRIER
  if (lightBarrierConfig.isActive()) {
    pinMode(PIN_LIGHT_BARRIER, INPUT_PULLUP);  // NPN light barrier with pull-up
    Serial.println("[LIGHT BARRIER] GPIO 2 initialized (NPN sensor, active LOW)");
  }
  #endif

  // Vending machine sensor inputs (headless only — GPIO 22/23)
  // NOTE: Moved AFTER readFiles() — see sensor/relay-output init below

  // Boot indicator: Blink LEDs 3 times quickly to show device is starting
  for (int i = 0; i < 3; i++) {
    #if PIN_LED_BUTTON_LED >= 0
    digitalWrite(PIN_LED_BUTTON_LED, HIGH);
    #endif
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, HIGH);
    #endif
    delay(100); // 100ms on
    #if PIN_LED_BUTTON_LED >= 0
    digitalWrite(PIN_LED_BUTTON_LED, LOW);
    #endif
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, LOW);
    #endif
    delay(100); // 100ms off
  }

  FFat.begin(FORMAT_ON_FAIL);
  readFiles(); // get the saved details and store in global variables

  Serial.println("\n[SETUP] readFiles() completed");
  Serial.println("[SETUP] currency = " + currency);
  Serial.println("[SETUP] multiChannelConfig.btcTickerMode = " + multiChannelConfig.btcTickerMode);
  
  // Channel 4 ambient light (GPIO 11 synced with display backlight)
  // MUST be initialized AFTER readFiles() because channel4AmbientConfig is set there
  if (channel4AmbientConfig.enabled) {
    pinMode(11, OUTPUT);
    digitalWrite(11, HIGH); // Turn on by default (display backlight is on at startup)
    Serial.println("[AMBIENT LIGHT] GPIO 11 initialized (synced with display backlight)");
  }

  // Touch 3.5 flex channel init (JC3248W535C): CH02–CH06 (GPIO 6,7,14,15,16)
  #ifdef BOARD_JC3248W535C
  {
    struct { int gpio; bool ambient; bool actor; bool sensor; } flexPins[] = {
      { 6,  t35AmbientConfig.gpio6Ambient,  t35AmbientConfig.gpio6Actor,  false },
      { 7,  t35AmbientConfig.gpio7Ambient,  t35AmbientConfig.gpio7Actor,  false },
      { 14, t35AmbientConfig.gpio14Ambient, t35AmbientConfig.gpio14Actor, false },
      { 15, t35AmbientConfig.gpio15Ambient, t35AmbientConfig.gpio15Actor, false },
      { 16, t35AmbientConfig.gpio16Ambient, t35AmbientConfig.gpio16Actor, false },
    };
    for (auto& p : flexPins) {
      if (p.ambient) {
        pinMode(p.gpio, OUTPUT);
        digitalWrite(p.gpio, HIGH); // Display is on at startup
        Serial.printf("[AMBIENT LIGHT] GPIO %d initialized (synced with display backlight)\n", p.gpio);
      } else if (p.actor) {
        if (t35AmbientConfig.isServoGpio(p.gpio)) {
          // Servo channel: attached/positioned by initServos(), not driven as a relay GPIO
          Serial.printf("[FLEX] GPIO %d: servo — attached separately\n", p.gpio);
        } else {
          pinMode(p.gpio, OUTPUT);
          digitalWrite(p.gpio, LOW);
          Serial.printf("[FLEX] GPIO %d initialized as OUTPUT LOW (relay)\n", p.gpio);
        }
      } else {
        Serial.printf("[FLEX] GPIO %d: off/sensor — skipped\n", p.gpio);
      }
    }
  }
  #endif

  // Servo motor initialization (Servo multi-channel mode or headless servo mode)
  if (multiChannelConfig.mode == "servo" || multiChannelConfig.mode == "servo180" || multiChannelConfig.mode == "servo360") {
    initServos();
  }
#ifdef BOARD_ESP32C3_21_1
  // C3: init flex channel servos independently of multiChannelConfig.mode
  else if (c3FlexConfig.gpio6Servo180 || c3FlexConfig.gpio6Servo360 ||
           c3FlexConfig.gpio7Servo180 || c3FlexConfig.gpio7Servo360) {
    initServos();
  }
#endif
#ifdef BOARD_JC3248W535C
  // Touch 3.5: init per-channel servos (multi-channel mode is "duo"/"off", not "servo*")
  else {
    bool anyT35Servo = false;
    for (int i = 0; i < 6; i++) if (t35AmbientConfig.servo[i].isServo()) anyT35Servo = true;
    if (anyT35Servo) initServos();
  }
#endif

#if !ENABLE_DISPLAY
  // Vending machine sensor / relay-output init (headless — GPIO 22/23)
  // MUST be AFTER readFiles() because lightBarrierConfig is set during config parsing
  #ifdef PIN_SENSOR_1
  if (lightBarrierConfig.isActive()) {
    pinMode(PIN_SENSOR_1, INPUT_PULLUP);
    Serial.println("[SENSOR] GPIO 22 initialized (sensor 1, active LOW)");
  } else if (lightBarrierConfig.relayOutput) {
    pinMode(PIN_SENSOR_1, OUTPUT);
    digitalWrite(PIN_SENSOR_1, LOW);
    Serial.println("[RELAY] GPIO 22 initialized (relay output, synced with Pin 12)");
  }
  #endif
  #ifdef PIN_SENSOR_2
  if (lightBarrierConfig.isActive2()) {
    pinMode(PIN_SENSOR_2, INPUT_PULLUP);
    Serial.println("[SENSOR] GPIO 23 initialized (sensor 2, active LOW)");
  } else if (lightBarrierConfig.relayOutput2) {
    pinMode(PIN_SENSOR_2, OUTPUT);
    digitalWrite(PIN_SENSOR_2, LOW);
    Serial.println("[RELAY] GPIO 23 initialized (relay output, synced with Pin 12)");
  }
  #endif

  // Headless ESP32 Dev: initialize all 10 relay channels to LOW at startup.
  // CH01=GPIO12, CH02=GPIO13, CH03=GPIO14, CH04=GPIO16 (NOT 10/11 – internal flash!)
  // CH05-CH10: GPIO 19, 22, 23, 25, 26, 27  |  CH11=GPIO32, CH12=GPIO33
  for (int i = 0; i < RELAY_CHANNEL_MAX; i++) {
    int p = RELAY_CHANNEL_PINS[i];
    // Skip Pin 12 when it's used as a servo (servo180/servo360 mode)
    if ((multiChannelConfig.mode == "servo180" || multiChannelConfig.mode == "servo360") && p == 12) {
      Serial.println("[RELAY] Skipping GPIO 12 (used as servo)");
      continue;
    }
    // Skip ambient-light pins on Touch 3.5 (JC3248W535C) — initialized separately below
    #ifdef BOARD_JC3248W535C
    if ((p == 6  && t35AmbientConfig.gpio6Ambient)  ||
        (p == 7  && t35AmbientConfig.gpio7Ambient)  ||
        (p == 14 && t35AmbientConfig.gpio14Ambient) ||
        (p == 15 && t35AmbientConfig.gpio15Ambient) ||
        (p == 16 && t35AmbientConfig.gpio16Ambient)) {
      Serial.printf("[RELAY] Skipping GPIO %d (ambient-light mode)\n", p);
      continue;
    }
    #endif
    // Skip GPIO 22/23 when configured as sensor input or relay output (already initialized above)
    if ((lightBarrierConfig.isActive() || lightBarrierConfig.relayOutput) && p == PIN_SENSOR_1) {
      Serial.printf("[RELAY] Skipping GPIO %d (configured as sensor/relay-output)\n", p);
      continue;
    }
    if ((lightBarrierConfig.isActive2() || lightBarrierConfig.relayOutput2) && p == PIN_SENSOR_2) {
      Serial.printf("[RELAY] Skipping GPIO %d (configured as sensor/relay-output)\n", p);
      continue;
    }
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  Serial.println("[RELAY] Relay channels initialized");

  // ESP32-C3-21-1: initialize GPIO6/GPIO7 flex channels based on configured mode
  #ifdef BOARD_ESP32C3_21_1
  // GPIO5 — status LED (initialized as OUTPUT, driven by existing LED logic)
  pinMode(PIN_LED_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_LED_BUTTON_LED, LOW);
  Serial.printf("[RELAY] GPIO%d (status LED) initialized\n", PIN_LED_BUTTON_LED);
  // GPIO6 / GPIO7 — mode-dependent
  auto initFlexPin = [](int gpio, bool isActor, bool isSensor) {
    if (isSensor) {
      pinMode(gpio, INPUT_PULLUP);
      Serial.printf("[FLEX] GPIO%d configured as INPUT_PULLUP (sensor)\n", gpio);
    } else if (isActor) {
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, LOW);
      Serial.printf("[FLEX] GPIO%d configured as OUTPUT (actor)\n", gpio);
    } else {
      Serial.printf("[FLEX] GPIO%d: no function (skipped)\n", gpio);
    }
  };
  initFlexPin(PIN_FLEX_CH01,
    c3FlexConfig.gpio6Relay,  // servo pins already configured by initServos() — skip OUTPUT override
    c3FlexConfig.gpio6SensorStop || c3FlexConfig.gpio6SensorMonitor || c3FlexConfig.gpio6SensorLevel);
  initFlexPin(PIN_FLEX_CH02,
    c3FlexConfig.gpio7Relay,  // servo pins already configured by initServos() — skip OUTPUT override
    c3FlexConfig.gpio7SensorStop || c3FlexConfig.gpio7SensorMonitor || c3FlexConfig.gpio7SensorLevel);
  #endif
#endif

  initDisplayMutex(); // MUST be called before any display function (thread-safe SPI)
  i2cBusInit();       // MUST be called before touch.begin() / NFC init (shared I2C bus)
  initDisplay();
  startupScreen();

#ifdef BOARD_JC3248W535C
  // The AXS15231B touch lives on Wire1 (internal SDA=4/SCL=8). Bring up the
  // external Wire (SDA=18/SCL=17) ourselves so NFC modules work even if touch
  // probe fails.
  Wire.begin(18, 17, 100000);
#endif
#ifdef BOARD_ESP32C3_21_1
  // ESP32-C3-21-1: I2C on GPIO20 (SDA) / GPIO21 (SCL) via pin header.
  // No touch controller on this board — only Wire bus init.
  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL, 100000);
#else
  // Other boards: touch controller initializes the Wire bus via touch.begin().
  touchState.available = touch.begin();
  if (touchState.available) {
    Serial.println("[TOUCH] ✓ Touch controller initialized successfully!");
  } else {
    Serial.println("[TOUCH] ✗ Touch controller NOT available (non-touch version)");
  }
#endif  // BOARD_ESP32C3_21_1 / else

  // IOExpander init: Wire (SDA=18, SCL=17) is now ready after touch.begin()
#if ENABLE_DISPLAY
  initIOExpander();
#endif

  // GPIO 3 (T-Display-S3) / GPIO 46 (JC3248W535C) / GPIO 34 (headless ESP32 Dev) — FD (Field Detection) for NT3H2111
#ifdef PIN_GPIO3
  pinMode(PIN_GPIO3, PIN_GPIO3_MODE);
  LOG_INFO("Setup", String("GPIO ") + PIN_GPIO3 + " configured as FD (Field Detection) for NT3H2111");
#endif

  // NFC init is deferred into the startup screen (2 s after WiFi begin) so NFC logs
  // appear cleanly in serial without being buried by boot noise.

  // I/O Expander init is deferred: runs after LNbits config is loaded (requires WiFi).
  // This prevents I2C bus activity during WiFi association.

  // CRITICAL: Start button task BEFORE WiFi setup so config mode works during reconnect!
  leftButton.setPressMs(3000); // 3 s hold triggers config mode internally; WiFi teardown adds ~2.7 s → config screen appears after ~5.7 s total (documented as "hold 5 seconds")
  leftButton.setDebounceMs(50); // 50ms debounce - fast response
  leftButton.attachClick(onNextButtonClick);        // Single click = Navigate / exit config
  leftButton.attachLongPressStart(onNextButtonLongPress); // Long press = Config (6-click+hold = Teach in authy mode)
#if ENABLE_DISPLAY && !defined(BOARD_JC3248W535C)
  // T-Display-S3 only: physical right button on PIN_BUTTON_2 (GPIO14 on esp32dev is unconnected
  // and floats during EN reset, causing ghost presses if ticked on headless version)
  rightButton.setDebounceMs(50);
  rightButton.setClickMs(400);
  rightButton.attachClick(showHelp);           // Single click = Help
  rightButton.attachDoubleClick(reportMode);   // Double click = Report
  rightButton.attachLongPressStart(showHelp);  // Long press = Help
#endif

  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1",   /* Name of the task */
      10000,     /* Stack size in words */
      NULL,      /* Task input parameter */
      1,         /* Priority of the task (increased from 0 to 1) */
      &Task1,    /* Task handle. */
      0);        /* Core where the task should run */

  Serial.println("Button task created - config mode available");

  // From this point on, configMode() can fire on Core 0 at any time.
  // All Serial output below must check CONFIG_MODE to avoid interleaving
  // with the paced serial output from executeConfig().
  #define SETUP_PRINT(msg) do { if (!deviceState.isInState(DeviceState::CONFIG_MODE)) Serial.println(msg); } while(0)
  #define SETUP_PRINTF(...) do { if (!deviceState.isInState(DeviceState::CONFIG_MODE)) Serial.printf(__VA_ARGS__); } while(0)

  // Start WiFi connection immediately (parallel to startup screen)
  initWiFiEventHandler(); // Register event handler before WiFi.begin() so AUTH_FAIL is caught
  WiFi.mode(WIFI_STA); // Set to Station mode
  WiFi.setSleep(false); // Disable WiFi power saving for stable connection
  WiFi.setAutoReconnect(true); // Enable auto-reconnect
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  // Track if WiFi was intentionally skipped due to missing/invalid SSID
  bool ssidMissingOrInvalid = false;
  // Guard: only start WiFi if SSID is present and valid length (<= 32)
  if (wifiConfig.ssid.length() > 0 && wifiConfig.ssid.length() <= 32) {
    WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.wifiPassword.c_str());
    SETUP_PRINT("[STARTUP] WiFi connection started in background (Power Save: OFF)");
  } else {
    SETUP_PRINT("[STARTUP] Skipping WiFi.begin(): SSID missing or invalid length");
    ssidMissingOrInvalid = true;
  }

  // Show startup screen for 5 seconds.
  // NFC I2C init fires at iteration 20 (~3 s from boot) so its logs appear
  // clearly separated from boot noise — no extra delay, no slower startup.
  SETUP_PRINT("[STARTUP] Showing startup screen for 5 seconds...");
  bool nfcInitFired = false;
  for (int i = 0; i < 50; i++) { // 50 * 100ms = 5 seconds
#if ENABLE_NFC
    if (i == 20 && !nfcInitFired) {
      Serial.println("[NFC] Initializing NFC modules (auto-detect)...");
      nfcBoltCardInit();   // PN532 @ 0x24 — Bolt Card reader (IRQ: PIN_NFC_IRQ)
      nfcNT3H2111Init();   // NT3H2111 @ 0x55 — Passive NFC tag for mobile phones
      nfcInitFired = true;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(100));
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      return; // Exit silently - config serial output is paced by executeConfig()
    }
  }
  SETUP_PRINT("[STARTUP] Startup screen completed, switching to initialization screen");

  // If WiFi was intentionally skipped (no SSID configured), enter Config mode immediately
  if (ssidMissingOrInvalid) {
    SETUP_PRINT("[STARTUP] No SSID configured - entering CONFIG mode");
    configMode();
    return;
  }

  // Switch to initialization screen
  initializationScreen();

  // Continue initialization for max 20 more seconds (total 25s)
  // Exit early if all connections are successful
  SETUP_PRINT("[STARTUP] Showing initialization screen (max 20s more) while connections establish...");

  const int MAX_INIT_TIME = 200; // 200 * 100ms = 20 seconds (5s startup + 20s init = 25s total)
  bool allConnectionsReady = false;
  bool wifiStarted = true;
  bool serverChecked = false;
  unsigned long serverCheckDoneTime = 0; // when server check finished
  bool websocketStarted = false;
  unsigned long wifiConnectTime = 0; // Track when WiFi first connected
  
  for (int i = 0; i < MAX_INIT_TIME; i++) {
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check for config mode
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      return; // Exit silently - config serial output is paced by executeConfig()
    }
    
    // Step 1: Check WiFi (runs continuously until connected)
    if (!networkStatus.confirmed.wifi && WiFi.status() == WL_CONNECTED) {
      networkStatus.confirmed.wifi = true;
      wifiConnectTime = millis(); // Record when WiFi connected
      SETUP_PRINT("[STARTUP] WiFi connected!");
    }
    
    // Step 2+3: Combined Internet + Server check.
    // A successful TCP connect to lnbitsServer:443 proves both internet access
    // and server reachability in one shot — avoids opening 3-6 extra TCP
    // connections (HTTP internet-check fallbacks) that overwhelm the connection-
    // tracking table on routers like Fritzbox 7510 and cause SSL timeouts.
    // Wait 2 s after WiFi connect to allow DNS/DHCP/gateway to stabilize.
    if (networkStatus.confirmed.wifi && !serverChecked && (millis() - wifiConnectTime > 2000)) {
      SETUP_PRINT("[STARTUP] Checking Server...");
      bool serverReachable = checkServerReachability();
      serverChecked = true;
      serverCheckDoneTime = millis();
      lastInternetCheck = millis(); // delay first periodic check by 30s — avoids competing with initial BTC fetch
      if (serverReachable) {
        networkStatus.confirmed.internet = true; // server reachable → internet works
        networkStatus.confirmed.server   = true;
        SETUP_PRINT("[STARTUP] Server OK!");
      } else {
        SETUP_PRINT("[STARTUP] Server not reachable");
        if (networkStatus.errors.server < 99) networkStatus.errors.server++;
      }
    }
    
    // Step 4: Start WebSocket (once Server is confirmed and not yet started)
    // Wait 3 s after the TCP server-probe so the Fritzbox connection-tracking table
    // can release the port-443 entry before the WebSocket opens a new SSL connection.
    // Without this gap the first WS SSL handshake is typically dropped by the router,
    // adding ~26 s of unnecessary retransmit backoff.
    if (networkStatus.confirmed.server && !websocketStarted && serverCheckDoneTime > 0 && (millis() - serverCheckDoneTime > 3000)) {
      SETUP_PRINT("[STARTUP] Starting WebSocket connection...");
      if (lightningConfig.thresholdKey.length() > 0) {
        webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + lightningConfig.thresholdKey);
      } else {
        webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
      }
      webSocket.onEvent(webSocketEvent);
      webSocket.setReconnectInterval(1000);
      websocketStarted = true;
    }
    
    // Step 5: Process WebSocket events and check connection
    if (websocketStarted && !labelsValidationAttempted) {
      webSocket.loop(); // Process events (this triggers WebSocket event handler and fetchSwitchLabels)
      
      // Only log once when WebSocket connects (not every loop iteration)
      static bool wsConnectLogged = false;
      if (webSocket.isConnected() && !wsConnectLogged) {
        SETUP_PRINT("[STARTUP] WebSocket TCP connected, waiting for config validation...");
        wsConnectLogged = true;
      }
    }
    
    // Check if validation is complete and successful
    if (networkStatus.confirmed.wifi && networkStatus.confirmed.internet && 
        networkStatus.confirmed.server && networkStatus.confirmed.websocket && 
        labelsLoadedSuccessfully && labelsValidationAttempted) {
      allConnectionsReady = true;
      SETUP_PRINTF("[STARTUP] All connections ready after %.1f seconds!\n", (i + 1) * 0.1);
      break; // Exit startup screen early
    }
    
    // Check if validation is complete but failed (404 error)
    if (labelsValidationAttempted && !labelsLoadedSuccessfully) {
      SETUP_PRINT("[STARTUP] Config validation failed - device ID invalid or deleted");
      break; // Exit startup screen to show error
    }
    
    // Progress indicator every 5 seconds
    if ((i + 1) % 50 == 0) {
      SETUP_PRINTF("[STARTUP] Progress: %.1fs - WiFi:%d Internet:%d Server:%d WS:%d Validated:%d Success:%d\n", 
                    (i + 1) * 0.1, networkStatus.confirmed.wifi, networkStatus.confirmed.internet, 
                    networkStatus.confirmed.server, networkStatus.confirmed.websocket, 
                    labelsValidationAttempted, labelsLoadedSuccessfully);
    }
  }
  
  SETUP_PRINT("[STARTUP] Startup screen completed");
  
  // CRITICAL: Don't proceed if config mode was triggered during startup
  // Config mode runs on Core 0, setup() runs on Core 1 - race condition possible
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    return; // Let config mode run on Core 0, don't interfere from Core 1
  }
  
  // Determine what to show after startup screen
  if (allConnectionsReady) {
    SETUP_PRINT("[STARTUP] All connections successful - ready to show QR code");
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
  } else {
    // Show appropriate error screen based on what failed (priority order)
    if (!networkStatus.confirmed.wifi) {
      SETUP_PRINT("[STARTUP] WiFi failed - showing WiFi error");
      wifiReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 1;
      onErrorScreen = true;
      if (networkStatus.errors.wifi < 99) networkStatus.errors.wifi++;
      
      // Handle WiFi failure
      if (wifiConfig.ssid.length() == 0) {
        configMode();
        return;
      }
      // Don't call checkAndReconnectWiFi here - it will be called below
      // This allows the loop to continue and handle touch/buttons
    } else if (!networkStatus.confirmed.internet) {
      SETUP_PRINT("[STARTUP] Internet failed - showing Internet error");
      internetReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 2;
      onErrorScreen = true;
    } else if (!networkStatus.confirmed.server) {
      SETUP_PRINT("[STARTUP] Server failed - showing Server error");
      serverReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 3;
      onErrorScreen = true;
    } else if (!networkStatus.confirmed.websocket) {
      SETUP_PRINT("[STARTUP] WebSocket failed - showing WebSocket error");
      websocketReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 4;
      onErrorScreen = true;
      if (networkStatus.errors.websocket < 99) networkStatus.errors.websocket++;
      
      // Start WebSocket if not yet started
      if (!websocketStarted) {
        if (lightningConfig.thresholdKey.length() > 0) {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + lightningConfig.thresholdKey);
        } else {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
        }
        webSocket.onEvent(webSocketEvent);
        webSocket.setReconnectInterval(1000);
      }
    }
  }

  // Button task already created earlier (before WiFi setup)
  
  // Set maxProducts based on multiChannelConfig.mode mode
  if (multiChannelConfig.mode == "quattro") {
    maxProducts = 4;
    SETUP_PRINT("[MULTI-CHANNEL-CONTROL] Quattro mode - 4 products available");
  } else if (multiChannelConfig.mode == "duo") {
    // Touch 3.5: use the exact count of relay/servo channels instead of hardcoded 2
    #ifdef BOARD_JC3248W535C
    maxProducts = t35AmbientConfig.oneForAll ? 1 : t35AmbientConfig.paymentChannelCount;
    SETUP_PRINT("[MULTI-CHANNEL-CONTROL] Touch 3.5 Multi-channel — " + String(maxProducts)
                + " product(s)" + (t35AmbientConfig.oneForAll ? " (One for All — CH01 triggers all)" : ""));
    Serial.println("[OFA-DEBUG] oneForAll=" + String(t35AmbientConfig.oneForAll ? "YES" : "NO")
                   + " paymentCh=" + String(t35AmbientConfig.paymentChannelCount)
                   + " gpio6Actor=" + String(t35AmbientConfig.gpio6Actor ? "Y" : "N")
                   + " — If OFA=NO but expected YES: re-enter config mode and set Activation Options to 'One for All'");
    #else
    maxProducts = 2;
    SETUP_PRINT("[MULTI-CHANNEL-CONTROL] Duo mode - 2 products available");
    #endif
  } else if (multiChannelConfig.mode == "servo") {
    maxProducts = servoConfig.activeChannelCount();
    SETUP_PRINT("[MULTI-CHANNEL-CONTROL] Servo mode - " + String(maxProducts) + " channel(s) available");
  } else {
    maxProducts = 1;
    SETUP_PRINT("[MULTI-CHANNEL-CONTROL] Single mode - 1 product");
  }
  
  // Initialize product navigation
  multiChannelConfig.currentProduct = 0; // Start at selection screen

#if ENABLE_BITCOIN_DATA
  // BTC data is fetched lazily by updateBitcoinTicker() / the periodic updater
  // in loop(). Fetching synchronously in setup() blocked for 20+ s on SSL
  // timeouts and prevented touch from responding on the product selection screen.
  bitcoinData.lastUpdate = 0; // force immediate refresh on first loop() iteration
  SETUP_PRINT("[BTC] Initial BTC fetch deferred to loop() — setup() unblocked");
#else
  SETUP_PRINT("[BTC] Bitcoin data fetching disabled (headless mode)");
#endif
  
  // Single mode: show BTC ticker immediately after setup (no product selection exists)
  SETUP_PRINTF("[DEBUG_SETUP] mode=%s, tickerMode=%s, special=%s, thresholdKeyLen=%d, errorState=%d\n",
                multiChannelConfig.mode.c_str(),
                multiChannelConfig.btcTickerMode.c_str(),
                specialModeConfig.mode.c_str(),
                (int)lightningConfig.thresholdKey.length(),
                deviceState.isInState(DeviceState::ERROR_RECOVERABLE));
  if (lightningConfig.thresholdKey.length() == 0 &&
      multiChannelConfig.mode == "off" &&
      !deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
    if (miniPosConfig.enabled) {
      // QR/NFC content is the invoice, created on demand; idle tag carries the
      // project URL. With ticker "always" the ticker acts as screensaver.
      miniPosIdleNfcTag();
      if (multiChannelConfig.btcTickerMode == "always") {
        SETUP_PRINT("[STARTUP] Mini-PoS mode (ticker ALWAYS) - showing BTC ticker");
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
      } else {
        SETUP_PRINT("[STARTUP] Mini-PoS mode - showing amount entry screen");
        miniPosState.inputActive = true;
        miniPosState.lastInputActivity = millis();
        showMiniPosInputScreen();
        multiChannelConfig.btcTickerActive = false;
      }
      productSelectionState.showTime = 0;
    } else if (authyConfig.enabled) {
      // Authy: idle on the IDENTITY TRIGGER start screen (or BTC ticker). The
      // single-use auth LNURL is only fetched when the user opens the QR screen
      // by touch — this keeps the NT3H tag from being rewritten every ~120 s.
      SETUP_PRINT("[STARTUP] Authy mode - showing IDENTITY TRIGGER start screen");
      authyShowStart();
      productSelectionState.showTime = 0;
    } else if (multiChannelConfig.btcTickerMode == "always") {
      SETUP_PRINT("[STARTUP] Single mode (ALWAYS) - showing Bitcoin ticker immediately");
      ensureQrForPin(RELAY_CHANNEL_PINS[0]); // pre-generate LNURL so NT3H writes immediately
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis();
    } else {
      SETUP_PRINT("[STARTUP] Single mode (SELECTING/OFF) - showing QR screen");
      // Show QR for single mode
      ensureQrForPin(RELAY_CHANNEL_PINS[0]);
      showQRScreen();
      multiChannelConfig.btcTickerActive = false;
      productSelectionState.showTime = 0; // No ticker timeout active
    }
    deviceState.transition(DeviceState::READY);
  } else if (multiChannelConfig.mode == "modeselect") {
    // Mode selection at startup: show the selection screen and let loop() handle
    // the button tap. WiFi connects in the background while the user chooses.
    SETUP_PRINT("[STARTUP] Mode-select screen — waiting for user to pick a mode");
    showModeSelectionScreen();
    deviceState.transition(DeviceState::READY);
  }

  // Setup complete - device state already set appropriately above
  #undef SETUP_PRINT
  #undef SETUP_PRINTF
}

// Punkt 3: forward declaration for modularized payment handler
void processPaymentEvent(String &payloadStr);

// ============================================================================
// MINI-POS HELPERS (Touch 3.5 — amount entry → invoice → QR/NFC payment)
// ============================================================================

// While no invoice is pending, the NT3H tag carries the project URL
// (MINIPOS_IDLE_TAG_URL) instead of an empty NDEF — a phone tap on the idle
// device opens the website. nfcNT3H2111UpdateIfChanged() performs the write.
static void miniPosIdleNfcTag() {
  strncpy(lightningConfig.lightning, MINIPOS_IDLE_TAG_URL,
          sizeof(lightningConfig.lightning) - 1);
  lightningConfig.lightning[sizeof(lightningConfig.lightning) - 1] = '\0';
}

// Reset all Mini-PoS state and show the (empty) amount entry screen.
static void miniPosShowInput() {
  miniPosState.resetInvoice();
  miniPosState.resetInput();
  miniPosState.inputActive = true;
  miniPosState.lastInputActivity = millis();
  miniPosIdleNfcTag();
  showMiniPosInputScreen();
}

// Normalize the entered amount for the API call:
//   decimal=YES: "5" → "5.00", "5.5" → "5.50", "5." → "5.00"
//   decimal=NO:  integer string used as-is
static String miniPosNormalizeAmount() {
  String a = String(miniPosState.amount);
  if (!miniPosConfig.decimal) return a;
  int dot = a.indexOf('.');
  if (dot < 0) return a + ".00";
  int decimals = a.length() - dot - 1;
  if (decimals == 0) return a + "00";
  if (decimals == 1) return a + "0";
  return a;
}

static void miniPosShowInfo(const String &msg) {
  miniPosState.infoMsg = msg;
  miniPosState.infoUntil = millis() + MINIPOS_INFO_MS;
  showMiniPosInputScreen();
}

// ============================================================================
// AUTHY SCREEN FLOW (Touch 3.5 — LNURL-auth identity trigger)
// ============================================================================

// Idle/start screen: "IDENTITY TRIGGER" (or the BTC ticker if preselected).
// No auth LNURL is fetched here and the NT3H tag carries the project URL, so
// the tag is NOT rewritten every ~120 s — only when the QR screen is opened.
void authyShowStart() {
  authyState.qrShown   = false;
  authyState.qrShownAt = 0;
  // Dual-page mode: when the payment page is active, show the classic ZapBox
  // product QR for the auth pin (LNURL + NFC tag from ensureQrForPin) with a
  // "< ID" tab back to the identity page.
  if (authyConfig.dualPage && authyState.payPage) {
    ensureQrForPin(authyConfig.authPin);   // generates LNURL and writes NT3H tag
    int idx = getPinIndex(authyConfig.authPin);
    String lbl = (idx >= 0 && productLabels.labels[idx].length() > 0)
                   ? productLabels.labels[idx]
                   : String("Pin ") + String(authyConfig.authPin);
    showAuthPayScreen(lbl, authyConfig.authPin);
    multiChannelConfig.btcTickerActive = false;
    return;
  }
  miniPosIdleNfcTag();   // idle NFC: project URL, not a (soon-stale) auth k1
  if (multiChannelConfig.btcTickerMode == "always") {
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
  } else {
    identityTriggerScreen();
    multiChannelConfig.btcTickerActive = false;
  }
}

// Open the identity-trigger QR: fetch a fresh single-use auth LNURL (this also
// updates the NT3H tag) and show it with the configured label. Idle for
// PRODUCT_TIMEOUT returns to authyShowStart().
static void authyShowQR() {
  int httpCode = 0;
  if (requestAuthLnurl(nullptr, &httpCode)) {
    // Dual-page: identity QR carries a "pay login >" tab to the payment page.
    if (authyConfig.dualPage)
      showAuthIdentityScreen(authyConfig.label, authyConfig.authPin);
    else
      showProductQRScreen(authyConfig.label, authyConfig.authPin);
    authyState.qrShown   = true;
    authyState.qrShownAt = millis();
    authyState.payPage   = false;
    multiChannelConfig.btcTickerActive = false;
  } else if (httpCode == 403) {
    // Identities disabled server-side: show a red hint instead of silently
    // ignoring the touch, then auto-return to the start screen.
    LOG_WARN("Authy", "Identity login disabled (HTTP 403) - showing hint");
    authIdentityDisabledScreen();
    authyState.qrShown   = false;
    authyState.payPage   = false;
    authyState.infoUntil = millis() + 4000;
    multiChannelConfig.btcTickerActive = false;
  }
}

// Called by navigateToNextProduct() on T-Display-S3 (no touch) when authy mode
// is active. NEXT button cancels teach mode (if active) or cycles between pages.
void authyHandleNextButton() {
  activityTracking.lastActivityTime = millis();

  // NEXT during teach mode = cancel teach session and restart
  if (authyState.teachActive) {
    LOG_INFO("Teach", "NEXT during teach mode — canceling teach session");
    endAuthTeach();
    return;
  }

  if (!authyState.qrShown && !authyState.payPage) {
    LOG_INFO("Authy", "NEXT on start screen — opening identity QR");
    authyShowQR();
  } else if (authyConfig.dualPage) {
    if (authyState.payPage) {
      LOG_INFO("Authy", "NEXT — back to IDENTITY login QR");
      authyState.payPage = false;
      authyShowQR();
    } else {
      LOG_INFO("Authy", "NEXT — to PAYMENT page");
      authyState.payPage = true;
      authyState.qrShown = false;
      authyShowStart();
    }
  } else {
    // Single page: keep QR alive (refresh timer)
    authyState.qrShownAt = millis();
  }
}

// ============================================================================
// MODE SELECTION HELPER
// ============================================================================
// Called when the user taps a button on the mode selection screen.
// Applies the chosen mode in memory (no flash write) and shows its startup
// screen. No restart needed — all configs were loaded from flash at boot.
// selected: 1=Single, 2=Multi-channel, 3=Mini-PoS, 4=Authy
static void applyModeSelection(int selected) {
  LOG_INFO("ModeSelect", "Mode selected: " + String(selected));
  miniPosConfig.enabled  = false;
  authyConfig.enabled    = false;

  switch (selected) {
    case 1: // Single channel
      multiChannelConfig.mode = "off";
      maxProducts = 1;
      multiChannelConfig.currentProduct = 0;
      ensureQrForPin(RELAY_CHANNEL_PINS[0]);
      showQRScreen();
      productSelectionState.showTime = 0;
      LOG_INFO("ModeSelect", "Single channel started");
      break;

    case 2: // Multi-channel
      multiChannelConfig.mode = "duo";
      #ifdef BOARD_JC3248W535C
      maxProducts = t35AmbientConfig.oneForAll ? 1 : t35AmbientConfig.paymentChannelCount;
      #else
      maxProducts = 2;
      #endif
      multiChannelConfig.currentProduct = 0;
      productSelectionScreen();
      productSelectionState.showTime = millis();
      LOG_INFO("ModeSelect", "Multi-channel started — " + String(maxProducts) + " product(s)");
      break;

    case 3: // Mini-PoS
      multiChannelConfig.mode = "off";
      miniPosConfig.enabled   = true;
      maxProducts = 1;
      miniPosIdleNfcTag();
      miniPosState.resetInput();
      miniPosState.inputActive = true;
      miniPosState.lastInputActivity = millis();
      showMiniPosInputScreen();
      productSelectionState.showTime = 0;
      LOG_INFO("ModeSelect", "Mini-PoS started");
      break;

    case 4: // Authy
      multiChannelConfig.mode = "off";
      authyConfig.enabled     = true;
      maxProducts = 1;
      if (multiChannelConfig.btcTickerMode != "always") {
        multiChannelConfig.btcTickerMode = "off";
      }
      if (!authyConfig.dualPage) {
        lightningConfig.thresholdKey = "";
      }
      authyShowStart();
      productSelectionState.showTime = 0;
      LOG_INFO("ModeSelect", "Authy started");
      break;

    default:
      return;
  }

  deviceState.transition(DeviceState::READY);
}

// ============================================================================
// AUTHY TEACH HELPERS (Touch 3.5 — LNURL-auth identity enrolment)
// ============================================================================

// Open the 6-digit teach PIN pad. Touch 3.5": triggered by 6-tap+hold gesture.
// T-Display-S3: called automatically on boot when authyConfig.teachPin is set.
void startAuthTeachEntry() {
#ifdef BOARD_JC3248W535C
  // Touch 3.5": show 6-digit PIN pad; submit is triggered when all digits entered.
  pinPadState = PinPadState();
  pinPadState.active      = true;
  pinPadState.teachMode   = true;
  pinPadState.maxDigits   = 6;
  pinPadState.maxAttempts = 3;
  pinPadState.activatedAt = millis();
  extensionConfig.nfcPaymentPending = false;
  showPinPadScreen(pinPadState);
  LOG_INFO("Teach", "Teach PIN entry opened (6 digits)");
#else
  // T-Display-S3: no PIN pad — PIN comes from installer config (authyConfig.teachPin).
  authyState.pendingTeachStart = true;
  LOG_INFO("Teach", "T-Display-S3: pending teach start with installer PIN");
#endif
}

// End an active teach session (user cancel / timeout).
// Touch 3.5": returns to the identity start screen.
// T-Display-S3: restarts the device to clear the one-time teach PIN from RAM.
static void endAuthTeach() {
  stopTeachSession();
  authyState.reset();
#ifdef BOARD_JC3248W535C
  authyShowStart();
  LOG_INFO("Teach", "Teach session ended - back to start screen");
#else
  LOG_INFO("Teach", "Teach session ended — restarting to clear teach PIN");
  delay(500);
  ESP.restart();
#endif
}

#ifdef BOARD_JC3248W535C
// ============================================================================
// NUMERICAL PRODUCT SELECTION HELPERS (Touch 3.5 multi-channel)
// Keypad panel → GPIO number → product QR. While no product QR is shown the
// NT3H tag carries the project URL (like Mini-PoS idle) and the PN532 ignores
// Bolt Card taps.
// ============================================================================

// True when the GPIO/virtual pin is configured as a payment actor on this
// device (relay or servo channel, or PCF8574 virtual pin with expander on).
static bool numericPinSelectable(int pin) {
  switch (pin) {
    case 5:  return true;  // CH01 is always a payment actor (relay or servo)
    case 6:  return t35AmbientConfig.gpio6Actor;
    case 7:  return t35AmbientConfig.gpio7Actor;
    case 14: return t35AmbientConfig.gpio14Actor;
    case 15: return t35AmbientConfig.gpio15Actor;
    case 16: return t35AmbientConfig.gpio16Actor;
    default: break;
  }
  if (pin >= 200 && pin <= 207) return ioExpanderConfig.enabled;
  return false;
}

// Reset input state and show the keypad panel.
static void numericShowPanel() {
  productSelectState.resetInput();
  productSelectState.panelActive = true;
  productSelectState.qrActive = false;
  productSelectState.qrPin = -1;
  productSelectState.lastActivity = millis();
  multiChannelConfig.btcTickerActive = false;
  miniPosIdleNfcTag(); // panel shows no QR — tag carries the project URL
  showProductSelectScreen();
}

// Return to the main screen: product selection screen, or the BTC ticker
// when ticker mode "always" is configured.
static void numericShowMainScreen() {
  productSelectState.resetAll();
  miniPosIdleNfcTag();
  if (multiChannelConfig.btcTickerMode == "always") {
    multiChannelConfig.currentProduct = 0;
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
  } else {
    multiChannelConfig.currentProduct = -1;
    multiChannelConfig.btcTickerActive = false;
    productSelectionScreen();
    deviceState.transition(DeviceState::PRODUCT_SELECTION);
  }
}

// GO pressed: validate the entered number against the device channel config
// and the switches loaded from LNbits; show the product QR or an error.
static void numericHandleGo() {
  int pin = (productSelectState.numDigits > 0)
            ? String(productSelectState.digits).toInt() : -1;
  int idx = (pin > 0) ? getPinIndex(pin) : -1;
  bool known = (pin > 0) && numericPinSelectable(pin) &&
               labelsLoadedSuccessfully && idx >= 0 &&
               (productLabels.durations[idx] > 0 ||
                productLabels.labels[idx].length() > 0);
  if (!known) {
    LOG_INFO("NumSel", String("Product ") + String(productSelectState.digits) + " not available");
    productSelectState.infoMsg = "Product not available";
    productSelectState.infoUntil = millis() + 3000;
    showProductSelectScreen();
    return;
  }
  String label = (productLabels.labels[idx].length() > 0)
                 ? productLabels.labels[idx] : "Pin " + String(pin);
  ensureQrForPin(pin); // also updates the NT3H tag content
  productSelectState.panelActive = false;
  productSelectState.qrActive = true;
  productSelectState.qrPin = pin;
  productSelectState.qrShownAt = millis();
  productSelectState.infoMsg = "";
  productSelectState.infoUntil = 0;
  showProductSelectQRScreen(label, pin);
  LOG_INFO("NumSel", String("Showing product QR for pin ") + String(pin) + " (" + label + ")");
}
#endif // BOARD_JC3248W535C

void loop()
{
  // Wait for setup to complete before running loop
  if (deviceState.getState() == DeviceState::INITIALIZING)
  {
    updateReadyLed(); // Fast blink during init
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
  
  // CRITICAL: Block loop() while config mode is active (runs on separate thread/core)
  // Without this check, loop() continues writing to config.json while serial config reads/writes it
  // This causes race condition and JSON corruption like: {"name":"qrFormat","vathresholdAmount","value":""}
  if (deviceState.getState() == DeviceState::CONFIG_MODE)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }

  // Update ready LED state (after CONFIG_MODE check to avoid LED race condition with config blink)
  updateReadyLed();

  // Once loop is running, we are past init screens
  if (initializationActive && !firstLoop) {
    initializationActive = false;
    updateReadyLed();
  }
  
  // Display power saving status on first loop iteration
  static bool firstLoopStatusShown = false;
  if (!firstLoopStatusShown) {
    firstLoopStatusShown = true;
    Serial.println("\n================================");
    Serial.println("   POWER SAVING STATUS");
    Serial.println("================================");
    
    // Display powerConfig.screensaver status with clear description
    if (powerConfig.screensaver == "backlight") {
      Serial.println("Screensaver: backlight off");
    } else {
      Serial.println("Screensaver: " + powerConfig.screensaver);
    }
    
    // Display deep sleep status with clear description
    if (powerConfig.deepSleep == "freeze") {
      Serial.println("Deep Sleep: deep sleep (freeze) mode");
    } else if (powerConfig.deepSleep == "light") {
      Serial.println("Deep Sleep: light sleep mode");
    } else {
      Serial.println("Deep Sleep: " + powerConfig.deepSleep);
    }
    
    Serial.println("Screensaver Time: " + String(powerConfig.activationTimeoutMs / 60000) + " minutes (" + String(powerConfig.activationTimeoutMs) + " ms)");
    Serial.println("Deep Sleep Time: " + String(powerConfig.deepSleepTimeoutMs / 60000) + " minutes (" + String(powerConfig.deepSleepTimeoutMs) + " ms)");
    Serial.println("screensaverActive: " + String(deviceState.isInState(DeviceState::SCREENSAVER)));
    Serial.println("deepSleepActive: " + String(deviceState.isInState(DeviceState::DEEP_SLEEP)));
    Serial.println("activityTracking.lastActivityTime: " + String(activityTracking.lastActivityTime));
    
    if (powerConfig.screensaver != "off" && powerConfig.deepSleep != "off") {
      Serial.println("\n⚡ MODE: SCREENSAVER + DEEP SLEEP (" + powerConfig.deepSleep + ")");
      Serial.println("   Screensaver first, then deep sleep after longer inactivity");
    } else if (powerConfig.screensaver != "off") {
      Serial.println("\n⚡ MODE: SCREENSAVER ENABLED");
      Serial.println("   Backlight will turn off after inactivity");
    } else if (powerConfig.deepSleep != "off") {
      Serial.println("\n⚡ MODE: DEEP SLEEP ENABLED (" + powerConfig.deepSleep + ")");
      Serial.println("   Device will sleep after inactivity");
    } else {
      Serial.println("\n⚡ MODE: POWER SAVING DISABLED");
    }
    Serial.println("================================\n");
  }
  
  // Screensaver and deep sleep checks are now inside the payment wait loop
  // to ensure they execute during payment waiting
  
  // If in config mode, do nothing - config is handled by SerialConfig
  if (deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
  
  checkAndReconnectWiFi();
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) return;
  
  // Handle QR redraw after WiFi recovery (outside of deep call stack)
  if (needsQRRedraw) {
    Serial.println("[RECOVERY] Redrawing QR screen after WiFi recovery");
    redrawQRScreen();
    needsQRRedraw = false;
  }

  // Re-check: config mode may have been triggered while we were busy above
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) return;

  payloadStr = "";
  
  // CRITICAL: Only show QR screen ONCE on first loop if ALL connections confirmed
  bool allConnectionsConfirmed = networkStatus.confirmed.wifi && networkStatus.confirmed.internet && networkStatus.confirmed.server && networkStatus.confirmed.websocket;
  
  // If ticker is already active from setup in Single mode, don't override it
  if (firstLoop && multiChannelConfig.mode == "off" && multiChannelConfig.btcTickerActive && !deviceState.isInState(DeviceState::REPORT_SCREEN)) {
    Serial.println("[FIRSTLOOP] Ticker already active in single-mode - skipping redraw");
    productSelectionState.showTime = millis();
    deviceState.transition(DeviceState::READY);
  }
  else if (firstLoop && allConnectionsConfirmed && !deviceState.isInState(DeviceState::REPORT_SCREEN) && !(powerConfig.lastWakeUpTime > 0 && (millis() - powerConfig.lastWakeUpTime) < GRACE_PERIOD_MS)) {
    Serial.println("[SCREEN] All connections confirmed - selecting initial screen");
    showInitialScreenAfterConnections();
    currentErrorType = 0;
    // Start product selection timer
    productSelectionState.showTime = millis();
    // Do NOT force READY here – showInitialScreenAfterConnections() already
    // sets the correct state (PRODUCT_SELECTION for duo/quattro, READY for single).
    // Overwriting with READY breaks the product selection guard that blocks NFC.
  } else if (firstLoop && !allConnectionsConfirmed) {
    Serial.printf("[SCREEN] First loop - waiting for all connections (WiFi:%d, Internet:%d, Server:%d, WS:%d)\n", 
                  networkStatus.confirmed.wifi, networkStatus.confirmed.internet, networkStatus.confirmed.server, networkStatus.confirmed.websocket);
  }
  
  firstLoop = false; // Mark first loop as completed

  // Clear initialization flag immediately so Ready LED turns on without a one-loop delay.
  // Previously this happened at the start of the next loop() iteration, causing a brief
  // LED gap after the initial screen was shown.
  if (initializationActive) {
    initializationActive = false;
    updateReadyLed();
  }

  // Show ready message immediately after QR screen is displayed and LED is on.
  // Previously this ran at the top of loop() using isReadyForReceive(), but since
  // the inner while(true) never exits under normal conditions, the message would only
  // appear after a delayed return, sometimes 5-10 seconds after the QR code was shown.
  static bool readyMessageShown = false;
  if (!readyMessageShown && isReadyForReceive()) {
    Serial.println("");
    Serial.println("======================");
    Serial.println("   ZapBox ready! \xF0\x9F\x8E\x89");
    Serial.println("   Firmware: " VERSION);
    Serial.println("======================");
    readyMessageShown = true;
  }

  static unsigned long lastWiFiCheck = 0; // static: survives loop() re-entry from return;
  networkStatus.lastPingTime = millis(); // Initialize global variable
  unsigned long loopCount = 0;
  // Don't reset onErrorScreen/currentErrorType - they should persist across loop iterations
  
  // Final CONFIG_MODE check before entering the long-running payment wait loop
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) return;

  Serial.println("[LOOP] Entering payment wait loop...");
  Serial.printf("[LOOP] Initial queue size: %d\n", paymentQueue.size());
  
  // Initialize ping/pong tracking
  networkStatus.lastPongTime = millis();
  networkStatus.waitingForPong = false;
  
  // Debug counter for loop iterations
  static unsigned long loopIterations = 0;
  static unsigned long lastLoopDebugPrint = 0;
  
  while (true) // Continuous loop - will process payments from queue
  {
    loopIterations++;
    
    // Print debug info every 10 seconds (skip during config mode to keep serial clean)
    if (!deviceState.isInState(DeviceState::CONFIG_MODE) && millis() - lastLoopDebugPrint > 10000) {
      Serial.printf("[LOOP_DEBUG] Iterations: %lu, touchState.available: %d, onErrorScreen: %d, errorType: %d\n",
                    loopIterations, touchState.available, onErrorScreen, currentErrorType);
      lastLoopDebugPrint = millis();
    }
    // Check if config mode was triggered during payment wait
    if (deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      return; // Exit silently - config mode handles serial output
    }

    // ── NFC payment monitoring ──────────────────────────────────────────────
    // MUST be very early in the loop – the network recovery block below contains
    // many return statements that would prevent this code from ever executing
    // if it were placed later.
    #if ENABLE_NFC
    {
      // Extension mismatch: NFC tap detected but not using zapbox_extension
      if (extensionConfig.nfcExtensionMismatch) {
        extensionConfig.nfcExtensionMismatch = false;
        nfcNotSupportedScreen();
        nfcNotSupportedShown = true;
        nfcNotSupportedStart = millis();
        LOG_WARN("NFC", "NFC not supported by active extension \u2013 showing error screen for 5s");
      } else if (nfcNotSupportedShown) {
        if (millis() - nfcNotSupportedStart > 5000) {
          nfcNotSupportedShown = false;
          needsQRRedraw = true;
          LOG_INFO("NFC", "NFC not supported screen dismissed \u2013 returning to QR screen");
        }
      // HTTP POST failed \u2192 show NO LUCK immediately
      } else if (extensionConfig.nfcPaymentFailed) {
        extensionConfig.nfcPaymentFailed  = false;
        extensionConfig.nfcPaymentPending = false;
        nfcPendingScreenShown = false;
        // Blink LED 3 times to signal failure
        for (int i = 0; i < 3; i++) {
          #if PIN_LED_BUTTON_LED >= 0
          digitalWrite(PIN_LED_BUTTON_LED, HIGH);
          #endif
          #ifdef PIN_ONBOARD_LED
          digitalWrite(PIN_ONBOARD_LED, HIGH);
          #endif
          delay(100);
          #if PIN_LED_BUTTON_LED >= 0
          digitalWrite(PIN_LED_BUTTON_LED, LOW);
          #endif
          #ifdef PIN_ONBOARD_LED
          digitalWrite(PIN_ONBOARD_LED, LOW);
          #endif
          delay(100);
        }
        nfcNoLuckScreen();
        nfcNoLuckScreenShown = true;
        nfcNoLuckStart = millis();
        LOG_WARN("NFC", "NFC payment failed \u2013 showing NO LUCK screen");
      } else if (nfcNoLuckScreenShown) {
        // "NFC NO LUCK" screen active – wait 3s, then show error detail (if any) or return to QR
        if (millis() - nfcNoLuckStart > 3000) {
          nfcNoLuckScreenShown = false;
          if (extensionConfig.nfcErrorDetail[0] != '\0') {
            nfcErrorDetailScreen(extensionConfig.nfcErrorDetail);
            extensionConfig.nfcErrorDetail[0] = '\0';
            nfcErrorDetailShown = true;
            nfcErrorDetailStart = millis();
            LOG_INFO("NFC", "NO LUCK dismissed – showing error detail screen");
          } else {
            productSelectionState.showTime = millis(); // Prevent immediate product timeout after NFC failure
            needsQRRedraw = true;
            LOG_INFO("NFC", "NFC NO LUCK screen dismissed – returning to QR screen");
          }
        }
      } else if (nfcErrorDetailShown) {
        // Error detail screen active – show for 6s then return to QR
        if (millis() - nfcErrorDetailStart > 6000) {
          nfcErrorDetailShown = false;
          productSelectionState.showTime = millis(); // Prevent immediate product timeout after NFC failure
          needsQRRedraw = true;
          LOG_INFO("NFC", "NFC error detail screen dismissed – returning to QR screen");
        }
      } else if (pinPadState.active) {
        // PIN pad error auto-dismiss: 5s for retryable error, 10s for blocked card
        if (pinPadState.showError) {
          uint32_t errDur = pinPadState.blocked ? 10000 : 5000;
          if (millis() - pinPadState.errorStart > errDur) {
            if (pinPadState.blocked) {
              pinPadState.active              = false;
              nfcPendingScreenShown           = false;
              needsQRRedraw                   = true;
              productSelectionState.showTime  = millis();
              LOG_INFO("PIN", "Card blocked – returning to QR screen after 10s");
            } else {
              pinPadState.showError    = false;
              pinPadState.submitted    = false;
              pinPadState.pendingShown = false;
              showPinPadScreen(pinPadState);
              LOG_INFO("PIN", "PIN error cleared – ready for retry");
            }
          }
        } else if (pinPadState.submitted && !pinPadState.pendingShown
                   && millis() - pinPadState.submittedAt > 800) {
          // PIN sent, no error yet after 800ms → show PENDING while waiting for payment
          nfcPendingScreen();
          pinPadState.pendingShown = true;
        } else if (millis() - pinPadState.activatedAt > 210000) {
          // Device-side fallback: 210s timeout (30s over server timeout of 180s).
          // Catches the case where the server's timeout WS event is lost.
          pinPadState.active             = false;
          pinPadState.submitted          = false;
          pinPadState.pendingShown       = false;
          nfcPendingScreenShown          = false;
          needsQRRedraw                  = true;
          productSelectionState.showTime = millis();
          LOG_WARN("PIN", "PIN pad device-side timeout – returning to QR screen");
        }
      } else if (extensionConfig.nfcPaymentPending) {
        if (!nfcPendingScreenShown) {
          nfcPendingScreen();
          nfcPendingScreenShown = true;
        }
        // Timeout: if no payment confirmation arrives, show NO LUCK screen.
        // Timer runs from the card tap (set once before HTTP POST).
        // Production: 60000 ms (60s).
        if (millis() - extensionConfig.nfcPaymentPendingStart > 60000) {
          extensionConfig.nfcPaymentPending = false;
          nfcPendingScreenShown = false;
          // Blink LED 3 times to signal failure
          for (int i = 0; i < 3; i++) {
            #if PIN_LED_BUTTON_LED >= 0
            digitalWrite(PIN_LED_BUTTON_LED, HIGH);
            #endif
            #ifdef PIN_ONBOARD_LED
            digitalWrite(PIN_ONBOARD_LED, HIGH);
            #endif
            delay(100);
            #if PIN_LED_BUTTON_LED >= 0
            digitalWrite(PIN_LED_BUTTON_LED, LOW);
            #endif
            #ifdef PIN_ONBOARD_LED
            digitalWrite(PIN_ONBOARD_LED, LOW);
            #endif
            delay(100);
          }
          nfcNoLuckScreen();
          nfcNoLuckScreenShown = true;
          nfcNoLuckStart = millis();
          LOG_WARN("NFC", "NFC payment timed out \u2013 showing NO LUCK screen");
        }
      } else if (nfcPendingScreenShown) {
        nfcPendingScreenShown = false;
      }
    }
    #endif
    // \u2500\u2500\u2500 End NFC payment monitoring \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500

    // NT3H2111: re-write NDEF if the active LNURL changed (no-op when unchanged)
    #if ENABLE_NFC
    nfcNT3H2111UpdateIfChanged();
    #endif

    // ── Mini-PoS timers ──────────────────────────────────────────────────
    if (miniPosConfig.enabled) {
      uint32_t mpNow = millis();
      // Unpaid invoice expires → back to amount entry, NFC tag cleared
      if (miniPosState.invoicePending &&
          (mpNow - miniPosState.invoiceCreatedAt >= INVOICE_TIMEOUT)) {
        LOG_INFO("MiniPoS", "Invoice timed out - returning to amount entry");
        miniPosShowInput();
      }
      if (miniPosState.inputActive) {
        // "Last Pay" orange lock expires → amount stays, normal color, editable
        if (miniPosState.amountLocked && mpNow >= miniPosState.lockUntil) {
          miniPosState.amountLocked = false;
          showMiniPosInputScreen();
        }
        // Transient info message expires
        if (miniPosState.infoMsg.length() > 0 && miniPosState.infoUntil > 0 &&
            mpNow >= miniPosState.infoUntil) {
          miniPosState.infoMsg = "";
          miniPosState.infoUntil = 0;
          showMiniPosInputScreen();
        }
        // Ticker screensaver ("always" mode): entry screen idle for
        // PRODUCT_TIMEOUT → back to the BTC ticker, entered digits discarded
        if (multiChannelConfig.btcTickerMode == "always" &&
            !multiChannelConfig.btcTickerActive &&
            miniPosState.lastInputActivity > 0 &&
            mpNow - miniPosState.lastInputActivity >= PRODUCT_SELECTION_DELAY) {
          LOG_INFO("MiniPoS", "Entry screen idle - showing BTC ticker");
          miniPosState.inputActive = false;
          miniPosState.resetInput();
          btctickerScreen();
          multiChannelConfig.btcTickerActive = true;
        }
      }
    }

    // ── Ring-Login: NTAG 424 SUN tap received from NFC task ─────────────
    if (authyConfig.enabled && authyState.nfcSunTapPending &&
        !pinPadState.active) {
      authyState.nfcSunTapPending = false;
      String extId = String(authyState.nfcSunExternalId);
      String sunP  = String(authyState.nfcSunP);
      String sunC  = String(authyState.nfcSunC);

      if (authyState.nfcSunIsTeach) {
        // Teach mode: enrol the card on the server
        if (requestNfcTeach(extId, sunP, sunC)) {
          authyState.infoMsg   = "NFC card enrolled";
          authyState.infoUntil = millis() + 3000;
          showAuthToast(authyState.infoMsg, false);
          LOG_INFO("NFC-Teach", "Card enrolled OK");
        } else {
          authyState.infoMsg   = "Card not enrolled";
          authyState.infoUntil = millis() + 3000;
          showAuthToast(authyState.infoMsg, true);
          LOG_WARN("NFC-Teach", "Enrol failed (card not in tagid or session closed)");
        }
      } else if (authyConfig.ntag424Pin) {
        // PIN required: show 4-digit PIN pad
        pinPadState              = PinPadState();
        pinPadState.active       = true;
        pinPadState.nfcRingLogin = true;
        pinPadState.maxDigits    = 4;
        pinPadState.maxAttempts  = 3;
        pinPadState.activatedAt  = millis();
        showPinPadScreen(pinPadState);
        LOG_INFO("NFC-Auth", "Showing PIN pad for Ring-Login");
      } else {
        // No PIN required: verify immediately
        String errMsg;
        if (requestNfcAuth(extId, sunP, sunC, "", &errMsg)) {
          LOG_INFO("NFC-Auth", "Auth OK (no PIN)");
        } else {
          authyState.infoMsg   = errMsg.isEmpty() ? "NFC Identity Failed" : errMsg;
          authyState.infoUntil = millis() + 3000;
          showAuthToast(authyState.infoMsg, true);
          LOG_WARN("NFC-Auth", String("Auth failed: ") + errMsg);
        }
      }
    }

    // ── Authy timers ─────────────────────────────────────────────────────
    if (authyConfig.enabled &&
        deviceState.isInState(DeviceState::READY) &&
        !pinPadState.active && !extensionConfig.nfcPaymentPending) {
      uint32_t aNow = millis();
      // Teach session 5-min backup timeout (server also sends teach_ended). The
      // teach screen must NOT fall back to the ticker — only this ends it.
#ifndef BOARD_JC3248W535C
      // T-Display-S3: initiate teach session using the PIN from the installer config.
      if (authyState.pendingTeachStart && !authyState.teachActive) {
        authyState.pendingTeachStart = false;
        if (submitTeachPin(authyConfig.teachPin)) {
          authyState.teachActive = true;
          authyState.teachUntil  = millis() + AUTHY_TEACH_TIMEOUT_MS;
          authyState.infoMsg     = "Teach mode";
          if (requestAuthLnurl()) showAuthTeachScreen("Learning Identities", authyConfig.authPin);
          LOG_INFO("Teach", "T-Display-S3 teach active");
        } else {
          authyState.infoMsg   = "Teach start failed";
          authyState.infoUntil = aNow + 4000;
          showAuthToast(authyState.infoMsg, true);
          LOG_WARN("Teach", "T-Display-S3 teach start failed (check LNbits teach PIN config)");
        }
      }
#endif
      if (authyState.teachActive) {
        if ((int32_t)(aNow - authyState.teachUntil) >= 0) {
          LOG_INFO("Teach", "Teach backup timeout - ending session");
          endAuthTeach();
        } else if (authyState.needsRefresh) {
          // After a wallet enrolled: show the next register challenge.
          authyState.needsRefresh = false;
          authyState.infoMsg   = "";
          authyState.infoUntil = 0;
          if (requestAuthLnurl()) showAuthTeachScreen("Learning Identities", authyConfig.authPin);
        } else if (authyState.infoUntil > 0 && aNow >= authyState.infoUntil) {
          authyState.infoMsg   = "";
          authyState.infoUntil = 0;
          showAuthTeachScreen("Learning Identities", authyConfig.authPin);
        }
      } else if (authyState.qrShown) {
        // Identity-trigger QR idle for PRODUCT_TIMEOUT → back to start screen.
        // The QR is fetched fresh on each open, so it never outlives its k1.
        if (aNow - authyState.qrShownAt >= (uint32_t)PRODUCT_TIMEOUT) {
          LOG_INFO("Authy", "Identity QR idle - back to start screen");
          authyShowStart();
        } else if (authyState.needsRefresh) {
          authyState.needsRefresh = false;
          authyShowQR();
        } else if (authyState.infoUntil > 0 && aNow >= authyState.infoUntil) {
          authyState.infoMsg   = "";
          authyState.infoUntil = 0;
          authyShowQR();   // redraw QR cleanly without toast
        }
      } else if (authyState.infoUntil > 0) {
        // Transient hint (e.g. "IDENTITY LOGIN DISABLED") → back to start.
        if (aNow >= authyState.infoUntil) {
          authyState.infoUntil = 0;
          authyShowStart();
        }
      }
    }

    // ── Numerical product selection timers (Touch 3.5 multi-channel) ─────
    #ifdef BOARD_JC3248W535C
    if (t35AmbientConfig.numericSelect &&
        !pinPadState.active && !extensionConfig.nfcPaymentPending) {
      uint32_t nsNow = millis();
      if (productSelectState.panelActive) {
        // Transient error message expires
        if (productSelectState.infoMsg.length() > 0 && productSelectState.infoUntil > 0 &&
            nsNow >= productSelectState.infoUntil) {
          productSelectState.infoMsg = "";
          productSelectState.infoUntil = 0;
          showProductSelectScreen();
        }
        // Panel idle → back to main screen, entered digits discarded
        if (productSelectState.lastActivity > 0 &&
            nsNow - productSelectState.lastActivity >= PRODUCT_SELECTION_DELAY) {
          LOG_INFO("NumSel", "Keypad panel idle - returning to main screen");
          numericShowMainScreen();
        }
      } else if (productSelectState.qrActive) {
        // Product QR idle → back to main screen
        if (productSelectState.qrShownAt > 0 &&
            nsNow - productSelectState.qrShownAt >= PRODUCT_SELECTION_DELAY) {
          LOG_INFO("NumSel", "Product QR idle - returning to main screen");
          numericShowMainScreen();
        }
      }
    }
    #endif

    // Check for touch input (if available)
    static unsigned long lastTouchEvent = 0;
    static bool wasTouched = false;
    static bool actionExecutedThisTouch = false; // Track if action already executed for current touch
    static unsigned long lastActionTime = 0; // Track when last action was executed for debouncing
    static unsigned long lastTouchDebugPrint = 0;
    
    if (touchState.available && !deviceState.isInState(DeviceState::CONFIG_MODE)) {
#ifdef BOARD_JC3248W535C
      // JC3248W535C: PIN_TOUCH_INT == GPIO16 == PIN_NFC_IRQ — permanently LOW, cannot use it.
      // Use touch.isPressed() (I2C poll) with a 1500 ms cooldown after screensaver activation
      // to prevent phantom wake from residual touch reports.
      static unsigned long screensaverEnteredAt = 0;
      if (deviceState.isInState(DeviceState::SCREENSAVER)) {
        if (screensaverEnteredAt == 0) screensaverEnteredAt = millis();
      } else {
        screensaverEnteredAt = 0;
      }
      bool jcWakeSignal = touch.isPressed();
      bool jcCooldownOk = (screensaverEnteredAt > 0 &&
                           millis() - screensaverEnteredAt >= 1500);

      if (deviceState.isInState(DeviceState::SCREENSAVER) && (millis() - lastTouchDebugPrint > 5000)) {
        Serial.printf("[TOUCH_DEBUG] Screensaver active, isPressed=%d, cooldown=%dms\n",
                      jcWakeSignal,
                      screensaverEnteredAt > 0 ? (int)(millis() - screensaverEnteredAt) : 0);
        lastTouchDebugPrint = millis();
      }

      if (jcWakeSignal && jcCooldownOk && deviceState.isInState(DeviceState::SCREENSAVER)) {
        Serial.println("[TOUCH] Touch detected during screensaver - WAKING UP");
        deviceState.transition(DeviceState::READY);
        deactivateScreensaver();
        screensaverEnteredAt = 0;
        powerConfig.lastWakeUpTime = millis();
        activityTracking.lastActivityTime = millis();
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
#else
      // T-Display-S3: dedicated touch INT pin, original logic unchanged
      int touchIntState = digitalRead(PIN_TOUCH_INT);

      if (deviceState.isInState(DeviceState::SCREENSAVER) && (millis() - lastTouchDebugPrint > 5000)) {
        Serial.printf("[TOUCH_DEBUG] Screensaver active, PIN_TOUCH_INT=%d\n", touchIntState);
        lastTouchDebugPrint = millis();
      }

      if (touchIntState == LOW && deviceState.isInState(DeviceState::SCREENSAVER)) {
        Serial.println("[TOUCH] Touch interrupt detected during powerConfig.screensaver - WAKING UP");
        deviceState.transition(DeviceState::READY);
        deactivateScreensaver();
        powerConfig.lastWakeUpTime = millis();
        activityTracking.lastActivityTime = millis();
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
#endif
      
      // Check for actual touch event
      // Note: Minimal debouncing for main area, button has its own 20ms debounce
#ifdef ENABLE_DISPLAY
      if (touch.available() && (millis() - lastTouchEvent > 10)) {
        uint8_t gesture = touch.getGesture();
        uint16_t x = touch.getX();
        uint16_t y = touch.getY();
        bool isTouched = touch.isPressed();
        // One log line per press (rising edge) so taps are visible for
        // diagnosing hit-detection issues.
        if (isTouched && !wasTouched) {
          LOG_INFO("Touch", String("tap x=") + String(x) + " y=" + String(y) +
                            " g=" + String(gesture));
        }

        // First touch after screensaver wake must only wake — not trigger any action.
        // Swallow the rising edge for 300 ms after the backlight came on.
        if (isTouched && !wasTouched &&
            powerConfig.lastWakeUpTime > 0 &&
            millis() - powerConfig.lastWakeUpTime < 300) {
          Serial.println("[TOUCH] Swallowed (screensaver wake protection)");
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }
#else
      // Headless mode - no touch events
      if (false) {
        uint8_t gesture = 0;
        uint16_t x = 0;
        uint16_t y = 0;
        bool isTouched = false;
#endif

        // Physical button area — same hardware edge regardless of display orientation.
        // h: lY > 305 | hi: lY < 14 | v: lX < 14 | vi: lX > 305
        bool inButtonArea;
        if (displayConfig.orientation == "hi") {
          inButtonArea = (y < 14);
        } else if (displayConfig.orientation == "v") {
          inButtonArea = (x < 14);
        } else if (displayConfig.orientation == "vi") {
          inButtonArea = (x > 305);
        } else { // h
          inButtonArea = (y > 305);
        }
        
        if (inButtonArea) {
          // Update activity timer only on new touch (rising edge) so phantom/idle I2C reads
          // from AXS15231B don't keep the screensaver timer from expiring.
          if (isTouched && !wasTouched) activityTracking.lastActivityTime = millis();
          lastTouchEvent = millis();
          // Skip the rest of touch processing - button handler in Task1 will handle this
          goto skip_product_touch_processing;
        }

        // PIN pad takes priority over all other touch processing
        #if ENABLE_NFC
        if (pinPadState.active) {
          if (isTouched && !wasTouched && !pinPadState.submitted) {
            int hit = pinPadHitTest(x, y);
            if (hit == 12) {  // cancel — always allowed, even during error display
              bool wasNfcRingLogin = pinPadState.nfcRingLogin;
              pinPadState.active                = false;
              extensionConfig.nfcPaymentPending = false;
              nfcPendingScreenShown             = false;
              needsQRRedraw                     = true;
              productSelectionState.showTime    = millis();
              if (wasNfcRingLogin) authyShowStart();
              LOG_INFO("PIN", "PIN entry cancelled by user");
            } else if (!pinPadState.showError) {
              // Normal input — only when no error is displayed
              if (hit >= 0 && hit <= 9) {
                if (pinPadState.numDigits < pinPadState.maxDigits) {
                  pinPadState.digits[pinPadState.numDigits++] = '0' + hit;
                  pinPadState.digits[pinPadState.numDigits]   = '\0';
                  showPinPadScreen(pinPadState);
                  if (pinPadState.numDigits == pinPadState.maxDigits) {
                    if (pinPadState.teachMode) {
                      // Authy: verify the teach PIN synchronously. On success,
                      // open the teach session and show the register QR.
                      if (submitTeachPin(String(pinPadState.digits))) {
                        pinPadState.active     = false;
                        authyState.teachActive = true;
                        authyState.teachUntil  = millis() + AUTHY_TEACH_TIMEOUT_MS;
                        authyState.infoMsg     = "Teach mode";
                        // action=register; "Learning Identities" label + CANCEL
                        if (requestAuthLnurl())
                          showAuthTeachScreen("Learning Identities", authyConfig.authPin);
                        LOG_INFO("Teach", "Teach active - showing register QR");
                      } else {
                        showPinPadScreen(pinPadState); // error set by submitTeachPin
                      }
                    } else if (pinPadState.nfcRingLogin) {
                      // Ring-Login: verify PIN + SUN params against zapbox_extension
                      String errMsg;
                      String extId = String(authyState.nfcSunExternalId);
                      String sunP  = String(authyState.nfcSunP);
                      String sunC  = String(authyState.nfcSunC);
                      if (requestNfcAuth(extId, sunP, sunC, String(pinPadState.digits), &errMsg)) {
                        pinPadState.active = false;
                        authyShowStart();
                        LOG_INFO("NFC-Auth", "Auth OK with PIN");
                      } else {
                        // SUN parameters (p/c) are one-time tokens — reusing them triggers
                        // replay detection. Close PIN pad and require a fresh NFC tap.
                        pinPadState.active = false;
                        // Split server message into 2–3 display lines (size 3, centered)
                        String l1, l2, l3;
                        int attIdx = errMsg.indexOf(" attempt");
                        if (attIdx > 0) {
                          // "Invalid PIN. N attempt(s) remaining." → extract N
                          int numEnd = attIdx;
                          int numStart = numEnd - 1;
                          while (numStart > 0 && isdigit((unsigned char)errMsg[numStart - 1]))
                            numStart--;
                          l1 = "Wrong PIN";
                          l2 = errMsg.substring(numStart, numEnd) + " tries left";
                          l3 = "Tap card again";
                        } else {
                          // Unknown card or other server error — show user-friendly message
                          if (errMsg.indexOf("404") >= 0)
                            l1 = "NFC tag unknown";
                          else
                            l1 = errMsg.isEmpty() ? "Wrong PIN" : errMsg;
                        }
                        authyState.infoMsg   = l1;
                        authyState.infoUntil = millis() + 5000;
                        authyShowQR();
                        showAuthPinError(l1, l2, l3);
                        LOG_WARN("NFC-Auth", String("Auth failed: ") + errMsg);
                      }
                    } else {
                      sendPinSubmit(pinPadState.sessionId, String(pinPadState.digits));
                      pinPadState.submitted    = true;
                      pinPadState.submittedAt  = millis();
                      pinPadState.pendingShown = false;
                    }
                  }
                }
              } else if (hit == 10) {  // backspace
                if (pinPadState.numDigits > 0) {
                  pinPadState.digits[--pinPadState.numDigits] = '\0';
                  showPinPadScreen(pinPadState);
                }
              } else if (hit == 11) {  // clear all
                memset(pinPadState.digits, 0, sizeof(pinPadState.digits));
                pinPadState.numDigits = 0;
                showPinPadScreen(pinPadState);
              }
            } else if (!pinPadState.blocked && hit >= 0 && hit <= 9) {
              // Digit during retryable error — clear error immediately and start fresh
              pinPadState.showError  = false;
              memset(pinPadState.digits, 0, sizeof(pinPadState.digits));
              pinPadState.numDigits  = 1;
              pinPadState.digits[0]  = '0' + hit;
              pinPadState.digits[1]  = '\0';
              showPinPadScreen(pinPadState);
              LOG_INFO("PIN", "New digit during error – cleared error, starting fresh");
            }
          }
          if (isTouched && !wasTouched) activityTracking.lastActivityTime = millis();
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }
        #endif // ENABLE_NFC

        // ── Mode selection screen: user picks a mode at startup ─────────────
        if (multiChannelConfig.mode == "modeselect") {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            int hit = modeSelectHitTest(x, y);
            if (hit >= 1 && hit <= 4) {
              applyModeSelection(hit);
            }
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Authy teach mode: only the CANCEL button ends teaching ──────────
        if (authyConfig.enabled && authyState.teachActive) {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            if (authTeachCancelHit(x, y)) {
              LOG_INFO("Teach", "CANCEL pressed - ending teach session");
              endAuthTeach();
            }
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Authy identity trigger: touch the start screen to open the QR ──
        // (Separates the IDENTITY TRIGGER idle screen from the actual auth QR
        // so it's always clear which one is shown.)
        if (authyConfig.enabled) {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            if (!authyState.qrShown && !authyState.payPage) {
              // Idle IDENTITY TRIGGER start screen (no tab): a touch anywhere
              // opens the identity-trigger login QR.
              LOG_INFO("Authy", "Touch on start screen - opening identity QR");
              authyShowQR();
            } else if (authyConfig.dualPage && authTabHit(x, y)) {
              // The bottom-left tab flips between the identity QR and the
              // payment page (only present on the QR pages, not on idle).
              if (authyState.payPage) {
                LOG_INFO("Authy", "Tab pressed - back to IDENTITY login QR");
                authyState.payPage = false;
                authyShowQR();
              } else {
                LOG_INFO("Authy", "Tab pressed - to PAYMENT page");
                authyState.payPage = true;
                authyState.qrShown = false;
                authyShowStart();   // renders the payment page (payPage=true)
              }
            } else if (authyState.qrShown) {
              authyState.qrShownAt = millis();   // identity QR: keep it alive
            }
            // Payment page, non-tab touch: nothing (QR is scannable/NFC-payable).
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Mini-PoS BTC ticker (screensaver in "always" mode) ──────────
        // One touch anywhere on the ticker returns to the amount entry screen.
        if (miniPosConfig.enabled && multiChannelConfig.btcTickerActive) {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            LOG_INFO("MiniPoS", "Ticker touched - showing amount entry screen");
            multiChannelConfig.btcTickerActive = false;
            miniPosState.resetInput();
            miniPosState.inputActive = true;
            miniPosState.lastInputActivity = millis();
            showMiniPosInputScreen();
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Mini-PoS amount entry screen ─────────────────────────────────
        if (miniPosConfig.enabled && miniPosState.inputActive) {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            miniPosState.lastInputActivity = millis();
            int hit = miniPosHitTest(x, y);
            // During the orange "Last Pay" lock all keys are ignored
            if (hit >= 0 && !miniPosState.amountLocked) {
              if (hit <= 9) {  // digit
                int len = miniPosState.numChars;
                String cur = String(miniPosState.amount);
                int dot = cur.indexOf('.');
                bool dotLimitOk = (dot < 0) || (len - dot - 1 < 2);
                if (cur == "0") {
                  // Replace a single leading zero (except "0." paths)
                  miniPosState.amount[0] = '0' + hit;
                } else if (len < 7 && dotLimitOk) {
                  miniPosState.amount[len] = '0' + hit;
                  miniPosState.amount[len + 1] = '\0';
                  miniPosState.numChars = len + 1;
                }
                showMiniPosInputScreen();
              } else if (hit == 10) {  // backspace
                if (miniPosState.numChars > 0) {
                  miniPosState.amount[--miniPosState.numChars] = '\0';
                  showMiniPosInputScreen();
                }
              } else if (hit == 13) {  // decimal point
                String cur = String(miniPosState.amount);
                if (cur.indexOf('.') < 0 && miniPosState.numChars < 6) {
                  if (miniPosState.numChars == 0) {
                    // ".5" → start with "0."
                    miniPosState.amount[0] = '0';
                    miniPosState.amount[1] = '.';
                    miniPosState.amount[2] = '\0';
                    miniPosState.numChars = 2;
                  } else {
                    miniPosState.amount[miniPosState.numChars++] = '.';
                    miniPosState.amount[miniPosState.numChars] = '\0';
                  }
                  showMiniPosInputScreen();
                }
              } else if (hit == 14) {  // INVOICE
                String amt = miniPosNormalizeAmount();
                if (miniPosState.numChars == 0 || amt.toFloat() <= 0) {
                  miniPosShowInfo("Enter amount");
                } else {
                  LOG_INFO("MiniPoS", "Invoice requested: " + amt + " " + miniPosConfig.currency);
                  if (requestMiniPosInvoice(amt)) {
                    miniPosState.inputActive = false;
                    miniPosState.infoMsg = "";
                    miniPosState.infoUntil = 0;
                    showMiniPosQRScreen();
                    // NT3H tag is written by nfcNT3H2111UpdateIfChanged() in the
                    // main loop (lightning buffer now holds the BOLT11)
                  } else {
                    // requestMiniPosInvoice set infoMsg with the error
                    miniPosState.infoUntil = millis() + 3000;
                    showMiniPosInputScreen();
                  }
                }
              } else if (hit == 15) {  // LAST PAY
                String lastAmt;
                if (fetchMiniPosLastPay(lastAmt) && lastAmt.length() > 0) {
                  strncpy(miniPosState.amount, lastAmt.c_str(), sizeof(miniPosState.amount) - 1);
                  miniPosState.amount[sizeof(miniPosState.amount) - 1] = '\0';
                  miniPosState.numChars = strlen(miniPosState.amount);
                  miniPosState.amountLocked = true;
                  miniPosState.lockUntil = millis() + MINIPOS_LASTPAY_LOCK_MS;
                  showMiniPosInputScreen();
                } else {
                  miniPosShowInfo("No history");
                }
              }
            }
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Mini-PoS invoice QR screen: only the CANCEL button reacts ────
        if (miniPosConfig.enabled && miniPosState.invoicePending) {
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            if (miniPosQrCancelHit(x, y)) {
              LOG_INFO("MiniPoS", "Invoice cancelled by user");
              miniPosShowInput();
            }
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Mini-PoS catch-all: transient screens (PAID, errors) ─────────
        // Generic navigation paths must never run in Mini-PoS mode.
        if (miniPosConfig.enabled) {
          if (isTouched && !wasTouched) activityTracking.lastActivityTime = millis();
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }

        // ── Numerical product selection (Touch 3.5 multi-channel) ────────
        // Replaces the generic swipe/tap product navigation: a touch on the
        // main screen (select-product or ticker) opens the keypad panel.
        #ifdef BOARD_JC3248W535C
        if (t35AmbientConfig.numericSelect) {
          // Ignore touches while an NFC payment is pending or a transient
          // screen (help/report/error) owns the display.
          if (extensionConfig.nfcPaymentPending ||
              !(deviceState.isInState(DeviceState::READY) ||
                deviceState.isInState(DeviceState::PRODUCT_SELECTION) ||
                deviceState.isInState(DeviceState::BTC_TICKER))) {
            if (isTouched && !wasTouched) activityTracking.lastActivityTime = millis();
            wasTouched = isTouched;
            goto skip_product_touch_processing;
          }
          if (isTouched && !wasTouched) {
            activityTracking.lastActivityTime = millis();
            if (productSelectState.qrActive) {
              // Product QR screen: only the CANCEL button reacts
              if (productSelectQrCancelHit(x, y)) {
                LOG_INFO("NumSel", "Product QR cancelled - back to keypad");
                numericShowPanel();
              } else {
                productSelectState.qrShownAt = millis(); // touch keeps the QR alive
              }
            } else if (productSelectState.panelActive) {
              productSelectState.lastActivity = millis();
              int hit = productSelectHitTest(x, y);
              if (hit >= 0 && hit <= 9) {  // digit
                if (productSelectState.numDigits < 3) {
                  if (productSelectState.numDigits == 1 && productSelectState.digits[0] == '0') {
                    // Replace a single leading zero
                    productSelectState.digits[0] = '0' + hit;
                  } else {
                    productSelectState.digits[productSelectState.numDigits++] = '0' + hit;
                    productSelectState.digits[productSelectState.numDigits] = '\0';
                  }
                  showProductSelectScreen();
                }
              } else if (hit == 10) {  // backspace
                if (productSelectState.numDigits > 0) {
                  productSelectState.digits[--productSelectState.numDigits] = '\0';
                  showProductSelectScreen();
                }
              } else if (hit == 11) {  // OK — confirm the entered number
                numericHandleGo();
              } else if (hit == 12) {  // CANCEL — exit to main screen
                LOG_INFO("NumSel", "Keypad cancelled - back to main screen");
                numericShowMainScreen();
              }
            } else {
              // Main screen (select-product or BTC ticker): open the keypad
              LOG_INFO("NumSel", "Main screen touched - showing keypad panel");
              deviceState.transition(DeviceState::READY);
              numericShowPanel();
            }
          }
          wasTouched = isTouched;
          goto skip_product_touch_processing;
        }
        #endif // BOARD_JC3248W535C

        // Log any detected gesture (except LONG_PRESS which spams continuously)
        if (gesture != GESTURE_NONE && gesture != GESTURE_LONG_PRESS) {
          Serial.printf("[TOUCH] Detected - Gesture: 0x%02X, X: %d, Y: %d", gesture, x, y);
          
          // Show gesture name
          if (gesture == GESTURE_SWIPE_LEFT) Serial.print(" (SWIPE LEFT)");
          else if (gesture == GESTURE_SWIPE_RIGHT) Serial.print(" (SWIPE RIGHT)");
          else if (gesture == GESTURE_SWIPE_UP) Serial.print(" (SWIPE UP)");
          else if (gesture == GESTURE_SWIPE_DOWN) Serial.print(" (SWIPE DOWN)");
          else if (gesture == GESTURE_SINGLE_CLICK) Serial.print(" (SINGLE CLICK)");
          else if (gesture == GESTURE_DOUBLE_CLICK) Serial.print(" (DOUBLE CLICK)");
          Serial.println();
        }
        
        // SPECIAL: If on error screen, wake from powerConfig.screensaver but don't allow navigation
        if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
          Serial.println("[TOUCH] Touch detected on error screen");
          // Wake from powerConfig.screensaver if active
          if (deviceState.isInState(DeviceState::SCREENSAVER)) {
            Serial.println("[TOUCH] Waking from powerConfig.screensaver (error screen)");
            deviceState.transition(DeviceState::READY);
            deactivateScreensaver();
            powerConfig.lastWakeUpTime = millis();
          }
          // Update activity timer (rising edge only)
          if (isTouched && !wasTouched) activityTracking.lastActivityTime = millis();
          lastTouchEvent = millis();
          wasTouched = isTouched;
          continue; // Don't process navigation on error screen
        }
        
        // Handle touch on product selection screen OR Bitcoin ticker (selecting/always) OR Single mode QR with selecting
        if (deviceState.isInState(DeviceState::PRODUCT_SELECTION) || 
            (multiChannelConfig.btcTickerActive && (multiChannelConfig.btcTickerMode == "selecting" || multiChannelConfig.btcTickerMode == "always")) ||
            (multiChannelConfig.mode == "off" && multiChannelConfig.btcTickerMode == "selecting" && !multiChannelConfig.btcTickerActive)) {
          bool navigateBack = false;
          String actionName = "";
          
          // IMPORTANT: Check if touch is in button area - if yes, IGNORE for product navigation
          // Let handleTouchButton() (running in Task1) handle it instead
          // Physical button is ALWAYS at Y > 305 (hardware coordinates don't rotate!)
          bool inButtonArea = (y > 305);
          
          if (inButtonArea) {
            // Touch is in button area - ignore for product navigation
            // handleTouchButton() will process it
            wasTouched = isTouched;
            continue; // Skip product navigation logic
          }
          
          // Check for swipe gestures (any direction)
          // Renamed for horizontal displayConfig.orientation with button on right:
          // Physical UP (away from button) = SWIPE_DOWN → renamed to LEFT
          // Physical DOWN (toward button) = SWIPE_UP → renamed to RIGHT  
          // Physical LEFT = SWIPE_LEFT → renamed to DOWN
          // Physical RIGHT = SWIPE_RIGHT → renamed to UP
          if (gesture == GESTURE_SWIPE_UP) {
            actionName = "SWIPE RIGHT";
            navigateBack = true;
          } else if (gesture == GESTURE_SWIPE_DOWN) {
            actionName = "SWIPE LEFT";
            navigateBack = true;
          } else if (gesture == GESTURE_SWIPE_LEFT) {
            actionName = "SWIPE DOWN";
            navigateBack = true;
          } else if (gesture == GESTURE_SWIPE_RIGHT) {
            actionName = "SWIPE UP";
            navigateBack = true;
          }
          // Check for single click or long press on left or right side of screen
          else if (gesture == GESTURE_SINGLE_CLICK || gesture == GESTURE_LONG_PRESS) {
            // Display: 170x320 native, rotated to 320x170 (rotation=1)
            // Touch coordinates are NOT rotated: X=0-170, Y=0-320
            Serial.printf("[TOUCH] %s detected at X=%d, Y=%d - ", 
                         gesture == GESTURE_SINGLE_CLICK ? "SINGLE CLICK" : "LONG PRESS", x, y);
            
            // With rotation=1: Touch Y maps to Display X
            // Left side of display (low Display X) = low Touch Y (< 160)
            // Right side of display (high Display X) = high Touch Y (> 160)
            if (y < 160) {
              Serial.println("LEFT SIDE");
              actionName = "TOUCH LEFT";
              navigateBack = true;
            } else if (y > 160) {
              Serial.println("RIGHT SIDE");
              actionName = "TOUCH RIGHT";
              navigateBack = true;
            } else {
              Serial.println("CENTER (ignored)");
            }
          }
          // Also accept quick touch without gesture (GESTURE_NONE rising edge)
          // or AXS15231B 0xFF "new-touch" marker (fired on every touch DOWN on JC3248W535C).
          // 500 ms timeout in the action block prevents rapid re-triggering.
          else if ((gesture == GESTURE_NONE && isTouched && !wasTouched) || gesture == 0xFF) {
            Serial.printf("[TOUCH] QUICK TOUCH at X=%d, Y=%d - ", x, y);

            if (y < 160) {
              Serial.println("LEFT SIDE");
              actionName = "QUICK TOUCH LEFT";
              navigateBack = true;
            } else if (y > 160) {
              Serial.println("RIGHT SIDE");
              actionName = "QUICK TOUCH RIGHT";
              navigateBack = true;
            } else {
              Serial.println("CENTER (ignored)");
            }
          }
          
          if (navigateBack) {
            // Hybrid approach: Session-based + timeout fallback (same as product navigation)
            // - Prevents continuous triggering when holding finger (session control)
            // - Allows fast consecutive swipes with finger-lift between (timeout reset)
            unsigned long now = millis();
            bool timeoutExpired = (now - lastNavigationTime >= 500); // 500ms timeout for new swipe
            
            if (!gestureHandledThisTouch || timeoutExpired) {
              Serial.printf("[TOUCH] >>> %s - ", actionName.c_str());
              if (timeoutExpired && gestureHandledThisTouch) {
                Serial.printf("(timeout reset after %lu ms) - ", now - lastNavigationTime);
              }
              gestureHandledThisTouch = true; // Mark gesture as handled
              lastNavigationTime = now; // Update navigation timestamp
            } else {
              Serial.printf("[TOUCH] >>> %s IGNORED (only %lu ms since last navigation)\n", actionName.c_str(), now - lastNavigationTime);
              wasTouched = isTouched;
              continue;
            }
            
            deviceState.transition(DeviceState::READY);
            
            // Multi-Channel-Control Mode with SELECTING: Show ticker on demand
            if (multiChannelConfig.mode != "off" && lightningConfig.thresholdKey.length() == 0 && multiChannelConfig.btcTickerMode == "selecting") {
              if (multiChannelConfig.btcTickerActive) {
                // Already showing ticker - skip back to product
                Serial.println("Skip from ticker to product");
                multiChannelConfig.btcTickerActive = false;
                navigateToNextProduct();
              } else if (multiChannelConfig.currentProduct == -1) {
                // On product-selection screen: go to first product (same as LED button)
                Serial.println("Select screen -> navigate to first product");
                navigateToNextProduct();
              } else {
                // On a product QR screen: show ticker on demand
                Serial.println("Show Bitcoin ticker for 10 seconds");
                btctickerScreen();
                multiChannelConfig.btcTickerActive = true;
                deviceState.transition(DeviceState::BTC_TICKER);
                productSelectionState.showTime = millis();
              }
            }
            // Single Mode with SELECTING: Show ticker on demand
            else if (multiChannelConfig.mode == "off" && multiChannelConfig.btcTickerMode == "selecting") {
              if (multiChannelConfig.btcTickerActive) {
                // Already showing ticker - skip back to QR
                Serial.println("Skip from ticker to QR (Single mode)");
                multiChannelConfig.btcTickerActive = false;
                ensureQrForPin(RELAY_CHANNEL_PINS[0]);
                showQRScreen();
                productSelectionState.showTime = 0;
              } else {
                // Show ticker for 10 seconds
                Serial.println("Show Bitcoin ticker for 10 seconds (Single mode)");
                btctickerScreen();
                multiChannelConfig.btcTickerActive = true;
                productSelectionState.showTime = millis();
              }
            }
            // Single Mode with ALWAYS: Show QR on touch, return to ticker after timeout
            else if (multiChannelConfig.mode == "off" && multiChannelConfig.btcTickerMode == "always") {
              if (multiChannelConfig.btcTickerActive) {
                // Showing ticker - switch to QR on touch
                Serial.println("Touch detected - switching from ticker to QR (ALWAYS mode)");
                multiChannelConfig.btcTickerActive = false;
                ensureQrForPin(RELAY_CHANNEL_PINS[0]);
                showQRScreen();
                productSelectionState.showTime = millis(); // Start timeout to return to ticker
              } else {
                // Already on QR: refresh timeout so interaction keeps QR visible
                productSelectionState.showTime = millis();
              }
            }
            // Multi-Channel-Control Mode: Navigate to next product
            else if (multiChannelConfig.mode != "off" && lightningConfig.thresholdKey.length() == 0) {
              multiChannelConfig.btcTickerActive = false; // Exit ticker on navigation
              Serial.println("Navigate to next product");
              navigateToNextProduct();
              // Reset timer for product navigation
              productSelectionState.showTime = millis();
            } else {
              // Normal/Special/Threshold Mode: Return to QR screen
              Serial.println("Returning to QR screen");
              redrawQRScreen();
              // Reset timer for normal mode
              productSelectionState.showTime = millis();
            }
          }
        }
        // Handle touch on product QR screen (Multi-Channel-Control mode only)
        // Allow navigation when showing product QR code
        else if (multiChannelConfig.mode != "off" && lightningConfig.thresholdKey.length() == 0 && !deviceState.isInState(DeviceState::PRODUCT_SELECTION)) {
          bool navigate = false;
          String actionName = "";
          
          // IMPORTANT: Check if touch is in button area - if yes, IGNORE for product navigation
          // Physical button is ALWAYS at Y > 305 (hardware coordinates don't rotate!)
          bool inButtonArea = (y > 305);
          
          if (inButtonArea) {
            // Touch is in button area - ignore for product navigation
            wasTouched = isTouched;
            continue; // Skip product navigation logic
          }
          
          // Respond to deliberate gestures
          if (gesture == GESTURE_SWIPE_UP || gesture == GESTURE_SWIPE_DOWN || 
              gesture == GESTURE_SWIPE_LEFT || gesture == GESTURE_SWIPE_RIGHT) {
            navigate = true;
            actionName = "SWIPE";
          } else if (gesture == GESTURE_SINGLE_CLICK) {
            if (y < 160 || y > 160) { // Left or right side
              navigate = true;
              actionName = "SINGLE CLICK";
            }
          }
          // Also accept quick touch: GESTURE_NONE rising edge OR AXS15231B 0xFF new-touch marker.
          // AXS15231B fires 0xFF on every touch DOWN regardless of wasTouched state;
          // the 500 ms timeout in the navigate block prevents rapid re-triggering.
          else if ((gesture == GESTURE_NONE && isTouched && !wasTouched) || gesture == 0xFF) {
            if (y < 160 || y > 160) { // Left or right side
              navigate = true;
              actionName = "QUICK TOUCH";
            }
          }
          
          if (navigate) {
            // Hybrid approach: Session-based + timeout fallback
            // - Prevents continuous triggering when holding finger (session control)
            // - Allows fast consecutive swipes with finger-lift between (timeout reset)
            unsigned long now = millis();
            bool timeoutExpired = (now - lastNavigationTime >= 500); // 500ms timeout for new swipe
            
            if (!gestureHandledThisTouch || timeoutExpired) {
              Serial.printf("[TOUCH] >>> %s on product screen - Navigate to next product", actionName.c_str());
              if (timeoutExpired && gestureHandledThisTouch) {
                Serial.printf(" (timeout reset after %lu ms)", now - lastNavigationTime);
              }
              Serial.println();
              navigateToNextProduct();
              gestureHandledThisTouch = true; // Mark gesture as handled
              lastNavigationTime = now; // Update navigation timestamp
            } else {
              Serial.printf("[TOUCH] >>> %s IGNORED (only %lu ms since last navigation)\n", actionName.c_str(), now - lastNavigationTime);
            }
          }
        }
        
        // Update last touch time if any gesture detected or touch state changed
        if (gesture != GESTURE_NONE || (isTouched != wasTouched)) {
          lastTouchEvent = millis();
        }
        
        // Reset gesture flag when touch is released - allows new gesture on next touch
        if (!isTouched && wasTouched) {
          gestureHandledThisTouch = false;
        }
        
        // Reset action flag when touch is released
        // (Debounce check happens during action execution instead)
        if (!isTouched && wasTouched) {
          actionExecutedThisTouch = false;
        }
        
        // Reset activity timer only on rising edge (new finger-down event).
        // Using rising-edge (isTouched && !wasTouched) instead of "any touch frame"
        // prevents the AXS15231B I2C poller from continuously resetting the screensaver
        // timer on every loop iteration — even phantom/idle reads that return touch data.
        if (isTouched && !wasTouched) {
          activityTracking.lastActivityTime = millis();
        }
        
        // Remember touch state for next iteration
        wasTouched = isTouched;
        
        skip_product_touch_processing:
        ; // Empty statement required after label
      }
    }
    
    // Check if it's time to show/hide Bitcoin ticker screen
    // Behavior depends on multiChannelConfig.btcTickerMode
    // Numerical product selection manages its own screens/timeouts (see above).
    bool numericSelectActive = false;
    #ifdef BOARD_JC3248W535C
    numericSelectActive = t35AmbientConfig.numericSelect;
    #endif
    if (!numericSelectActive &&
        !deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && lightningConfig.thresholdKey.length() == 0) {
      if (multiChannelConfig.btcTickerMode == "always") {
        if (multiChannelConfig.mode != "off") {
          // ALWAYS mode Duo/Quattro: Show ticker after PRODUCT_SELECTION_DELAY on products
          if (!pinPadState.active && !multiChannelConfig.btcTickerActive && productSelectionState.showTime > 0 &&
              (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
            Serial.println("[SCREEN] Showing Bitcoin ticker screen after timeout (ALWAYS mode - Duo/Quattro)");
            btctickerScreen();
            multiChannelConfig.btcTickerActive = true;
            deviceState.transition(DeviceState::BTC_TICKER);
            productSelectionState.showTime = 0; // Reset timer - ticker has no timeout in ALWAYS mode
          }
        } else {
          // ALWAYS mode Single: Return to ticker after PRODUCT_TIMEOUT on QR
          // (Mini-PoS has its own idle timer keyed to the amount entry screen)
          if (!miniPosConfig.enabled &&
              !pinPadState.active && !multiChannelConfig.btcTickerActive && productSelectionState.showTime > 0 &&
              (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
            Serial.println("[SCREEN] Returning to ticker after timeout (ALWAYS mode - Single)");
            btctickerScreen();
            multiChannelConfig.btcTickerActive = true;
            productSelectionState.showTime = 0; // Reset timer
          }
        }
      } else if (multiChannelConfig.btcTickerMode == "off" && multiChannelConfig.mode != "off") {
        // OFF mode with Duo/Quattro: Return to Product No.1 after timeout
        if (productSelectionState.showTime > 0 &&
            !pinPadState.active &&
            !extensionConfig.nfcPaymentPending &&
            (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
          if (multiChannelConfig.currentProduct > 0) {
            if (multiChannelConfig.currentProduct == 1) {
              productSelectionState.showTime = 0; // Already on Product 1, reset timer
            } else {
              Serial.println("[SCREEN] Timeout reached - returning to Product No.1 (OFF mode - Duo/Quattro)");
              multiChannelConfig.currentProduct = 1;
              deviceState.transition(DeviceState::READY);
              int pin = (multiChannelConfig.mode == "servo") ? servoConfig.productToPin(1) : RELAY_CHANNEL_PINS[0];
              ensureQrForPin(pin);
              int pinIndex = getPinIndex(pin);
              String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                             ? productLabels.labels[pinIndex] : "Pin " + String(pin);
              showProductQRScreen(label, pin);
              productSelectionState.showTime = 0;
            }
          }
        }
      } else if (multiChannelConfig.btcTickerMode == "selecting") {
        if (multiChannelConfig.mode == "off") {
          // Single mode: Hide ticker after BTC_TICKER_TIMEOUT_DELAY (10 seconds)
          if (multiChannelConfig.btcTickerActive && productSelectionState.showTime > 0 && 
              (millis() - productSelectionState.showTime) >= BTC_TICKER_TIMEOUT_DELAY) {
            Serial.println("[SCREEN] Hiding Bitcoin ticker after ticker timeout (SELECTING mode - Single)");
            multiChannelConfig.btcTickerActive = false;
            // Show normal QR screen
            ensureQrForPin(RELAY_CHANNEL_PINS[0]);
            showQRScreen();
            productSelectionState.showTime = 0; // Reset timer
          }
        } else {
          // Duo/Quattro mode: Different timeout behavior based on what's showing
          if (productSelectionState.showTime > 0) {
            if (multiChannelConfig.btcTickerActive && (millis() - productSelectionState.showTime) >= BTC_TICKER_TIMEOUT_DELAY) {
              // Ticker showing: Hide ticker after BTC_TICKER_TIMEOUT_DELAY and return to last product
              Serial.println("[SCREEN] Hiding ticker after ticker timeout (SELECTING mode - Duo/Quattro)");
              multiChannelConfig.btcTickerActive = false;
              // Show last product again
              if (multiChannelConfig.currentProduct >= 1) {
                navigateToNextProduct();
              } else {
                // Fallback: show product selection (or stay on product 1 for servo single)
                if (multiChannelConfig.mode == "servo" && servoConfig.activeChannelCount() <= 1) {
                  multiChannelConfig.currentProduct = 1;
                  int firstPin = servoConfig.productToPin(1);
                  int pinIndex = getPinIndex(firstPin);
                  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                      ? productLabels.labels[pinIndex]
                      : String("Pin ") + String(firstPin);
                  ensureQrForPin(firstPin);
                  showProductQRScreen(label, firstPin);
                  deviceState.transition(DeviceState::READY);
                }
                #ifdef BOARD_JC3248W535C
                else if (t35AmbientConfig.oneForAll) {
                  // T35 OFA: go straight to CH01 QR
                  multiChannelConfig.currentProduct = 1;
                  int firstPin = PIN_RELAY_CH01;
                  int pinIndex = getPinIndex(firstPin);
                  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                      ? productLabels.labels[pinIndex] : String("Pin ") + String(firstPin);
                  ensureQrForPin(firstPin);
                  showProductQRScreen(label, firstPin);
                  deviceState.transition(DeviceState::READY);
                }
                #endif
                else {
                  multiChannelConfig.currentProduct = -1;
                  deviceState.transition(DeviceState::PRODUCT_SELECTION);
                  productSelectionScreen();
                }
              }
              productSelectionState.showTime = 0; // Reset timer
            } else if (multiChannelConfig.currentProduct > 0 && !deviceState.isInState(DeviceState::PRODUCT_SELECTION) &&
                      !pinPadState.active &&
                      !extensionConfig.nfcPaymentPending &&
                      (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
              // Product showing: Return to Product No.1 after PRODUCT_SELECTION_DELAY
              if (multiChannelConfig.currentProduct == 1) {
                productSelectionState.showTime = 0; // Already on Product 1, reset timer
              } else {
                Serial.println("[SCREEN] Timeout reached - returning to Product No.1 (SELECTING mode - Duo/Quattro)");
                multiChannelConfig.currentProduct = 1;
                deviceState.transition(DeviceState::READY);
                int pin = (multiChannelConfig.mode == "servo") ? servoConfig.productToPin(1) : RELAY_CHANNEL_PINS[0];
                ensureQrForPin(pin);
                int pinIndex = getPinIndex(pin);
                String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                               ? productLabels.labels[pinIndex] : "Pin " + String(pin);
                showProductQRScreen(label, pin);
                productSelectionState.showTime = 0;
              }
            }
          }
        }
      }
    }
    
    // Power saving checks (screensaver/deep sleep)
    handlePowerSavingChecks();
    
    // ── Headless: WebSocket disconnect/reconnect based on sensor blocking ────
    // When any sensor condition blocks payments, disconnect WebSocket so the
    // LNbits server rejects static QR code payments (no active connection = no payout).
    // When sensors clear, resume webSocket.loop() to auto-reconnect.
    #if !ENABLE_DISPLAY
    {
      static bool sensorWsDisconnected = false;
      if (lightBarrierConfig.isAnyBlocking()) {
        if (!sensorWsDisconnected) {
          webSocket.disconnect();
          sensorWsDisconnected = true;
          Serial.println("[SENSOR] WebSocket disconnected — server will reject QR payments");
        }
        // Skip webSocket.loop() while sensors are blocking
      } else {
        if (sensorWsDisconnected) {
          sensorWsDisconnected = false;
          Serial.println("[SENSOR] Sensors cleared — WebSocket reconnecting");
        }
        webSocket.loop();
      }
    }
    #else
    webSocket.loop();
    #endif
    loopCount++;
    
    // Update LED status regularly to reflect current network state
    updateReadyLed();

#if ENABLE_BITCOIN_DATA
    // First-time BTC fetch: launched once as a FreeRTOS task so loop() keeps running.
    // This replaces the old synchronous fetch in setup() / fetchSwitchLabels() that
    // blocked touch processing for up to 25 s during SSL timeouts.
    static bool btcFirstFetchLaunched = false;
    if (!btcFirstFetchLaunched && networkStatus.confirmed.websocket &&
        !deviceState.isInState(DeviceState::CONFIG_MODE)) {
      btcFirstFetchLaunched = true;
      xTaskCreatePinnedToCore(
        [](void*) {
          vTaskDelay(pdMS_TO_TICKS(500)); // brief pause so QR screen draws first
          fetchBitcoinData();
          // First data just arrived — draw it if the ticker is on screen.
          // Without this the periodic updater waits a full update interval
          // (5 min) before the "Loading..." placeholders are replaced.
          if (!deviceState.isInState(DeviceState::SCREENSAVER) &&
              !deviceState.isInState(DeviceState::DEEP_SLEEP) &&
              !deviceState.isInState(DeviceState::CONFIG_MODE)) {
            if (multiChannelConfig.btcTickerActive) {
              updateBtctickerValues();
              Serial.println("[BTC] Initial data drawn to active ticker");
            }
            #ifdef BOARD_JC3248W535C
            if (t35AmbientConfig.numericSelect &&
                deviceState.isInState(DeviceState::PRODUCT_SELECTION) &&
                !productSelectState.panelActive && !productSelectState.qrActive) {
              updateProductSelectBlockHeight();
              Serial.println("[BTC] Initial block height drawn to product selection screen");
            }
            #endif
          }
          vTaskDelete(nullptr);
        },
        "btc_init", 8192, nullptr, 1, nullptr, 1 /* Core 1 */
      );
      Serial.println("[BTC] Initial fetch task launched (async, Core 1)");
    }
    // Periodic ticker update (runs only when ticker screen is active)
    updateBitcoinTicker();
#endif
    
    // Update switch labels periodically (checks interval internally, non-blocking)
    updateSwitchLabels();
    
    // Log status every 200000 loops (roughly every 10-20 minutes)
    if (loopCount % 200000 == 0)
    {
      Serial.printf("[LOOP] Still waiting... WiFi: %d, WS Connected: %d, Queue size: %d\n", 
                    WiFi.status() == WL_CONNECTED, webSocket.isConnected(), paymentQueue.size());
    }
    
    // Check Internet connectivity every 30 seconds (independent of WebSocket)
    if (millis() - lastInternetCheck > 30000 && !deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      // CRITICAL: Check WiFi first! Don't show "No Internet" if WiFi is down
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[INTERNET] Skipping Internet check - WiFi is down");
        lastInternetCheck = millis();
      } else if (nfcConfig.nfcSessionActive) {
        // Defer: NFC is exchanging APDUs — HTTP activity on Core 0 disrupts I2C timing
        Serial.println("[INTERNET] Deferred - NFC session active");
      } else if (webSocket.isConnected() &&
                 ((networkStatus.lastServerPingTime > 0 && millis() - networkStatus.lastServerPingTime < 45000) ||
                  (networkStatus.wsConnectedTime    > 0 && millis() - networkStatus.wsConnectedTime    < 60000))) {
        // WebSocket alive: skip HTTP check if server pinged us within 45s OR if WS just connected
        // (first ping arrives ~20s after connect, so the 60s window covers the gap).
        // This avoids competing TCP connections that overwhelm Fritzbox connection-tracking.
        String reason = (networkStatus.lastServerPingTime > 0 && millis() - networkStatus.lastServerPingTime < 45000)
                        ? "last server ping: " + String((millis() - networkStatus.lastServerPingTime) / 1000) + "s ago"
                        : "WS just connected (" + String((millis() - networkStatus.wsConnectedTime) / 1000) + "s ago)";
        Serial.println("[INTERNET] Skipping check - WebSocket active (" + reason + ")");
        lastInternetCheck = millis();
        networkStatus.confirmed.internet = true;
      } else if (WiFi.status() == WL_CONNECTED) {
        bool hasInternet = checkInternetConnectivity();
        if (!hasInternet) {
          if (!deviceState.isInState(DeviceState::ERROR_RECOVERABLE) || currentErrorType > 2) {
            Serial.println("[INTERNET] Internet connection lost!");
            if (networkStatus.errors.internet < 99) networkStatus.errors.internet++;
            Serial.printf("[ERROR] Internet error count: %d\n", networkStatus.errors.internet);
            internetReconnectScreen();
            deviceState.transition(DeviceState::ERROR_RECOVERABLE);
            currentErrorType = 2; // Internet error
            // Reset product selection screen
            deviceState.transition(DeviceState::READY);
          }
          networkStatus.confirmed.internet = false; // Clear confirmation
          networkStatus.confirmed.server = false; // Also clear server/websocket (they depend on Internet)
          networkStatus.confirmed.websocket = false;
          webSocket.disconnect();
        } else {
          // Internet OK - set confirmation
          if (!networkStatus.confirmed.internet) {
            Serial.println("[CONFIRMED] Internet connection confirmed!");
            networkStatus.confirmed.internet = true;
            
            // Always fetch Bitcoin data when Internet is restored (if ticker is active)
            // BUT: Don't update bitcoinData.lastUpdate so the regular timer continues
            if (multiChannelConfig.btcTickerActive) {
              Serial.println("[RECOVERY] Internet restored - fetching Bitcoin data for ticker...");
              HTTPClient http;

              // Fetch BTC price from mempool.space (same server as block height)
              String currencyUpper = currency;
              currencyUpper.toUpperCase();
              http.begin("https://mempool.space/api/v1/prices");
              http.setTimeout(8000);
              if (http.GET() == 200) {
                JsonDocument doc;
                if (!deserializeJson(doc, http.getString())) {
                  long price = doc[currencyUpper] | 0L;
                  if (price > 0) {
                    bitcoinData.price = String(price);
                    Serial.println("[BTC] Recovery price updated: " + bitcoinData.price + " " + currency);
                  }
                }
              }
              http.end();

              delay(100);

              // Fetch block height
              http.begin("https://mempool.space/api/blocks/tip/height");
              http.setTimeout(8000);
              if (http.GET() == 200) {
                String val = http.getString();
                val.trim();
                if (val.length() > 0) {
                  bitcoinData.blockHigh = val;
                  Serial.println("[BTC] Recovery block height updated: " + bitcoinData.blockHigh);
                }
              }
              http.end();
              
              // DON'T update bitcoinData.lastUpdate - let the regular update cycle continue
              Serial.println("[BTC] Recovery fetch completed (timer NOT reset)");
              
              // Redraw ticker screen if it was active
              if (!deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
                btctickerScreen();
              }
            }
            
            // If recovering from Internet error screen, clear error and refresh display
            if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && currentErrorType == 2) {
              Serial.println("[RECOVERY] Clearing Internet error screen...");
              onErrorScreen = false;
              currentErrorType = 0;
              deviceState.transition(DeviceState::READY);
              
              // Redraw appropriate screen
              if (multiChannelConfig.btcTickerActive) {
                btctickerScreen();
              } else {
                redrawQRScreen();
              }
            }
          }
        }
        lastInternetCheck = millis();
      }
    }
    
    // Send ping every 30 seconds to check if WebSocket connection is really alive
    // Only if WebSocket is connected!
    if (webSocket.isConnected() && millis() - networkStatus.lastPingTime > 30000 && !deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      // Check if we're still waiting for a pong from previous ping
      if (networkStatus.waitingForPong && (millis() - networkStatus.lastPingTime) > 40000)
      {
        // No pong received for 40+ seconds - connection is dead
        Serial.println("[PING] ERROR: No pong received for 40+ seconds, connection dead!");
        Serial.println("[PING] Forcing WebSocket disconnect and reconnect...");
        webSocket.disconnect();
        networkStatus.confirmed.websocket = false;
        networkStatus.waitingForPong = false;
        return; // Let reconnect logic handle it in next cycle
      }
      
      Serial.println("[PING] Sending WebSocket ping to verify connection...");
      webSocket.sendPing();
      networkStatus.lastPingTime = millis();
      networkStatus.waitingForPong = true;
    }
    
    // Check WiFi, Server and WebSocket every 5 seconds while waiting for payment
    // Note: Internet is checked separately every 30 seconds
    if (millis() - lastWiFiCheck > 5000)
    {
      lastWiFiCheck = millis(); // Reset at entry so all return paths are rate-limited
      // Check connection status step by step
      bool wifiOk = (WiFi.status() == WL_CONNECTED);
      bool serverOk = true;
      bool websocketOk = webSocket.isConnected();
      
      // Step 1: WiFi check (HIGHEST PRIORITY - check immediately)
      if (!wifiOk) {
        // WiFi down - highest priority, skip all other checks immediately
        Serial.println("[CHECK] WiFi is down - triggering WiFi reconnect");
        currentErrorType = 1; // WiFi error (highest priority)
        networkStatus.confirmed.wifi = false; // Clear all confirmations when WiFi is down
        networkStatus.confirmed.internet = false;
        networkStatus.confirmed.server = false;
        networkStatus.confirmed.websocket = false;
        checkAndReconnectWiFi();
        if (deviceState.isInState(DeviceState::CONFIG_MODE)) return;
        return; // Exit immediately after WiFi check
      }
      // Step 2: WebSocket NOT connected - check Server
      else if (!websocketOk) {
        // WiFi OK but WebSocket not connected - check if Server is reachable
        serverOk = checkServerReachability();
        if (!serverOk) {
          // Server down - WebSocket can't connect (that's expected)
          websocketOk = false;
        }
        // If server OK but WebSocket not connected → WebSocket problem (checked later)
      }
      
      // Handle errors by priority: WiFi (1) > Internet (2) > Server (3) > WebSocket (4)
      // Note: Internet errors are handled in the 30-second check above
      // Note: WiFi error already handled above with immediate return
      
      if (wifiOk && !serverOk && !deviceState.isInState(DeviceState::CONFIG_MODE))
      {
        // Server not reachable (TCP port check failed)
        // IMPORTANT: Skip if Internet error (type 2) is active - higher priority!
        if (onErrorScreen && currentErrorType == 2)
        {
          // Internet error has higher priority - don't show Server error
          Serial.println("Server check skipped - Internet error has higher priority");
          return;
        }
        
        // Only show/update Server error if no higher priority error
        if (!onErrorScreen || currentErrorType >= 3)
        {
          Serial.println("Server not reachable (TCP port 443 closed/timeout)");
          if (networkStatus.errors.server < 99) networkStatus.errors.server++;
          Serial.printf("[ERROR] Server error count: %d\n", networkStatus.errors.server);
          Serial.println("[SCREEN] Showing Server error screen (type 3)");
          serverReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 3; // Server error
          // Reset product selection screen (transition to READY as base state)
          deviceState.transition(DeviceState::READY);
        }
        // Clear server/websocket confirmations
        networkStatus.confirmed.server = false;
        networkStatus.confirmed.websocket = false;
        // Note: WebSocket can't connect if server is down - that's expected
        webSocket.disconnect();
        networkStatus.waitingForPong = false;
        return;
      }
      
      // Server OK but still on Server error screen → Move to WebSocket error check
      if (wifiOk && serverOk && onErrorScreen && currentErrorType == 3 && !deviceState.isInState(DeviceState::CONFIG_MODE))
      {
        Serial.println("[RECOVERY] Server OK - moving to WebSocket error check");
        networkStatus.confirmed.server = true;
        currentErrorType = 4; // Move to WebSocket error check
        // Don't return - let WebSocket check run below
      }
      
      if (wifiOk && serverOk && !websocketOk && !deviceState.isInState(DeviceState::CONFIG_MODE))
      {
        // WebSocket error - only if WiFi AND Server are OK
        // IMPORTANT: Skip if higher priority error (Internet=2 or Server=3) is active!
        if (onErrorScreen && currentErrorType > 0 && currentErrorType < 4)
        {
          // Higher priority error (WiFi/Internet/Server) - don't show WebSocket error
          Serial.println("WebSocket check skipped - higher priority error active");
          return;
        }
        
        // Only show/update WebSocket error if no higher priority error
        if (!onErrorScreen || currentErrorType >= 4)
        {
          Serial.println("WebSocket disconnected, attempting reconnect...");
          // Reset product selection screen (transition to READY as base state)
          deviceState.transition(DeviceState::READY);
        }
        
        // Try to reconnect WebSocket (up to 3 attempts)
        int reconnectAttempts = 0;
        int sslErrorCount = 0; // Count SSL connection errors (detected by error events)
        
        while (!webSocket.isConnected() && reconnectAttempts < 3 && !deviceState.isInState(DeviceState::CONFIG_MODE))
        {
          if (WiFi.status() != WL_CONNECTED || deviceState.isInState(DeviceState::CONFIG_MODE)) return;
          
          webSocket.disconnect();
          vTaskDelay(pdMS_TO_TICKS(500));
          
          reconnectAttempts++;
          Serial.printf("WebSocket reconnect attempt %d/3\n", reconnectAttempts);
          
          if (lightningConfig.thresholdKey.length() > 0) {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + lightningConfig.thresholdKey);
          } else {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
          }
          webSocket.onEvent(webSocketEvent);
          webSocket.setReconnectInterval(1000);
          
          // Wait for connection to establish (up to 3 seconds, with loop() calls)
          for (int i = 0; i < 30 && !webSocket.isConnected(); i++) {
            webSocket.loop();
            vTaskDelay(pdMS_TO_TICKS(100));
          }
        }
        
        if (webSocket.isConnected())
        {
          Serial.println("WebSocket reconnected successfully");
          networkStatus.confirmed.websocket = true; // Set confirmation
          networkStatus.confirmed.wifi = true;       // WiFi must be OK if WS connected
          networkStatus.confirmed.server = true;     // Server must be OK if WS connected
          onErrorScreen = false;
          currentErrorType = 0;
          consecutiveWebSocketFailures = 0;
          networkStatus.waitingForPong = false;
          
          // Labels are fetched automatically by WStype_CONNECTED in webSocketEvent()
          // when the WebSocket connects — no second fetch needed here.

          // Redraw QR screen to replace any leftover error screen
          Serial.println("[RECOVERY] WebSocket recovery complete - redrawing QR screen");
          redrawQRScreen();
          productSelectionState.showTime = millis();
          deviceState.transition(DeviceState::READY);
          
          return;
        }
        else
        {
          // WebSocket reconnect failed after 3 attempts
          Serial.printf("WebSocket reconnect failed after %d attempts\n", reconnectAttempts);
          if (networkStatus.errors.websocket < 99) networkStatus.errors.websocket++;
          Serial.printf("[ERROR] WebSocket error count: %d\n", networkStatus.errors.websocket);
          
          // CRITICAL: Fully tear down the WebSocket before showing error screen.
          // The previous beginSSL() calls left the library in auto-reconnect mode.
          // If WiFi comes up while we're on the error screen, the library reconnects
          // in the background – but often into a broken state where it reports
          // isConnected()==true yet the server never sends pings / data.
          // disconnect() stops the auto-reconnect timer and closes the TCP socket.
          webSocket.disconnect();
          
          Serial.println("[SCREEN] Showing WebSocket error screen (type 4)");
          websocketReconnectScreen();
          networkStatus.confirmed.websocket = false; // Clear confirmation
          currentErrorType = 4;
          onErrorScreen = true;
          // Reset product selection screen (transition to READY as base state)
          deviceState.transition(DeviceState::READY);
          return;
        }
      }
      
      // Auto-recovery: All connections restored - set confirmation flags
      // IMPORTANT: Check labelsLoadedSuccessfully to ensure device config is valid
      // A 404 from fetchSwitchLabels() means the instance was deleted on the server
      if (wifiOk && serverOk && websocketOk && labelsLoadedSuccessfully)
      {
        // Set all confirmation flags (so QR screen can be shown)
        if (!networkStatus.confirmed.wifi) {
          networkStatus.confirmed.wifi = true;
          Serial.println("[CONFIRMED] WiFi connection confirmed!");
        }
        if (!networkStatus.confirmed.server) {
          networkStatus.confirmed.server = true;
          Serial.println("[CONFIRMED] Server connection confirmed!");
        }
        if (!networkStatus.confirmed.websocket) {
          networkStatus.confirmed.websocket = true;
          Serial.println("[CONFIRMED] WebSocket connection confirmed!");
        }
        
        // Clear error state and redraw QR screen
        if (onErrorScreen) {
          Serial.printf("[RECOVERY] All connections recovered (was error type %d)\n", currentErrorType);
          Serial.println("[SCREEN] Clearing error screen and redrawing QR code");
          onErrorScreen = false;
          currentErrorType = 0;
          consecutiveWebSocketFailures = 0; // Reset failure counter
          networkStatus.waitingForPong = false;
          
          // Redraw QR screen immediately
          redrawQRScreen();
          Serial.println("[SCREEN] QR screen displayed after recovery");
          // Reset product selection timer
          productSelectionState.showTime = millis();
          deviceState.transition(DeviceState::READY);
        }
        // Successful recovery — reset 404 retry state
        labels404NextRetry = 0;
        labels404RetryCount = 0;
        return;
      }
      // If WebSocket is connected but labels failed to load, retry.
      // For transient errors (timeout, 500), retry quickly (15s).
      // For config errors (404), retry slowly (2 min) — device may not exist.
      else if (wifiOk && serverOk && websocketOk && !labelsLoadedSuccessfully) {
        unsigned long now = millis();
        unsigned long retryInterval = labelsValidationAttempted ? 15000UL : 120000UL;
        if (labels404NextRetry == 0) {
          // First time — schedule retry
          labels404NextRetry = now + retryInterval;
          if (retryInterval < 60000) {
            Serial.println("[AUTO-RECOVERY] Label fetch failed (transient) - will retry in 15s");
          } else {
            Serial.println("[AUTO-RECOVERY] Device config invalid (404) - will retry in 2 minutes");
          }
        } else if (now >= labels404NextRetry) {
          labels404RetryCount++;
          if (labels404RetryCount > 6) {
            Serial.printf("[AUTO-RECOVERY] Label fetch failed %d times - rebooting\n", labels404RetryCount);
            delay(500);
            ESP.restart();
          }
          Serial.printf("[AUTO-RECOVERY] Retrying label fetch (attempt %d/6)...\n", labels404RetryCount);
          fetchSwitchLabels();
          // After retry: if still failing, use same interval strategy
          retryInterval = labelsLoadedSuccessfully ? 0 : (labelsValidationAttempted ? 15000UL : 120000UL);
          labels404NextRetry = now + retryInterval;
        }
        // Don't override the websocket = false that was set by fetchSwitchLabels()
        return;
      }
    }
    
    // ── Level monitoring: continuous bin-empty check (PIN 2 HIGH = empty) ────
    #ifdef PIN_LIGHT_BARRIER
    if (lightBarrierConfig.levelMonitoring) {
      bool pinIsLow = (digitalRead(PIN_LIGHT_BARRIER) == LOW);
      if (!pinIsLow && !lightBarrierConfig.binEmpty) {
        lightBarrierConfig.binEmpty = true;
        supplyBinEmptyScreen();
        Serial.println("[LEVEL MONITOR] Bin empty detected — payments blocked");
      } else if (pinIsLow && lightBarrierConfig.binEmpty) {
        lightBarrierConfig.binEmpty = false;
        Serial.println("[LEVEL MONITOR] Bin restocked — payments re-enabled");
        redrawQRScreen();
      }
    }
    #endif

    // ── Headless vending sensors: dual-sensor monitoring (GPIO 22 / GPIO 23) ────
    #ifdef PIN_SENSOR_1
    if (lightBarrierConfig.levelMonitoring) {
      bool pinIsLow = (digitalRead(PIN_SENSOR_1) == LOW);
      if (!pinIsLow && !lightBarrierConfig.binEmpty) {
        lightBarrierConfig.binEmpty = true;
        Serial.println("[SENSOR 1] Bin empty detected (GPIO 22) — payments blocked");
      } else if (pinIsLow && lightBarrierConfig.binEmpty) {
        lightBarrierConfig.binEmpty = false;
        Serial.println("[SENSOR 1] Bin restocked (GPIO 22) — payments re-enabled");
      }
    }
    if (lightBarrierConfig.monitoring) {
      bool pinIsLow = (digitalRead(PIN_SENSOR_1) == LOW);
      if (pinIsLow && !lightBarrierConfig.blocked) {
        lightBarrierConfig.blocked = true;
        Serial.println("[SENSOR 1] Product blockage detected (GPIO 22) — payments blocked");
      } else if (!pinIsLow && lightBarrierConfig.blocked) {
        lightBarrierConfig.blocked = false;
        Serial.println("[SENSOR 1] Product blockage cleared (GPIO 22) — payments re-enabled");
      }
    }
    #endif
    #ifdef PIN_SENSOR_2
    if (lightBarrierConfig.levelMonitoring2) {
      bool pinIsLow = (digitalRead(PIN_SENSOR_2) == LOW);
      if (!pinIsLow && !lightBarrierConfig.binEmpty2) {
        lightBarrierConfig.binEmpty2 = true;
        Serial.println("[SENSOR 2] Bin empty detected (GPIO 23) — payments blocked");
      } else if (pinIsLow && lightBarrierConfig.binEmpty2) {
        lightBarrierConfig.binEmpty2 = false;
        Serial.println("[SENSOR 2] Bin restocked (GPIO 23) — payments re-enabled");
      }
    }
    if (lightBarrierConfig.monitoring2) {
      bool pinIsLow = (digitalRead(PIN_SENSOR_2) == LOW);
      if (pinIsLow && !lightBarrierConfig.blocked2) {
        lightBarrierConfig.blocked2 = true;
        Serial.println("[SENSOR 2] Product blockage detected (GPIO 23) — payments blocked");
      } else if (!pinIsLow && lightBarrierConfig.blocked2) {
        lightBarrierConfig.blocked2 = false;
        Serial.println("[SENSOR 2] Product blockage cleared (GPIO 23) — payments re-enabled");
      }
    }
    #endif


    // ── GPIO 3 (T-Display-S3) / GPIO 46 (JC3248W535C) / GPIO 34 (headless): FD from NT3H2111 ──
    // Open-drain active LOW: phone near → FD LOW; no phone → pull-up → HIGH.
    // Extends PN532 RF pause while the phone field is active.
    #ifdef PIN_GPIO3
    {
      static bool lastFdState = false;
      bool fdLow = (digitalRead(PIN_GPIO3) == LOW);
      if (fdLow && !lastFdState) {
        Serial.println("[FD] Mobile phone approaching NFC Tag 2");
      } else if (!fdLow && lastFdState) {
        Serial.println("[FD] Mobile phone left NFC Tag 2");
      }
      lastFdState = fdLow;
      if (fdLow) {
        nfcConfig.pn532PauseUntil = millis() + 8000; // extend pause while phone field is active
      }
    }
    #endif

    // Process payments from queue
    if (paymentQueue.hasPending() && !paymentQueue.processing) {
      // Block payment activation while any sensor condition is active
      if (lightBarrierConfig.isAnyBlocking()) {
        Serial.println("[SENSOR] Payment skipped — sensor blocking active");
        vTaskDelay(pdMS_TO_TICKS(500));
      } else {
        paymentQueue.processing = true;
        String payloadStr = paymentQueue.dequeue();
        if (payloadStr.length() > 0) {
          Serial.printf("[QUEUE] Processing payment from queue. Remaining: %d\n", paymentQueue.size());
          processPaymentEvent(payloadStr);
        }
        paymentQueue.processing = false;
      }
    }

    // CRITICAL: Yield to other tasks to prevent tight loop
    // Without this, the loop runs thousands of times per second,
    // blocking WebSocket processing and causing connection loss
    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay between loop iterations
  }
  Serial.println("[LOOP] Exiting payment wait loop");
}

// --- Punkt 3: Modularized payment handling ---

// Helper function: Check if light barrier / sensor should stop the action
// Returns true if sensor is in "stop" mode AND minimum action time has passed
inline bool shouldStopForLightBarrier(unsigned long actionStartTime) {
  // Check if minimum action time has passed (2 seconds)
  unsigned long elapsed = millis() - actionStartTime;
  if (elapsed < lightBarrierConfig.minActionTime) {
    return false; // Too early to stop
  }

  #ifdef PIN_LIGHT_BARRIER
  if (lightBarrierConfig.enabled) {
    bool lightBarrierActive = (digitalRead(PIN_LIGHT_BARRIER) == LOW);
    if (lightBarrierActive) {
      Serial.printf("[LIGHT BARRIER] Triggered after %lu ms - stopping action!\n", elapsed);
      return true;
    }
  }
  #endif

  #ifdef PIN_SENSOR_1
  if (lightBarrierConfig.enabled) {
    if (digitalRead(PIN_SENSOR_1) == LOW) {
      Serial.printf("[SENSOR 1] Triggered after %lu ms - stopping action!\n", elapsed);
      return true;
    }
  }
  #endif
  #ifdef PIN_SENSOR_2
  if (lightBarrierConfig.enabled2) {
    if (digitalRead(PIN_SENSOR_2) == LOW) {
      Serial.printf("[SENSOR 2] Triggered after %lu ms - stopping action!\n", elapsed);
      return true;
    }
  }
  #endif


  #ifdef BOARD_ESP32C3_21_1
  if (c3FlexConfig.gpio6SensorStop && digitalRead(PIN_FLEX_CH01) == LOW) {
    Serial.printf("[FLEX] GPIO6 sensor-stop triggered after %lu ms\n", elapsed);
    return true;
  }
  if (c3FlexConfig.gpio7SensorStop && digitalRead(PIN_FLEX_CH02) == LOW) {
    Serial.printf("[FLEX] GPIO7 sensor-stop triggered after %lu ms\n", elapsed);
    return true;
  }
  #endif

  return false;
}

// Helper function: Check for product blockage after payment (monitor mode only).
// If sensor is still LOW after payment, the product exit is blocked.
// Shows the blocked screen, sets lightBarrierConfig.blocked, and waits until clear.
static void checkProductBlockage() {
  #ifdef PIN_LIGHT_BARRIER
  if (!lightBarrierConfig.monitoring) return; // Only active in "monitor" mode

  bool isBlocked = (digitalRead(PIN_LIGHT_BARRIER) == LOW);
  if (!isBlocked) return; // Path is clear — nothing to do

  Serial.println("[LIGHT BARRIER] Product blockage detected — waiting for clearance");
  lightBarrierConfig.blocked = true;
  productBlockedScreen();

  // Wait until the light barrier clears (product removed by customer or operator)
  while (digitalRead(PIN_LIGHT_BARRIER) == LOW) {
    webSocket.loop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  lightBarrierConfig.blocked = false;
  Serial.println("[LIGHT BARRIER] Product blockage cleared — ready for next payment");
  #endif

  // Headless sensors: blockage is handled in the main loop (continuous monitoring)
  // because headless has no display for a "blocked screen" — the LED fast blink
  // and WebSocket disconnect already signal the blockage state.
}

// Forward declarations for One For All task (defined after processThresholdPayment)
static volatile bool g_ofaStop = false;
struct OFATaskParams {
  int pin;
  int fallbackDuration;
};
static void oneForAllActivationTask(void* pvParams);

static void processThresholdPayment(const JsonDocument &doc)
{
  JsonVariantConst payment = doc["payment"];
  int payment_amount = payment["amount"].as<int>(); // in mSats
  int payment_sats = payment_amount / 1000; // Convert to sats
  int threshold_sats = lightningConfig.thresholdAmount.toInt();

  Serial.printf("[THRESHOLD] Payment received: %d sats (%d mSats)\n", payment_sats, payment_amount);
  Serial.printf("[THRESHOLD] Threshold: %d sats\n", threshold_sats);

  // Check if payment meets or exceeds threshold
  if (payment_sats >= threshold_sats) {
    Serial.println("[THRESHOLD] *** PAYMENT >= THRESHOLD! Triggering GPIO! ***");
    Serial.printf("[THRESHOLD] Switching GPIO %d for %d ms\n",
                  lightningConfig.thresholdPin.toInt(), lightningConfig.thresholdTime.toInt());

    // Pause product timeout while ACTION TIME is active
    productSelectionState.showTime = 0;
    
    // CRITICAL: Keep WebSocket alive during processing
    webSocket.loop();

    int pin = lightningConfig.thresholdPin.toInt();
    int duration = lightningConfig.thresholdTime.toInt();

    // Special mode only applies to Pin 12 (single-channel context).
    // Skip when: servo mode is active, or channel 4 ambient owns Pin 11.
    bool useSpecialMode = (specialModeConfig.mode != "standard" && specialModeConfig.mode != "")
                       && (multiChannelConfig.mode != "servo")
                       && !(channel4AmbientConfig.enabled && pin == 11);

    if (useSpecialMode) {
      Serial.println("[THRESHOLD] Using special mode: " + specialModeConfig.mode);
      if (deviceState.isInState(DeviceState::SCREENSAVER)) {
        deviceState.transition(DeviceState::READY);
        deactivateScreensaver();
      }
      activityTracking.lastActivityTime = millis();
      actionTimeScreen();
      updateActionTimeCountdown(duration / 1000);
      executeSpecialMode(pin, duration, specialModeConfig.frequency, specialModeConfig.dutyCycleRatio);
    } else {
      Serial.println("[THRESHOLD] Using standard mode");
      if (deviceState.isInState(DeviceState::SCREENSAVER)) {
        deviceState.transition(DeviceState::READY);
        deactivateScreensaver();
      }
      activityTracking.lastActivityTime = millis();
      actionTimeScreen();
      updateActionTimeCountdown(duration / 1000);
      
      // ── One For All mode support in Threshold ──────────────────────────────
      // When Pin 12 is threshold-pin in servo OFA mode, also trigger secondary channels.
      if (multiChannelConfig.mode == "servo" && servoConfig.oneForAll() && pin == 12) {
        Serial.println("[THRESHOLD-OFA] One For All: launching secondary channel activations");
        g_ofaStop = false; // Reset OFA stop flag
        
        // Pin 13 — Servo 1 (positional)
        if (servoConfig.servo1Active()) {
          int d13 = (productLabels.durations[1] > 0) ? productLabels.durations[1] : duration;
          OFATaskParams* params13 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
          if (params13) {
            params13->pin = 13; params13->fallbackDuration = d13;
            xTaskCreate(oneForAllActivationTask, "ofa_s1", 4096, params13, 2, nullptr);
            Serial.printf("[THRESHOLD-OFA] Launched servo1 task (Pin 13, hold=%d ms%s)\n", d13,
                          (productLabels.durations[1] > 0) ? " [own]" : " [fallback]");
          }
        }
        
        // Pin 10 — Servo 2 (continuous)
        if (servoConfig.servo2Active()) {
          int d10 = (servoConfig.servo2Duration > 0) ? servoConfig.servo2Duration
                   : (productLabels.durations[2] > 0) ? productLabels.durations[2]
                   : duration;
          OFATaskParams* params10 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
          if (params10) {
            params10->pin = 10; params10->fallbackDuration = d10;
            xTaskCreate(oneForAllActivationTask, "ofa_s2", 4096, params10, 2, nullptr);
            Serial.printf("[THRESHOLD-OFA] Launched servo2 task (Pin 10, dur=%d ms)\n", d10);
          }
        }
        
        // Pin 11 — Relay 2 (unless ambient-light mode)
        if (servoConfig.relay2Active() && !channel4AmbientConfig.enabled) {
          int d11 = (productLabels.durations[3] > 0) ? productLabels.durations[3] : duration;
          OFATaskParams* params11 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
          if (params11) {
            params11->pin = 11; params11->fallbackDuration = d11;
            xTaskCreate(oneForAllActivationTask, "ofa_r2", 2048, params11, 2, nullptr);
            Serial.printf("[THRESHOLD-OFA] Launched relay2 task (Pin 11, dur=%d ms%s)\n", d11,
                          (productLabels.durations[3] > 0) ? " [own]" : " [fallback]");
          }
        }
      }
      
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
      
      // CRITICAL: Non-blocking delay that keeps WebSocket alive
      unsigned long startTime = millis();
      int lastDisplayedSec = -1;
      while (millis() - startTime < duration) {
        // Check light barrier (stop early if triggered after minimum time)
        if (shouldStopForLightBarrier(startTime)) {
          Serial.println("[THRESHOLD] Light barrier stopped action early");
          // If OFA mode is active, signal secondary tasks to stop
          if (multiChannelConfig.mode == "servo" && servoConfig.oneForAll() && pin == 12) {
            g_ofaStop = true;
            Serial.println("[THRESHOLD-OFA] Signaling secondary tasks to stop");
          }
          break;
        }
        // Update countdown timer once per second
        unsigned long elapsed = millis() - startTime;
        int remaining = (int)((duration - elapsed) / 1000);
        if (remaining != lastDisplayedSec) {
          lastDisplayedSec = remaining;
          updateActionTimeCountdown(remaining);
        }
        webSocket.loop(); // Keep WebSocket connection alive
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
      }
      
      digitalWrite(pin, LOW);
      Serial.printf("[RELAY] Pin %d set LOW\n", pin);
    }

    thankYouScreen();
    activityTracking.lastActivityTime = millis();
    
    // CRITICAL: Non-blocking delay that keeps WebSocket alive
    unsigned long startTime = millis();
    while (millis() - startTime < 2000) {
      webSocket.loop(); // Keep WebSocket connection alive
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    }

    // Monitor mode: block next payment until product path is clear
    checkProductBlockage();
    
    // Reset timer AFTER thank you screen so full PRODUCT_TIMEOUT runs from now
    productSelectionState.showTime = millis();
    showThresholdQRScreen();
    Serial.println("[THRESHOLD] Ready for next payment");
    deviceState.transition(DeviceState::READY);
  } else {
    Serial.printf("[THRESHOLD] Payment too small (%d < %d sats) - ignoring\n",
                  payment_sats, threshold_sats);
  }
}

// ─── One For All: concurrent activation task ─────────────────────────────────
// Launched as a FreeRTOS task for each secondary channel (Pin 13, 10, 11) when
// servoConfig.relayMode == "one-for-all" and Pin 12 is triggered.

static void oneForAllActivationTask(void* pvParams) {
  OFATaskParams* p = (OFATaskParams*)pvParams;
  int pin             = p->pin;
  int fallback        = p->fallbackDuration;
  free(p);

  if (pin == 13) {
    if (servoConfig.pin13IsRelay) {
      // Pin 13 configured as external relay: switch HIGH for fallback duration
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      Serial.println("[OFA] Pin 13 set HIGH (relay mode)");
      unsigned long t0 = millis();
      while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback)) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      digitalWrite(pin, LOW);
      Serial.println("[OFA] Pin 13 set LOW (relay mode)");
    } else {
      // Servo 1 (positional 0-180°): sweep Start→End, hold at end for the
      // remaining time, then sweep back End→Start.
      // Hold = max(0, fallback - servo1Duration) so that total active time == fallback.
      activateServo(13); // sweep to end (takes servo1Duration ms)
      int holdTime = fallback - servoConfig.servo1Duration;
      if (holdTime > 0) {
        // Poll g_ofaStop so the light barrier can cut the hold short.
        unsigned long t0 = millis();
        while (!g_ofaStop && (millis() - t0 < (unsigned long)holdTime)) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (g_ofaStop) {
          Serial.println("[OFA] Pin 13 (servo1): stopped early by light barrier");
        }
      }
      deactivateServo(13); // sweep back to start
    }

  } else if (pin == 10) {
    if (servoConfig.pin10IsRelay) {
      // Pin 10 configured as external relay: switch HIGH for fallback duration
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      Serial.println("[OFA] Pin 10 set HIGH (relay mode)");
      unsigned long t0 = millis();
      while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback)) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      digitalWrite(pin, LOW);
      Serial.println("[OFA] Pin 10 set LOW (relay mode)");
    } else {
      // Servo 2 (continuous 360°): spin for servo2Duration ms.
      // If servo2Duration == 0 (indefinite), use fallback duration instead.
      activateServo(10);
      if (servoConfig.servo2Duration == 0 && fallback > 0) {
        // Poll g_ofaStop so the light barrier can stop the motor early.
        unsigned long t0 = millis();
        while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback)) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (g_ofaStop) {
          Serial.println("[OFA] Pin 10 (servo2): stopped early by light barrier");
        }
        deactivateServo(10); // Stop spinning
      }
    }
    // If servo2Duration > 0: activateServo() already stopped the motor internally.

  } else if (pin == 11) {
    // Relay 2: simple HIGH/LOW for fallback duration
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    Serial.printf("[OFA] Pin %d set HIGH\n", pin);
    // Poll g_ofaStop so the light barrier can cut the relay short.
    unsigned long t0 = millis();
    while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (g_ofaStop) {
      Serial.printf("[OFA] Pin %d: stopped early by light barrier\n", pin);
    }
    digitalWrite(pin, LOW);
    Serial.printf("[OFA] Pin %d set LOW\n", pin);
  }
#ifdef BOARD_ESP32C3_21_1
  else if (pin == PIN_FLEX_CH01) {
    if (c3FlexConfig.gpio6Servo180) {
      activateServo(PIN_FLEX_CH01); // sweep start → end
      int holdTime = fallback - c3FlexConfig.gpio6S180Duration;
      if (holdTime > 0) {
        unsigned long t0 = millis();
        while (!g_ofaStop && (millis() - t0 < (unsigned long)holdTime))
          vTaskDelay(pdMS_TO_TICKS(10));
      }
      deactivateServo(PIN_FLEX_CH01); // sweep end → start
    } else if (c3FlexConfig.gpio6Servo360) {
      activateServo(PIN_FLEX_CH01); // spin, self-stops when duration > 0
    }
  } else if (pin == PIN_FLEX_CH02) {
    if (c3FlexConfig.gpio7Servo180) {
      activateServo(PIN_FLEX_CH02);
      int holdTime = fallback - c3FlexConfig.gpio7S180Duration;
      if (holdTime > 0) {
        unsigned long t0 = millis();
        while (!g_ofaStop && (millis() - t0 < (unsigned long)holdTime))
          vTaskDelay(pdMS_TO_TICKS(10));
      }
      deactivateServo(PIN_FLEX_CH02);
    } else if (c3FlexConfig.gpio7Servo360) {
      activateServo(PIN_FLEX_CH02);
    }
  }
#endif
#ifdef BOARD_JC3248W535C
  else {
    int si = t35AmbientConfig.servoIndexForGpio(pin);
    if (si >= 0 && t35AmbientConfig.servo[si].isServo()) {
      // Touch 3.5 servo channel: sweep/spin with the channel's own parameters
      auto& sc = t35AmbientConfig.servo[si];
      if (sc.servo180) {
        activateServo(pin); // sweep start → end (takes s180Duration ms)
        int holdTime = fallback - sc.s180Duration;
        if (holdTime > 0) {
          unsigned long t0 = millis();
          while (!g_ofaStop && (millis() - t0 < (unsigned long)holdTime))
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        deactivateServo(pin); // sweep end → start
      } else { // servo360
        activateServo(pin); // self-stops when s360Duration > 0
        if (sc.s360Duration == 0) {
          unsigned long t0 = millis();
          while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback))
            vTaskDelay(pdMS_TO_TICKS(10));
          deactivateServo(pin); // stop spinning
        }
      }
      Serial.printf("[OFA-T35] GPIO%d servo action done\n", pin);
    } else {
      // Touch 3.5 relay channel (GPIO 6/7/14/15/16): HIGH for fallbackDuration, then LOW
      digitalWrite(pin, HIGH);
      Serial.printf("[OFA-T35] GPIO%d set HIGH\n", pin);
      unsigned long t0 = millis();
      while (!g_ofaStop && (millis() - t0 < (unsigned long)fallback)) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      if (g_ofaStop) Serial.printf("[OFA-T35] GPIO%d stopped early\n", pin);
      digitalWrite(pin, LOW);
      Serial.printf("[OFA-T35] GPIO%d set LOW\n", pin);
    }
  }
#endif

  vTaskDelete(nullptr);
}

static void processNormalPayment(int pin, int duration)
{
  Serial.printf("[RELAY] Pin: %d, Duration: %d ms\n", pin, duration);

  // Virtual pin mapping: 200–207 → PCF8574 I/O-Expander CH05–CH12 (channels 0–7)
#if ENABLE_DISPLAY
  if (pin >= 200 && pin <= 207) {
    int ch = pin - 200;
    if (!ioExpanderConfig.enabled) {
      Serial.printf("[IOExpander] ERROR: virtual pin %d received but I/O-Expander is disabled!\n", pin);
      return;
    }
    // Show action-time screen with countdown, same as physical pins
    productSelectionState.showTime = 0;
    activityTracking.lastActivityTime = millis();
    if (deviceState.isInState(DeviceState::SCREENSAVER)) {
      deactivateScreensaver();
      deviceState.transition(DeviceState::READY);
    }
    actionTimeScreen();
    updateActionTimeCountdown(duration / 1000);

    Serial.printf("[IOExpander] Activating CH%02d (P%d) for %d ms\n", ch + 5, ch, duration);
    activateExpanderChannel(ch);

    unsigned long startTime = millis();
    int lastDisplayedSec = duration / 1000;
    while (millis() - startTime < (unsigned long)duration) {
      int remaining = (int)((duration - (millis() - startTime)) / 1000);
      if (remaining != lastDisplayedSec) {
        lastDisplayedSec = remaining;
        updateActionTimeCountdown(remaining);
      }
      if (shouldStopForLightBarrier(startTime)) {
        break;
      }
      webSocket.loop();
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    deactivateExpanderChannel(ch);
    Serial.printf("[IOExpander] CH%02d (P%d) deactivated\n", ch + 5, ch);

    // Thank-you screen + 2s hold (same as physical pins)
    thankYouScreen();
    activityTracking.lastActivityTime = millis();
    unsigned long tyStart = millis();
    while (millis() - tyStart < 2000) {
      webSocket.loop();
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Reset timer and NFC state – same cleanup as end of processNormalPayment()
    productSelectionState.showTime = millis();
    multiChannelConfig.btcTickerActive = false;
    #if ENABLE_NFC
    extensionConfig.nfcPaymentPending = false;
    extensionConfig.nfcErrorDetail[0] = '\0';
    nfcPendingScreenShown = false;
    nfcNoLuckScreenShown  = false;
    nfcErrorDetailShown   = false;
    nfcNotSupportedShown  = false;
    #endif
    // Numeric selection (Touch 3.5): payment settled — clear the product QR
    // state so redrawQRScreen() returns to the main product-selection screen,
    // matching the physical-pin path (otherwise it stays on the same QR).
    #ifdef BOARD_JC3248W535C
    if (t35AmbientConfig.numericSelect) {
      productSelectState.resetAll();
      miniPosIdleNfcTag();
    }
    #endif
    redrawQRScreen();
    Serial.println("[NORMAL] Ready for next payment");
    return;
  }
#endif

  // Reset OFA stop flag so secondary tasks from any previous payment don't
  // carry over into this activation.
  g_ofaStop = false;

#if !ENABLE_DISPLAY
  // Whitelist check: only fire pins that are registered relay channels
  bool validPin = false;
  for (int i = 0; i < RELAY_CHANNEL_MAX; i++) {
    if (RELAY_CHANNEL_PINS[i] == pin) { validPin = true; break; }
  }
  if (!validPin) {
    Serial.printf("[RELAY] ERROR: GPIO %d is not a registered relay channel – ignoring!\n", pin);
    return;
  }
#endif

  // Pause product timeout while ACTION TIME is active
  productSelectionState.showTime = 0;

  // Special mode applies to relay pins (12, 11) but NOT servo pins (13, 10).
  // Skip when: this is a servo pin, or channel 4 ambient owns Pin 11.
  // In servo mode: Pin 13/10 are servo pins UNLESS configured as external relay
  bool isServoPin = (multiChannelConfig.mode == "servo" &&
                       ((pin == 13 && !servoConfig.pin13IsRelay) ||
                        (pin == 10 && !servoConfig.pin10IsRelay)))
                 || ((multiChannelConfig.mode == "servo180" || multiChannelConfig.mode == "servo360") && pin == 12);
  #ifdef BOARD_JC3248W535C
  // Touch 3.5 multi-channel: any flex channel set to servo180/servo360
  if (t35AmbientConfig.isServoGpio(pin)) isServoPin = true;
  #endif
  bool useSpecialMode = (specialModeConfig.mode != "standard" && specialModeConfig.mode != "")
                     && !isServoPin
                     && !(channel4AmbientConfig.enabled && pin == 11);

  // ── One For All mode ──────────────────────────────────────────────────────
  // When Pin 12 fires in OFA mode (servo), launch concurrent activations for
  // pins 13 (servo1), 10 (servo2), and 11 (relay2, unless ambient-light mode).
  // Each secondary channel uses its own configured duration as activation time;
  // if none is configured (== 0), Pin 12's payload duration is used as fallback.
  if (multiChannelConfig.mode == "servo" && servoConfig.oneForAll() && pin == 12) {
    Serial.println("[OFA] One For All: launching secondary channel activations");
    // Pin 13 — Servo 1 (positional)
    // Use Pin 13's own configured duration if set; fall back to Pin 12's duration.
    if (servoConfig.servo1Active()) {
      int d13 = (productLabels.durations[1] > 0) ? productLabels.durations[1] : duration;
      OFATaskParams* params13 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
      if (params13) {
        params13->pin = 13; params13->fallbackDuration = d13;
        xTaskCreate(oneForAllActivationTask, "ofa_s1", 4096, params13, 2, nullptr);
        Serial.printf("[OFA] Launched servo1 task (Pin 13, hold=%d ms%s)\n", d13,
                      (productLabels.durations[1] > 0) ? " [own]" : " [fallback from Pin 12]");
      }
    }
    // Pin 10 — Servo 2 (continuous)
    // If servo2Duration > 0: motor stops itself after that time internally.
    // If servo2Duration == 0: spin indefinitely — use Pin 10's own configured duration, then fallback.
    if (servoConfig.servo2Active()) {
      int d10 = (servoConfig.servo2Duration > 0) ? servoConfig.servo2Duration
               : (productLabels.durations[2] > 0) ? productLabels.durations[2]
               : duration;
      OFATaskParams* params10 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
      if (params10) {
        params10->pin = 10; params10->fallbackDuration = d10;
        xTaskCreate(oneForAllActivationTask, "ofa_s2", 4096, params10, 2, nullptr);
        Serial.printf("[OFA] Launched servo2 task (Pin 10, dur=%d ms)\n", d10);
      }
    }
    // Pin 11 — Relay 2 (skip if ambient lighting mode owns Pin 11)
    // Use Pin 11's own configured duration if set; fall back to Pin 12's duration.
    if (!channel4AmbientConfig.enabled) {
      int d11 = (productLabels.durations[3] > 0) ? productLabels.durations[3] : duration;
      OFATaskParams* params11 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
      if (params11) {
        params11->pin = 11; params11->fallbackDuration = d11;
        xTaskCreate(oneForAllActivationTask, "ofa_r2", 2048, params11, 2, nullptr);
        Serial.printf("[OFA] Launched relay2 task (Pin 11, dur=%d ms%s)\n", d11,
                      (productLabels.durations[3] > 0) ? " [own]" : " [fallback from Pin 12]");
      }
    }
  }
  // C3: launch flex channel servo tasks concurrently with the main payment action
  #ifdef BOARD_ESP32C3_21_1
  if (pin == PIN_RELAY) {
    if (c3FlexConfig.gpio6Servo180 || c3FlexConfig.gpio6Servo360) {
      OFATaskParams* p6 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
      if (p6) {
        p6->pin = PIN_FLEX_CH01; p6->fallbackDuration = duration;
        xTaskCreate(oneForAllActivationTask, "ofa_flex6", 4096, p6, 2, nullptr);
        Serial.printf("[FLEX-OFA] Launched GPIO6 servo task (dur=%d ms)\n", duration);
      }
    }
    if (c3FlexConfig.gpio7Servo180 || c3FlexConfig.gpio7Servo360) {
      OFATaskParams* p7 = (OFATaskParams*)malloc(sizeof(OFATaskParams));
      if (p7) {
        p7->pin = PIN_FLEX_CH02; p7->fallbackDuration = duration;
        xTaskCreate(oneForAllActivationTask, "ofa_flex7", 4096, p7, 2, nullptr);
        Serial.printf("[FLEX-OFA] Launched GPIO7 servo task (dur=%d ms)\n", duration);
      }
    }
  }
  #endif

  if (useSpecialMode) {
    Serial.println("[NORMAL] Using special mode: " + specialModeConfig.mode);
    activityTracking.lastActivityTime = millis();
    if (deviceState.isInState(DeviceState::SCREENSAVER)) {
      deactivateScreensaver();
      deviceState.transition(DeviceState::READY);
    }
    actionTimeScreen();
    updateActionTimeCountdown(duration / 1000);
    executeSpecialMode(pin, duration, specialModeConfig.frequency, specialModeConfig.dutyCycleRatio);
  } else {
    Serial.println("[NORMAL] Using standard mode");
    activityTracking.lastActivityTime = millis();
    if (deviceState.isInState(DeviceState::SCREENSAVER)) {
      deactivateScreensaver();
      deviceState.transition(DeviceState::READY);
    }
    actionTimeScreen();
    updateActionTimeCountdown(duration / 1000);
    // Servo mode: servo pins (13, 10) trigger servo directly, no GPIO relay
    if (isServoPin) {
      // Headless servo mode (servo180/servo360): activate GPIO 13 as LED indicator
      #if !ENABLE_DISPLAY
      if (pin == 12) {
        pinMode(PIN_RELAY_CH02, OUTPUT);
        digitalWrite(PIN_RELAY_CH02, HIGH);
        Serial.println("[RELAY] Pin 13 set HIGH (LED indicator, synced with Pin 12)");
      }
      #endif
      activateServo(pin);
    } else {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
      // ESP32-C3-21-1: activate GPIO6/GPIO7 flex channels together with GPIO4
      #ifdef BOARD_ESP32C3_21_1
      if (pin == PIN_RELAY) {
        if (c3FlexConfig.gpio6Relay) {
          pinMode(PIN_FLEX_CH01, OUTPUT);
          digitalWrite(PIN_FLEX_CH01, HIGH);
          Serial.printf("[RELAY] GPIO%d (flex CH01 relay) set HIGH\n", PIN_FLEX_CH01);
        }
        if (c3FlexConfig.gpio7Relay) {
          pinMode(PIN_FLEX_CH02, OUTPUT);
          digitalWrite(PIN_FLEX_CH02, HIGH);
          Serial.printf("[RELAY] GPIO%d (flex CH02 relay) set HIGH\n", PIN_FLEX_CH02);
        }
      }
      #endif
      // JC3248W535C One-for-All: launch each secondary actor channel as an independent task
      // so each channel runs for its own LNbits-configured duration (fallback: CH01 duration).
      #ifdef BOARD_JC3248W535C
      if (t35AmbientConfig.oneForAll && pin == PIN_RELAY_CH01) {
        const int ofaPins[] = {6, 7, 14, 15, 16};
        const bool act[] = {t35AmbientConfig.gpio6Actor, t35AmbientConfig.gpio7Actor,
                            t35AmbientConfig.gpio14Actor, t35AmbientConfig.gpio15Actor,
                            t35AmbientConfig.gpio16Actor};
        g_ofaStop = false;
        for (int i = 0; i < 5; i++) {
          if (act[i]) {
            int pidx = getPinIndex(ofaPins[i]);
            int chDur = (pidx >= 0 && productLabels.durations[pidx] > 0)
                        ? productLabels.durations[pidx] : duration;
            OFATaskParams* p = (OFATaskParams*)malloc(sizeof(OFATaskParams));
            if (p) {
              p->pin = ofaPins[i]; p->fallbackDuration = chDur;
              xTaskCreate(oneForAllActivationTask, "ofa_t35", 2048, p, 2, nullptr);
              Serial.printf("[OFA-T35] GPIO%d task launched (dur=%d ms%s)\n", ofaPins[i], chDur,
                            (pidx >= 0 && productLabels.durations[pidx] > 0) ? " [own]" : " [fallback]");
            }
          }
        }
      }
      #endif
    }

    if (multiChannelConfig.mode == "off" && pin == 12) {
      pinMode(13, OUTPUT);
      digitalWrite(13, HIGH);
      Serial.println("[RELAY] Pin 13 set HIGH (parallel to Pin 12 in Single mode)");
    }

    // Relay output mode: GPIO 22/23 switch together with Pin 12
    #if !ENABLE_DISPLAY
    if (pin == 12) {
      if (lightBarrierConfig.relayOutput) {
        digitalWrite(PIN_SENSOR_1, HIGH);
        Serial.println("[RELAY] GPIO 22 set HIGH (relay output, synced with Pin 12)");
      }
      if (lightBarrierConfig.relayOutput2) {
        digitalWrite(PIN_SENSOR_2, HIGH);
        Serial.println("[RELAY] GPIO 23 set HIGH (relay output, synced with Pin 12)");
      }
    }

    // Action start indicator: briefly turn off status LEDs for 300ms so the
    // onboard LED (GPIO 2) signals that the relay/action has just fired.
    #if PIN_LED_BUTTON_LED >= 0
    digitalWrite(PIN_LED_BUTTON_LED, LOW);
    #endif
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, LOW);
    #endif
    delay(300);
    #if PIN_LED_BUTTON_LED >= 0
    digitalWrite(PIN_LED_BUTTON_LED, HIGH);
    #endif
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, HIGH);
    #endif
    #endif

    // CRITICAL: Non-blocking delay that keeps WebSocket alive
    unsigned long startTime = millis();
    int lastDisplayedSec = -1;
    while (millis() - startTime < duration) {
      // Check light barrier (stop early if triggered after minimum time)
      if (shouldStopForLightBarrier(startTime)) {
        Serial.println("[NORMAL] Light barrier stopped action early");
        g_ofaStop = true; // Signal OFA secondary tasks to stop too
        break;
      }
      // Update countdown timer once per second
      unsigned long elapsed = millis() - startTime;
      int remaining = (int)((duration - elapsed) / 1000);
      if (remaining != lastDisplayedSec) {
        lastDisplayedSec = remaining;
        updateActionTimeCountdown(remaining);
      }
      webSocket.loop(); // Keep WebSocket connection alive
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    }

    // Servo mode: return servo to rest state; otherwise turn relay off
    if (isServoPin) {
      deactivateServo(pin);
      // Headless servo mode (servo180/servo360): deactivate GPIO 13 LED indicator
      #if !ENABLE_DISPLAY
      if (pin == 12) {
        digitalWrite(PIN_RELAY_CH02, LOW);
        Serial.println("[RELAY] Pin 13 set LOW (LED indicator, synced with Pin 12)");
      }
      #endif
    } else {
      digitalWrite(pin, LOW);
      Serial.printf("[RELAY] Pin %d set LOW\n", pin);
      // ESP32-C3-21-1: deactivate GPIO6/GPIO7 flex channels together with GPIO4
      #ifdef BOARD_ESP32C3_21_1
      if (pin == PIN_RELAY) {
        if (c3FlexConfig.gpio6Relay) {
          digitalWrite(PIN_FLEX_CH01, LOW);
          Serial.printf("[RELAY] GPIO%d (flex CH01 relay) set LOW\n", PIN_FLEX_CH01);
        }
        if (c3FlexConfig.gpio7Relay) {
          digitalWrite(PIN_FLEX_CH02, LOW);
          Serial.printf("[RELAY] GPIO%d (flex CH02 relay) set LOW\n", PIN_FLEX_CH02);
        }
      }
      #endif
      // JC3248W535C OFA: secondary tasks handle their own LOW — nothing to do here.
    }

    if (multiChannelConfig.mode == "off" && pin == 12) {
      digitalWrite(13, LOW);
      Serial.println("[RELAY] Pin 13 set LOW (parallel to Pin 12 in Single mode)");
    }

    // Relay output mode: GPIO 22/23 switch off together with Pin 12
    #if !ENABLE_DISPLAY
    if (pin == 12) {
      if (lightBarrierConfig.relayOutput) {
        digitalWrite(PIN_SENSOR_1, LOW);
        Serial.println("[RELAY] GPIO 22 set LOW (relay output, synced with Pin 12)");
      }
      if (lightBarrierConfig.relayOutput2) {
        digitalWrite(PIN_SENSOR_2, LOW);
        Serial.println("[RELAY] GPIO 23 set LOW (relay output, synced with Pin 12)");
      }
    }
    #endif
  }

  if (miniPosConfig.enabled) {
    miniPosPaidScreen();  // "PAID" + amount, shown 3 s
  } else {
    thankYouScreen();
  }
  activityTracking.lastActivityTime = millis();
  if (deviceState.isInState(DeviceState::SCREENSAVER)) {
    deactivateScreensaver();
    deviceState.transition(DeviceState::READY);
  }

  // CRITICAL: Non-blocking delay that keeps WebSocket alive
  unsigned long startTime = millis();
  unsigned long confirmScreenMs = miniPosConfig.enabled ? 3000 : 2000;
  while (millis() - startTime < confirmScreenMs) {
    webSocket.loop(); // Keep WebSocket connection alive
    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
  }

  // Monitor mode: block next payment until product path is clear
  checkProductBlockage();
  
  // Reset timer AFTER thank you screen so full PRODUCT_TIMEOUT runs from now
  productSelectionState.showTime = millis();
  // Force QR display (not ticker) after payment in ALWAYS mode
  multiChannelConfig.btcTickerActive = false;

  // Device is fully ready again – allow next NFC tap.
  // Also reset all NFC screen state so the monitoring block doesn't
  // trigger a redundant QR redraw after the payment is complete.
  #if ENABLE_NFC
  extensionConfig.nfcPaymentPending = false;
  extensionConfig.nfcErrorDetail[0] = '\0';
  nfcPendingScreenShown  = false;
  nfcNoLuckScreenShown   = false;
  nfcErrorDetailShown    = false;
  nfcNotSupportedShown   = false;
  pinPadState            = PinPadState();  // reset PIN pad so touch nav works again
  #endif

  // Mini-PoS: payment settled — reset state and put the idle URL on the NFC
  // tag so the following redraw shows an empty amount entry screen.
  if (miniPosConfig.enabled) {
    miniPosState.resetInvoice();
    miniPosState.resetInput();
    miniPosState.inputActive = true;
    miniPosState.lastInputActivity = millis();
    miniPosIdleNfcTag();
  }

  // Numeric selection: payment settled — back to the main screen, the NFC
  // tag carries the project URL again until the next product is selected.
  #ifdef BOARD_JC3248W535C
  if (t35AmbientConfig.numericSelect) {
    productSelectState.resetAll();
    miniPosIdleNfcTag();
  }
  #endif

  // Restore correct product QR (handles single, multi-channel, servo)
  redrawQRScreen();
  Serial.println("[NORMAL] Ready for next payment");
}

void processPaymentEvent(String &payloadStr)
{
  Serial.println("[PAYMENT] Payment detected!");
  Serial.printf("[PAYMENT] PayloadStr: %s\n", payloadStr.c_str());

  // ── PIN pad WS events (arrive before normal "paid" event) ────────────────
  if (payloadStr.startsWith("{")) {
    JsonDocument pinDoc;
    if (!deserializeJson(pinDoc, payloadStr)) {
      const char *event = pinDoc["event"];
      if (event && strcmp(event, "pin_required") == 0) {
        pinPadState = PinPadState();  // reset to defaults
        pinPadState.active      = true;
        pinPadState.activatedAt = millis();
        pinPadState.maxAttempts = pinDoc["max_attempts"] | 3;
        pinPadState.amountSat   = pinDoc["amount_sat"]   | 0L;
        pinPadState.sessionId   = pinDoc["session_id"]   | "";
        extensionConfig.nfcPaymentPending = false;
        LOG_INFO("PIN", String("PIN required – ") + String(pinPadState.amountSat) + " sat");
        showPinPadScreen(pinPadState);
        return;
      }
      if (event && strcmp(event, "pin_error") == 0) {
        if (!pinPadState.active) {
          LOG_INFO("PIN", "Ignoring pin_error – PIN pad no longer active");
          return;
        }
        // Ignore stale events from a previous session (e.g. old loop timing out
        // while a new tap already started a fresh session).
        const char *evtSession = pinDoc["session_id"] | "";
        if (*evtSession && pinPadState.sessionId != String(evtSession)) {
          LOG_INFO("PIN", String("Ignoring pin_error for stale session: ") + String(evtSession));
          return;
        }
        memset(pinPadState.digits, 0, sizeof(pinPadState.digits));
        pinPadState.numDigits    = 0;
        pinPadState.submitted    = false;
        pinPadState.pendingShown = false;
        pinPadState.attemptNum   = pinDoc["attempts"]    | (pinPadState.attemptNum + 1);
        pinPadState.errorMsg   = pinDoc["reason"]      | "Invalid PIN";
        pinPadState.showError  = true;
        pinPadState.errorStart = millis();
        pinPadState.blocked    = (pinPadState.attemptNum >= pinPadState.maxAttempts);
        LOG_INFO("PIN", String("PIN error – attempt ") + String(pinPadState.attemptNum)
                        + "/" + String(pinPadState.maxAttempts)
                        + (pinPadState.blocked ? " BLOCKED" : ""));
        showPinPadScreen(pinPadState);
        return;
      }
      // ── Authy (LNURL-auth) WS events ──────────────────────────────────
      if (event && strcmp(event, "auth_enrolled") == 0) {
        // A new wallet was registered during the teach session. Keep the
        // session open and fetch the next register challenge; show a hint.
        LOG_INFO("Authy", "Wallet enrolled");
        authyState.enrolledPrompt = true;
        authyState.infoMsg   = "Wallet registered";
        authyState.infoUntil = millis() + 4000;
        showAuthToast(authyState.infoMsg, false);
        authyState.needsRefresh = true;
        return;
      }
      if (event && strcmp(event, "teach_ended") == 0) {
        // Server closed the teach session (timeout) → back to the start screen.
        LOG_INFO("Authy", "Teach session ended (server) - back to start screen");
        authyState.reset();
        authyShowStart();
        return;
      }
    }
  }

  if (lightningConfig.thresholdKey.length() > 0) {
    Serial.println("[THRESHOLD] Processing payment in threshold mode...");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadStr);
    if (error) {
      Serial.print("[THRESHOLD] JSON parse error: ");
      Serial.println(error.c_str());
      return;
    }
    processThresholdPayment(doc);
  } else {
    Serial.println("[NORMAL] Processing payment in normal mode...");
    int pin      = getValue(payloadStr, '-', 0).toInt();
    int duration = getValue(payloadStr, '-', 1).toInt();

#ifdef BOARD_JC3248W535C
    // Numeric product selection: only accept a payment that matches the
    // product QR currently on screen.  A payment for a different pin (e.g.
    // an old invoice still pending in the payer's wallet) must be rejected
    // so the wrong relay is not triggered.
    // Numeric product selection guard: only active in multi-channel mode.
    // When mode-select at startup switches to Single/Mini-PoS/Authy,
    // multiChannelConfig.mode is "off" and the guard must not fire — otherwise
    // every payment would be silently dropped because productSelectState.qrActive
    // is never set in those modes.
    if (t35AmbientConfig.numericSelect && multiChannelConfig.mode == "duo") {
      if (!productSelectState.qrActive || productSelectState.qrPin <= 0) {
        LOG_WARN("NumSel", "Payment received but no product QR active — ignoring");
        return;
      }
      if (pin != productSelectState.qrPin) {
        LOG_WARN("NumSel", String("Payment pin ") + String(pin) +
                 " does not match displayed pin " + String(productSelectState.qrPin) +
                 " — rejected");
        return;
      }
    }
#endif

    processNormalPayment(pin, duration);

    // Authy: a successful identification fired the relay and spent the k1 —
    // return to the IDENTITY TRIGGER start screen (unless teaching).
    if (authyConfig.enabled && !authyState.teachActive) {
      authyShowStart();
    }
  }
}
