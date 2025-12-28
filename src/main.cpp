#include <Arduino.h>
#include <WiFi.h>
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
#include "TouchCST816S.h"
#include "DeviceState.h"
#include "GlobalState.h"

#define FORMAT_ON_FAIL true
#define PARAM_FILE "/config.json"

TaskHandle_t Task1;

String qrFormat = "bech32"; // "bech32" or "lud17"

// External LED button (PIN_LED_BUTTON_LED / PIN_LED_BUTTON_SW)
bool readyLedState = false; // Track current LED state to avoid redundant writes
bool initializationActive = true; // Startup/initialization phase flag for LED control
const unsigned long EXTERNAL_DEBOUNCE_MS = 50;
const unsigned long EXTERNAL_TRIPLE_WINDOW_MS = 2000;
const unsigned long EXTERNAL_HELP_HOLD_MS = 2000;
const unsigned long EXTERNAL_CONFIG_HOLD_MS = 3000;
const unsigned long CONFIG_EXIT_GUARD_MS = 2000; // Minimum time before button/touch can exit config

// Buttons
OneButton leftButton(PIN_BUTTON_1, true);
OneButton rightButton(PIN_BUTTON_2, true);

// Touch controller
TouchCST816S touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, PIN_TOUCH_RES, PIN_TOUCH_INT);

// Variables that remain here (not migrated to GlobalState)
String currency = "USD"; // Currency from config, default USD
bool labelsLoadedSuccessfully = false; // Track if labels were successfully fetched
String payloadStr;
String lnbitsServer;
String deviceId;
unsigned long configModeStartTime = 0; // Track when config mode started for touch exit
bool firstLoop = true; // Track first loop iteration
byte currentErrorType = 0; // 0=none, 1=WiFi (highest), 2=Internet, 3=Server, 4=WebSocket (lowest)
bool onErrorScreen = false; // Track if error screen is displayed (synchronized with DeviceState)
unsigned long lastInternetCheck = 0; // Track when we last checked Internet connectivity
byte consecutiveWebSocketFailures = 0; // Track consecutive WebSocket failures to detect Internet issues
bool needsQRRedraw = false; // Flag to trigger QR redraw after WiFi recovery
bool gestureHandledThisTouch = false; // Track if gesture was already handled in current touch session
unsigned long lastNavigationTime = 0; // Track time of last navigation for timeout-based reset
const unsigned long TOUCH_DOUBLE_CLICK_MS = 1000; // 1 second window for second click
const unsigned long TOUCH_LONG_PRESS_MS = 3000;  // 3 seconds for long press
const unsigned long BTC_UPDATE_INTERVAL = 300000; // 5 minutes in milliseconds
const unsigned long LABEL_UPDATE_INTERVAL = 300000; // 5 minutes in milliseconds
const unsigned long GRACE_PERIOD_MS = 1000;  // 1 second grace period after wake-up (reduced from 5s for better UX)

const unsigned long EXTERNAL_DEBOUNCE_MS = 50;
const unsigned long EXTERNAL_TRIPLE_WINDOW_MS = 2000;
const unsigned long EXTERNAL_HELP_HOLD_MS = 2000;
const unsigned long EXTERNAL_CONFIG_HOLD_MS = 3000;

// Product timeout: configurable via platformio.ini build flag PRODUCT_TIMEOUT
// Default 10 seconds for testing, use 60 seconds for production
// Used when: QR/Product shown → timeout → back to Ticker/ProductSelection
#ifndef PRODUCT_TIMEOUT
#define PRODUCT_TIMEOUT 10000
#endif

// BTC Ticker timeout: configurable via platformio.ini build flag BTCTICKER_TIMEOUT
// Default 10 seconds
// Used when: Ticker shown → timeout → back to QR (only in 'selecting' mode)
#ifndef BTCTICKER_TIMEOUT
#define BTCTICKER_TIMEOUT 10000
#endif

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

WebSocketsClient webSocket;

//////////////////FORWARD DECLARATIONS///////////////////

void reportMode();
void configMode();
void showHelp();
void fetchSwitchLabels();
void fetchBitcoinData();
void updateBitcoinTicker();
void updateSwitchLabels();
void updateLightningQR(String lnurlStr);
void navigateToNextProduct();
void handleExternalButton();
void handleExternalSingleClick();
void checkExternalButtonHolds();
void handleConfigExitButtons();
void updateReadyLed();
bool isReadyForReceive();
String generateLNURL(int pin);
String encodeBech32(const String& data);
bool wakeFromPowerSavingMode();

//////////////////HELPERS///////////////////

// Bech32 encoding helper - now defined in GlobalState.cpp, using extern from GlobalState.h

uint32_t bech32Polymod(const std::vector<uint8_t>& values) {
  uint32_t chk = 1;
  for (size_t i = 0; i < values.size(); ++i) {
    uint8_t top = chk >> 25;
    chk = (chk & 0x1ffffff) << 5 ^ values[i];
    if (top & 1) chk ^= 0x3b6a57b2;
    if (top & 2) chk ^= 0x26508e6d;
    if (top & 4) chk ^= 0x1ea119fa;
    if (top & 8) chk ^= 0x3d4233dd;
    if (top & 16) chk ^= 0x2a1462b3;
  }
  return chk;
}

std::vector<uint8_t> bech32HrpExpand(const String& hrp) {
  std::vector<uint8_t> ret;
  ret.reserve(hrp.length() * 2 + 1);
  for (size_t i = 0; i < hrp.length(); ++i) {
    ret.push_back(hrp[i] >> 5);
  }
  ret.push_back(0);
  for (size_t i = 0; i < hrp.length(); ++i) {
    ret.push_back(hrp[i] & 31);
  }
  return ret;
}

std::vector<uint8_t> convertBits(const uint8_t* data, size_t len, int frombits, int tobits, bool pad) {
  std::vector<uint8_t> ret;
  int acc = 0;
  int bits = 0;
  int maxv = (1 << tobits) - 1;
  for (size_t i = 0; i < len; i++) {
    int value = data[i];
    acc = (acc << frombits) | value;
    bits += frombits;
    while (bits >= tobits) {
      bits -= tobits;
      ret.push_back((acc >> bits) & maxv);
    }
  }
  if (pad) {
    if (bits > 0) {
      ret.push_back((acc << (tobits - bits)) & maxv);
    }
  } else if (bits >= frombits || ((acc << (tobits - bits)) & maxv)) {
    return std::vector<uint8_t>();
  }
  return ret;
}

String encodeBech32(const String& data) {
  String hrp = "lnurl";
  
  // Convert data to bytes (keep original case)
  std::vector<uint8_t> dataBytes;
  for (size_t i = 0; i < data.length(); i++) {
    dataBytes.push_back((uint8_t)data[i]);
  }
  
  // Convert from 8-bit to 5-bit
  std::vector<uint8_t> data5bit = convertBits(dataBytes.data(), dataBytes.size(), 8, 5, true);
  
  if (data5bit.empty()) {
    return "";
  }
  
  // Calculate checksum
  std::vector<uint8_t> combined = bech32HrpExpand(hrp);
  combined.insert(combined.end(), data5bit.begin(), data5bit.end());
  combined.insert(combined.end(), 6, 0);
  
  uint32_t polymod = bech32Polymod(combined) ^ 1;  // XOR with 1 for Bech32 (not Bech32m)
  std::vector<uint8_t> checksum;
  for (int i = 0; i < 6; ++i) {
    checksum.push_back((polymod >> (5 * (5 - i))) & 31);
  }
  
  // Build final string
  String result = hrp + "1";
  for (size_t i = 0; i < data5bit.size(); i++) {
    result += BECH32_CHARSET[data5bit[i]];
  }
  for (size_t i = 0; i < checksum.size(); i++) {
    result += BECH32_CHARSET[checksum[i]];
  }
  
  // Convert to uppercase for LNURL standard
  result.toUpperCase();
  
  return result;
}

// Helper function: Generate LNURL for a given pin
String generateLNURL(int pin) {
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    Serial.println("[LNURL] Cannot generate - server or deviceId not configured");
    return "";
  }
  
  // Build URL: https://{server}/bitcoinswitch/api/v1/lnurl/{deviceId}?pin={pin}
  String url = "https://" + lnbitsServer + "/bitcoinswitch/api/v1/lnurl/" + deviceId + "?pin=" + String(pin);
  
  Serial.printf("[LNURL] Generated for pin %d: %s\n", pin, url.c_str());
  
  if (qrFormat == "lud17") {
    // LUD17 format: replace https: with lnurlp:
    String result = url;
    result.replace("https:", "lnurlp:");
    Serial.printf("[LNURL] LUD17 format: %s\n", result.c_str());
    return result;
  } else {
    // BECH32 format (default)
    // Encode the https URL with bech32 and prepend lightning:
    String encoded = encodeBech32(url);
    if (encoded.length() == 0) {
      Serial.println("[LNURL] BECH32 encoding failed, falling back to URL format");
      return "lightning:" + url;
    }
    String result = "lightning:" + encoded;
    Serial.printf("[LNURL] BECH32 format: %s\n", result.c_str());
    return result;
  }
}

// Helper function: Update lightning QR code with given LNURL
void updateLightningQR(String lnurlStr) {
  lnurlStr.trim();
  
  // Don't convert to uppercase - keep original case (important for LUD17)
  if (lnurlStr.startsWith("lightning:") || lnurlStr.startsWith("LIGHTNING:")) {
    // Already has prefix, use as-is
    strcpy(lightningConfig.lightning, lnurlStr.c_str());
  } else {
    // No prefix, add it (lowercase for consistency)
    strcpy(lightningConfig.lightning, "lightning:");
    strcat(lightningConfig.lightning, lnurlStr.c_str());
  }
  
  Serial.print("[QR] Updated lightning QR: ");
  Serial.println(lightningConfig.lightning);
}

// Helper function: Navigate to next product in Multi-Channel-Control mode
void navigateToNextProduct() {
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    Serial.println("[NAV] Device woke up, not navigating");
    return; // Don't navigate, just wake up
  }
  
  Serial.println("[BUTTON] Navigate button pressed");
  
  if (multiChannelConfig.mode == "off") {
    // Single mode behavior depends on multiChannelConfig.btcTickerMode
    if (multiChannelConfig.btcTickerMode == "selecting") {
      if (multiChannelConfig.btcTickerActive) {
        // Already showing ticker - skip back to QR immediately
        Serial.println("[NAV] Single mode SELECTING - Skipping from ticker to QR");
        multiChannelConfig.btcTickerActive = false;
        String lnurlStr = generateLNURL(12);
        updateLightningQR(lnurlStr);
        if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
          showSpecialModeQRScreen();
        } else {
          showQRScreen();
        }
        productSelectionState.showTime = 0; // Reset timer
      } else {
        // Show Bitcoin ticker for 10 seconds
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        productSelectionState.showTime = millis(); // Start 10-second timer
        Serial.println("[NAV] Single mode with SELECTING - Showing Bitcoin ticker for 10 seconds");
      }
    } else {
      Serial.println("[NAV] Single mode - no navigation available");
    }
    return; // Single mode, no multi-product navigation
  }
  
  // Check if we're on product selection screen (multiChannelConfig.currentProduct == -1)
  if (multiChannelConfig.currentProduct == -1) {
    // Start from first product
    multiChannelConfig.currentProduct = 1;
  } else if (multiChannelConfig.btcTickerActive) {
    // If ticker is active, go back to first product
    multiChannelConfig.btcTickerActive = false;
    multiChannelConfig.currentProduct = 1;
    deviceState.transition(DeviceState::READY);
    Serial.println("[NAV] Ticker active - returning to first product");
  } else {
    multiChannelConfig.currentProduct++;
  }
  
  // Determine navigation behavior based on multiChannelConfig.btcTickerMode
  if (multiChannelConfig.btcTickerMode == "selecting") {
    // SELECTING mode: After last product, show BTC ticker (which will auto-return to product selection)
    if (multiChannelConfig.mode == "duo" && multiChannelConfig.currentProduct > 2) {
      multiChannelConfig.currentProduct = 0; // Reset for next navigation
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis(); // Start timer for auto-return
      Serial.println("[NAV] SELECTING mode - Showing Bitcoin ticker after last product");
      return;
    } else if (multiChannelConfig.mode == "quattro" && multiChannelConfig.currentProduct > 4) {
      multiChannelConfig.currentProduct = 0; // Reset for next navigation
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis(); // Start timer for auto-return
      Serial.println("[NAV] SELECTING mode - Showing Bitcoin ticker after last product");
      return;
    }
  } else {
    // ALWAYS or OFF mode: Loop back to first product
    if (multiChannelConfig.mode == "duo" && multiChannelConfig.currentProduct > 2) {
      multiChannelConfig.currentProduct = 1; // Loop back to first product
    } else if (multiChannelConfig.mode == "quattro" && multiChannelConfig.currentProduct > 4) {
      multiChannelConfig.currentProduct = 1; // Loop back to first product
    }
  }
  
  Serial.printf("[NAV] Navigate to product: %d\n", multiChannelConfig.currentProduct);
  
  // IMPORTANT: Disable product selection screen FIRST to prevent concurrent screen updates
  deviceState.transition(DeviceState::READY);
  
  // Small delay to ensure any ongoing display operation completes
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Show product QR screen (multiChannelConfig.currentProduct should be 1-4 for duo/quattro)
  if (multiChannelConfig.currentProduct >= 1) {
    // Show product QR screen
    multiChannelConfig.btcTickerActive = false; // Exit Bitcoin ticker when navigating to products
    
    // Capture multiChannelConfig.currentProduct value to prevent race conditions
    int productNum = multiChannelConfig.currentProduct;
    String label = "";
    int pin = 0;
    
    switch(productNum) {
      case 1: // Pin 12
        label = (productLabels.label12.length() > 0) ? productLabels.label12 : "Pin 12";
        pin = 12;
        break;
      case 2: // Pin 13
        label = (productLabels.label13.length() > 0) ? productLabels.label13 : "Pin 13";
        pin = 13;
        break;
      case 3: // Pin 10
        label = (productLabels.label10.length() > 0) ? productLabels.label10 : "Pin 10";
        pin = 10;
        break;
      case 4: // Pin 11
        label = (productLabels.label11.length() > 0) ? productLabels.label11 : "Pin 11";
        pin = 11;
        break;
      default: // Fallback to Pin 12 if invalid product number
        Serial.printf("[NAV] WARNING: Invalid product number %d, defaulting to Pin 12\n", productNum);
        label = (productLabels.label12.length() > 0) ? productLabels.label12 : "Pin 12";
        pin = 12;
        break;
    }
    
    // Generate LNURL dynamically for this pin
    String lnurlStr = generateLNURL(pin);
    if (lnurlStr.length() > 0) {
      updateLightningQR(lnurlStr);
    }
    
    // Show product screen
    showProductQRScreen(label, pin);
    Serial.printf("[NAV] Showing product: %s (Pin %d)\n", label.c_str(), pin);
    
    // Reset product selection timer after every navigation
    productSelectionState.showTime = millis();
    Serial.println("[NAV] Product selection timer reset");
  }
}

// Helper function: Wake from power saving modes
bool wakeFromPowerSavingMode() {
  // Check if we're in grace period after wake-up
  if (powerConfig.lastWakeUpTime > 0 && (millis() - powerConfig.lastWakeUpTime) < GRACE_PERIOD_MS) {
    Serial.println("[WAKE] Ignored - in grace period after wake-up");
    return true; // Indicate we're in grace period
  }
  
  // Clear wake-up timestamp once grace period has passed - allows subsequent touches to navigate normally
  if (powerConfig.lastWakeUpTime > 0) {
    Serial.println("[WAKE] Grace period expired, resuming normal operation");
    powerConfig.lastWakeUpTime = 0;
  }
  
  // Reset activity timer
  activityTracking.lastActivityTime = millis();
  
  // If powerConfig.screensaver or deep sleep was active, deactivate and return true
  if (deviceState.isInState(DeviceState::SCREENSAVER) || deviceState.isInState(DeviceState::DEEP_SLEEP)) {
    Serial.println("[WAKE] Waking from power saving mode");
    deviceState.transition(DeviceState::READY);
    deactivateScreensaver();
    powerConfig.lastWakeUpTime = millis();
    activityTracking.lastActivityTime = millis();
    return true; // Indicate we just woke up
  }
  
  return false; // Normal operation, no wake-up needed
}

  bool isReadyForReceive() {
    // LED ON when device is past init, not in error/config/help/report, and not in deep sleep
    return deviceState.getState() != DeviceState::INITIALIZING && !initializationActive && !deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && !deviceState.isInState(DeviceState::CONFIG_MODE) && !deviceState.isInState(DeviceState::HELP_SCREEN) && !deviceState.isInState(DeviceState::REPORT_SCREEN) && !deviceState.isInState(DeviceState::DEEP_SLEEP);
  }

  void updateReadyLed() {
    bool shouldBeOn = isReadyForReceive();
    if (shouldBeOn != readyLedState) {
      digitalWrite(PIN_LED_BUTTON_LED, shouldBeOn ? HIGH : LOW); // Source 3.3V when ready
      readyLedState = shouldBeOn;
      Serial.printf("[LED] Ready LED %s\n", shouldBeOn ? "ON" : "OFF");
    }
  }

// Helper function: Redraw appropriate QR screen based on mode
void redrawQRScreen() {
  Serial.println("[DISPLAY] Redrawing QR screen...");
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    Serial.println("[DISPLAY] Threshold QR screen displayed");
  } else if (multiChannelConfig.mode != "off") {
    // Multi-Channel-Control mode: Behavior depends on multiChannelConfig.btcTickerMode and multiChannelConfig.currentProduct
    if (multiChannelConfig.currentProduct == -1) {
      // Special value: product selection screen
      productSelectionScreen();
      deviceState.transition(DeviceState::PRODUCT_SELECTION);
      multiChannelConfig.btcTickerActive = false;
      Serial.println("[DISPLAY] Product selection screen displayed");
    } else if (multiChannelConfig.currentProduct == 0) {
      // Bitcoin ticker screen (only if ticker mode allows it)
      if (multiChannelConfig.btcTickerMode == "off") {
        // Should not show ticker if OFF, show product selection instead
        multiChannelConfig.currentProduct = -1;
        productSelectionScreen();
        deviceState.transition(DeviceState::PRODUCT_SELECTION);
        multiChannelConfig.btcTickerActive = false;
        Serial.println("[DISPLAY] BTC-Ticker OFF - Showing product selection screen");
      } else {
        // Show ticker for "always" or "selecting" modes
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        Serial.println("[DISPLAY] Bitcoin ticker screen displayed");
      }
    } else {
      String label = "";
      int displayPin = 0;
      
      switch(multiChannelConfig.currentProduct) {
        case 1:
          label = (productLabels.label12.length() > 0) ? productLabels.label12 : "Pin 12";
          displayPin = 12;
          break;
        case 2:
          label = (productLabels.label13.length() > 0) ? productLabels.label13 : "Pin 13";
          displayPin = 13;
          break;
        case 3:
          label = (productLabels.label10.length() > 0) ? productLabels.label10 : "Pin 10";
          displayPin = 10;
          break;
        case 4:
          label = (productLabels.label11.length() > 0) ? productLabels.label11 : "Pin 11";
          displayPin = 11;
          break;
      }
      
      // Generate LNURL dynamically for current product's pin
      String lnurlStr = generateLNURL(displayPin);
      updateLightningQR(lnurlStr);
      showProductQRScreen(label, displayPin);
      multiChannelConfig.btcTickerActive = false;
      Serial.printf("[DISPLAY] Product %d QR screen displayed\n", multiChannelConfig.currentProduct);
    }
    deviceState.transition(DeviceState::READY);
  } else if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
    // Generate LNURL for pin 12 before showing special mode QR
    String lnurlStr = generateLNURL(12);
    updateLightningQR(lnurlStr);
    showSpecialModeQRScreen();
    Serial.println("[DISPLAY] Special mode QR screen displayed");
  } else {
    // Generate LNURL for pin 12 before showing normal QR
    String lnurlStr = generateLNURL(12);
    updateLightningQR(lnurlStr);
    showQRScreen();
    Serial.println("[DISPLAY] Normal QR screen displayed");
  }
}

void executeSpecialMode(int pin, unsigned long duration_ms, float freq, float ratio) {
  Serial.println("[SPECIAL] Executing special mode:");
  Serial.printf("[SPECIAL] Pin: %d, Duration: %lu ms, Frequency: %.2f Hz, Ratio: %.2f\n", 
                pin, duration_ms, freq, ratio);
  
  // Calculate timing
  unsigned long period_ms = (unsigned long)(1000.0 / freq);
  unsigned long onTime_ms = (unsigned long)(period_ms / (1.0 + 1.0/ratio));
  unsigned long offTime_ms = period_ms - onTime_ms;
  
  Serial.printf("[SPECIAL] Period: %lu ms, ON: %lu ms, OFF: %lu ms\n", 
                period_ms, onTime_ms, offTime_ms);
  
  unsigned long startTime = millis();
  unsigned long elapsed = 0;
  int cycleCount = 0;
  
  pinMode(pin, OUTPUT);
  
  // If Single mode (multiChannelConfig.mode == "off") and pin is 12, also prepare pin 13
  bool parallelPin13 = (multiChannelConfig.mode == "off" && pin == 12);
  if (parallelPin13) {
    pinMode(13, OUTPUT);
    Serial.println("[SPECIAL] Pin 13 will be controlled in parallel to Pin 12 (Single mode)");
  }
  
  // Execute cycles until duration is reached
  while (elapsed < duration_ms) {
    // Check for config mode interrupt
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      Serial.println("[SPECIAL] Interrupted by config mode");
      digitalWrite(pin, LOW);
      if (parallelPin13) {
        digitalWrite(13, LOW);
      }
      break;
    }
    
    cycleCount++;
    
    // PIN HIGH
    digitalWrite(pin, HIGH);
    if (parallelPin13) {
      digitalWrite(13, HIGH);
    }
    Serial.printf("[SPECIAL] Cycle %d: Pin HIGH\n", cycleCount);
    delay(onTime_ms);
    
    // PIN LOW
    digitalWrite(pin, LOW);
    if (parallelPin13) {
      digitalWrite(13, LOW);
    }
    Serial.printf("[SPECIAL] Cycle %d: Pin LOW\n", cycleCount);
    delay(offTime_ms);
    
    elapsed = millis() - startTime;
  }
  
  // Ensure pin is LOW at the end
  digitalWrite(pin, LOW);
  if (parallelPin13) {
    digitalWrite(13, LOW);
  }
  Serial.printf("[SPECIAL] Completed %d cycles in %lu ms\n", cycleCount, elapsed);
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

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
    Serial.println("SSID: " + wifiConfig.ssid);

    const JsonObject maRoot1 = doc[1];
    const char *maRoot1Char = maRoot1["value"];
    wifiConfig.wifiPassword = maRoot1Char;
    Serial.println("Wifi pass: " + wifiConfig.wifiPassword);

    const JsonObject maRoot2 = doc[2];
    const char *maRoot2Char = maRoot2["value"];
    wifiConfig.switchStr = maRoot2Char;
    
    // Parse WebSocket URL (works with both ws:// and wss://)
    int protocolIndex = wifiConfig.switchStr.indexOf("://");
    Serial.printf("DEBUG: switchStr='%s', protocolIndex=%d\n", wifiConfig.switchStr.c_str(), protocolIndex);
    
    if (protocolIndex == -1) {
      Serial.println("Invalid switchStr: " + wifiConfig.switchStr);
      lnbitsServer = "";
      deviceId = "";
    } else {
      int domainIndex = wifiConfig.switchStr.indexOf("/", protocolIndex + 3);
      Serial.printf("DEBUG: domainIndex=%d\n", domainIndex);
      
      if (domainIndex == -1) {
        Serial.println("Invalid switchStr: " + wifiConfig.switchStr);
        lnbitsServer = "";
        deviceId = "";
      } else {
        int uidLength = 22; // Length of device ID
        lnbitsServer = wifiConfig.switchStr.substring(protocolIndex + 3, domainIndex);
        deviceId = wifiConfig.switchStr.substring(wifiConfig.switchStr.length() - uidLength);
        
        Serial.printf("DEBUG: Extracted server from index %d to %d\n", protocolIndex + 3, domainIndex);
      }
    }

    Serial.println("Socket: " + wifiConfig.switchStr);
    Serial.println("LNbits server: " + lnbitsServer);
    Serial.println("Switch device ID: " + deviceId);

    const JsonObject maRoot3 = doc[3];
    const char *maRoot3Char = maRoot3["value"];
    qrFormat = String(maRoot3Char);
    qrFormat.trim();
    if (qrFormat.length() == 0) {
      qrFormat = "bech32"; // Default
    }
    Serial.println("QR Format: " + qrFormat);

    // Screen displayConfig.orientation configuration (maRoot4)
    // Available options:
    // "h"  = horizontal (button right)
    // "v"  = vertical (button bottom)
    // "hi" = horizontal inverse (button left)
    // "vi" = vertical inverse (button top)
    const JsonObject maRoot4 = doc[4];
    const char *maRoot4Char = maRoot4["value"];
    displayConfig.orientation = maRoot4Char;
    Serial.println("Screen displayConfig.orientation: " + displayConfig.orientation);

    const JsonObject maRoot5 = doc[5];
    if (!maRoot5.isNull()) {
      const char *maRoot5Char = maRoot5["value"];
      displayConfig.theme = maRoot5Char;
    }
    Serial.println("Theme: " + displayConfig.theme);

    // Read threshold configuration (optional)
    const JsonObject maRoot6 = doc[6];
    if (!maRoot6.isNull()) {
      const char *maRoot6Char = maRoot6["value"];
      lightningConfig.thresholdKey = maRoot6Char;
    }

    const JsonObject maRoot7 = doc[7];
    if (!maRoot7.isNull()) {
      const char *maRoot7Char = maRoot7["value"];
      lightningConfig.thresholdAmount = maRoot7Char;
    }

    const JsonObject maRoot8 = doc[8];
    if (!maRoot8.isNull()) {
      const char *maRoot8Char = maRoot8["value"];
      lightningConfig.thresholdPin = maRoot8Char;
    }

    const JsonObject maRoot9 = doc[9];
    if (!maRoot9.isNull()) {
      const char *maRoot9Char = maRoot9["value"];
      lightningConfig.thresholdTime = maRoot9Char;
    }

    const JsonObject maRoot10 = doc[10];
    if (!maRoot10.isNull()) {
      const char *maRoot10Char = maRoot10["value"];
      lightningConfig.thresholdLnurl = maRoot10Char;
    }

    // Read special mode configuration (Index 11-13)
    const JsonObject maRoot11 = doc[11];
    if (!maRoot11.isNull()) {
      const char *maRoot11Char = maRoot11["value"];
      specialModeConfig.mode = maRoot11Char;
    }

    const JsonObject maRoot12 = doc[12];
    if (!maRoot12.isNull()) {
      const char *maRoot12Char = maRoot12["value"];
      specialModeConfig.frequency = String(maRoot12Char).toFloat();
      if (specialModeConfig.frequency < 0.1) specialModeConfig.frequency = 0.1;  // Min 0.1 Hz
      if (specialModeConfig.frequency > 10.0) specialModeConfig.frequency = 10.0; // Max 10 Hz
    }

    const JsonObject maRoot13 = doc[13];
    if (!maRoot13.isNull()) {
      const char *maRoot13Char = maRoot13["value"];
      specialModeConfig.dutyCycleRatio = String(maRoot13Char).toFloat();
      if (specialModeConfig.dutyCycleRatio < 0.1) specialModeConfig.dutyCycleRatio = 0.1;   // Min 1:10
      if (specialModeConfig.dutyCycleRatio > 10.0) specialModeConfig.dutyCycleRatio = 10.0; // Max 10:1
    }

    // Read powerConfig.screensaver and deep sleep configuration (optional, indices 14-16)
    const JsonObject maRoot14 = doc[14];
    if (!maRoot14.isNull()) {
      const char *maRoot14Char = maRoot14["value"];
      powerConfig.screensaver = maRoot14Char;
    }

    const JsonObject maRoot15 = doc[15];
    if (!maRoot15.isNull()) {
      const char *maRoot15Char = maRoot15["value"];
      powerConfig.deepSleep = maRoot15Char;
    }

    const JsonObject maRoot16 = doc[16];
    if (!maRoot16.isNull()) {
      const char *maRoot16Char = maRoot16["value"];
      powerConfig.activationTime = maRoot16Char;
      // Validate activation time (1-120 minutes)
      int actTime = String(powerConfig.activationTime).toInt();
      if (actTime < 1) powerConfig.activationTime = "1";
      if (actTime > 120) powerConfig.activationTime = "120";
    }

    // Read multi-channel-control configuration (index 17)
    const JsonObject maRoot17 = doc[17];
    if (!maRoot17.isNull()) {
      const char *maRoot17Char = maRoot17["value"];
      multiChannelConfig.mode = maRoot17Char;
    }

    // Read BTC-Ticker configuration (index 18)
    const JsonObject maRoot18 = doc[18];
    if (!maRoot18.isNull()) {
      const char *maRoot18Char = maRoot18["value"];
      multiChannelConfig.btcTickerMode = maRoot18Char;
      Serial.println("[CONFIG] Read multiChannelConfig.btcTickerMode from config: " + multiChannelConfig.btcTickerMode);
    } else {
      Serial.println("[CONFIG] Index 18 (multiChannelConfig.btcTickerMode) not found in config - using default: " + multiChannelConfig.btcTickerMode);
    }

    // Read currency configuration (index 19)
    const JsonObject maRoot19 = doc[19];
    if (!maRoot19.isNull()) {
      const char *maRoot19Char = maRoot19["value"];
      currency = String(maRoot19Char);
      Serial.println("[CONFIG] Read currency from config (before processing): " + currency);
      currency.toUpperCase(); // Ensure uppercase
      if (currency.length() == 0 || currency.length() > 3) {
        Serial.println("[CONFIG] Invalid currency length, using default USD");
        currency = "USD"; // Default fallback
      }
      Serial.println("[CONFIG] Final currency value: " + currency);
    } else {
      Serial.println("[CONFIG] Index 19 (currency) not found in config - using default: " + currency);
    }
    // Indices 18-20 removed (lnurl13, lnurl10, lnurl11 - now auto-generated)

    // Apply predefined mode settings
    if (specialModeConfig.mode == "blink") {
      specialModeConfig.frequency = 1.0;
      specialModeConfig.dutyCycleRatio = 1.0;
      Serial.println("[CONFIG] Applied 'blink' preset: 1 Hz, 1:1");
    } else if (specialModeConfig.mode == "pulse") {
      specialModeConfig.frequency = 2.0;
      specialModeConfig.dutyCycleRatio = 0.25; // 1:4 = 0.25
      Serial.println("[CONFIG] Applied 'pulse' preset: 2 Hz, 1:4");
    } else if (specialModeConfig.mode == "fast-blink") {
      specialModeConfig.frequency = 5.0;
      specialModeConfig.dutyCycleRatio = 1.0;
      Serial.println("[CONFIG] Applied 'fast-blink' preset: 5 Hz, 1:1");
    }
    
    Serial.println("Special Mode: " + specialModeConfig.mode);
    Serial.print("Frequency: ");
    Serial.print(specialModeConfig.frequency);
    Serial.println(" Hz");
    Serial.print("Duty Cycle Ratio: ");
    Serial.println(specialModeConfig.dutyCycleRatio);

    // Display Multi-Channel-Control configuration
    Serial.print("Multi-Channel-Control Mode: ");
    if (multiChannelConfig.mode == "off") {
      Serial.println("Single (Pin 12 only)");
    } else if (multiChannelConfig.mode == "duo") {
      Serial.println("Duo (Pins 12, 13) - LNURLs auto-generated");
    } else if (multiChannelConfig.mode == "quattro") {
      Serial.println("Quattro (Pins 12, 13, 10, 11) - LNURLs auto-generated");
    }

    // Display BTC-Ticker configuration
    Serial.println("\n================================");
    Serial.println("   BTC-TICKER CONFIGURATION");
    Serial.println("================================");
    Serial.print("BTC-Ticker Mode: ");
    Serial.println(multiChannelConfig.btcTickerMode);
    Serial.print("Currency: ");
    Serial.println(currency);
    Serial.println("================================\n");

    // Display mode based on threshold configuration
    Serial.println("\n================================");
    if (lightningConfig.thresholdKey.length() > 0) {
      Serial.println("       THRESHOLD MODE");
      Serial.println("================================");
      Serial.println("Threshold Key: " + lightningConfig.thresholdKey);
      Serial.println("Threshold Amount: " + lightningConfig.thresholdAmount + " sats");
      Serial.println("GPIO Pin: " + lightningConfig.thresholdPin);
      Serial.println("Control Time: " + lightningConfig.thresholdTime + " ms");
      
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
      Serial.println("        NORMAL MODE");
      Serial.println("================================");
    }

    // Display powerConfig.screensaver and deep sleep configuration
    Serial.println("\n================================");
    Serial.println("   POWER SAVING CONFIGURATION");
    Serial.println("================================");
    Serial.println("Screensaver: " + powerConfig.screensaver);
    Serial.println("Deep Sleep: " + powerConfig.deepSleep);
    Serial.println("Activation Time: " + powerConfig.activationTime + " minutes");
    
    // Convert activation time from minutes to milliseconds
    int activationTimeMinutes = String(powerConfig.activationTime).toInt();
    powerConfig.activationTimeoutMs = activationTimeMinutes * 60 * 1000UL;
    Serial.println("Activation Timeout: " + String(powerConfig.activationTimeoutMs) + " ms");
    
    // Determine and display active power saving mode
    if (powerConfig.screensaver != "off" && powerConfig.deepSleep == "off") {
      Serial.println("\n⚡ POWER SAVING MODE: SCREENSAVER");
      Serial.println("   Display backlight will turn off after " + powerConfig.activationTime + " minutes");
      Serial.println("   Press BOOT or IO14 button to wake up");
    } else if (powerConfig.deepSleep != "off" && powerConfig.screensaver == "off") {
      Serial.println("\n⚡ POWER SAVING MODE: DEEP SLEEP (" + powerConfig.deepSleep + ")");
      Serial.println("   Device will enter " + powerConfig.deepSleep + " sleep after " + powerConfig.activationTime + " minutes");
      Serial.println("   Press BOOT or IO14 button to wake up");
      Serial.println("Deep Sleep enabled - configuring GPIO wake-up sources");
      // Wake-up sources will be configured in setupDeepSleepWakeup() when sleep is triggered
    } else {
      Serial.println("\n⚡ POWER SAVING MODE: DISABLED");
      Serial.println("   Device will stay active continuously");
    }
    
    // Initialize last activity time
    activityTracking.lastActivityTime = millis();
    Serial.println("Last Activity Time initialized: " + String(activityTracking.lastActivityTime) + " ms");
    
    Serial.println("================================\n");
  }
  else
  {
    Serial.println("Config file not found - using defaults");
    displayConfig.orientation = "h";
    strcpy(lightningConfig.lightning, "LIGHTNING:lnurl1dp68gurn8ghj7ctsdyhxkmmvwp5jucm0d9hkuegpr4r33");
    Serial.println("\n================================");
    Serial.println("        NORMAL MODE");
    Serial.println("================================\n");
  }
  paramFile.close();
}

//////////////////WEBSOCKET///////////////////

// networkStatus.lastPongTime and networkStatus.waitingForPong are now in networkStatus struct

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  Serial.printf("[WS Event] Type: %d, ConfigMode: %d\n", type, (int)deviceState.isInState(DeviceState::CONFIG_MODE));
  
  if (!deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    switch (type)
    {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected!");
      break;
    case WStype_CONNECTED:
    {
      Serial.printf("[WS] Connected to url: %s\n", payload);
      webSocket.sendTXT("Connected");
      networkStatus.lastPongTime = millis(); // Reset pong timer on connect
      networkStatus.waitingForPong = false;
      networkStatus.confirmed.websocket = true; // Mark WebSocket as confirmed on first connect
      Serial.println("[CONFIRMED] WebSocket connection confirmed!");
      
      // Fetch switch labels from backend after successful connection
      fetchSwitchLabels();
    }
    break;
    case WStype_TEXT:
      Serial.printf("[WS] Received text: %s\n", payload);
      payloadStr = (char *)payload;
      Serial.printf("[WS] PayloadStr set to: %s\n", payloadStr.c_str());
      paymentStatus.paid = true;
      Serial.println("[WS] 'paymentStatus.paid' flag set to TRUE");
      break;
    case WStype_PING:
      Serial.println("[WS] Received Ping");
      break;
    case WStype_PONG:
      Serial.println("[WS] Received Pong - connection alive!");
      networkStatus.lastPongTime = millis();
      networkStatus.waitingForPong = false;
      break;
    case WStype_ERROR:
      Serial.println("[WS] Error occurred!");
      break;
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
    }
  }
  else
  {
    Serial.println("[WS] Event ignored - in config mode");
  }
}

// Fetch switch labels from backend API
void fetchSwitchLabels()
{
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    Serial.println("[LABELS] Cannot fetch labels - server or deviceId not configured");
    return;
  }

  HTTPClient http;
  String url = "https://" + lnbitsServer + "/bitcoinswitch/api/v1/public/" + deviceId;
  
  Serial.println("[LABELS] Fetching switch configurations from: " + url);
  http.begin(url);
  http.setTimeout(5000); // 5 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("[LABELS] Received response: " + payload);
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Extract currency from response
      const char* currencyChar = doc["currency"];
      Serial.print("[LABELS] DEBUG - currency field from API: ");
      if (currencyChar != nullptr) {
        Serial.println(String(currencyChar));
        String oldCurrency = currency;
        currency = String(currencyChar);
        currency.toUpperCase(); // Ensure uppercase for display and API calls
        Serial.println("[LABELS] Currency changed from '" + oldCurrency + "' to '" + currency + "'");
      } else {
        Serial.println("NULL - field not found in response");
        // Keep currency from config (don't override with USD)
        Serial.println("[LABELS] No currency in API response, keeping config value: " + currency);
      }
      
      // Clear existing labels
      productLabels.label12 = "";
      productLabels.label13 = "";
      productLabels.label10 = "";
      productLabels.label11 = "";
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        
        // Store label based on pin number
        if (pin == 12) {
          productLabels.label12 = labelStr;
          Serial.println("[LABELS] Pin 12 label: " + productLabels.label12);
        } else if (pin == 13) {
          productLabels.label13 = labelStr;
          Serial.println("[LABELS] Pin 13 label: " + productLabels.label13);
        } else if (pin == 10) {
          productLabels.label10 = labelStr;
          Serial.println("[LABELS] Pin 10 label: " + productLabels.label10);
        } else if (pin == 11) {
          productLabels.label11 = labelStr;
          Serial.println("[LABELS] Pin 11 label: " + productLabels.label11);
        }
      }
      
      Serial.println("[LABELS] Successfully fetched and cached all labels");
      labelsLoadedSuccessfully = true; // Mark labels as successfully loaded
      productLabels.lastUpdate = millis(); // Update timestamp
      
      // Always fetch Bitcoin data with the correct currency (not just when ticker is active)
      // This ensures data is ready when ticker is activated
      Serial.println("[LABELS] Currency received - fetching Bitcoin data with correct currency");
      fetchBitcoinData();
      
      // If ticker is currently active, redraw screen to show new currency
      if (multiChannelConfig.btcTickerActive) {
        Serial.println("[LABELS] Ticker active - refreshing display");
        btctickerScreen();
      }
    } else {
      Serial.println("[LABELS] JSON parsing failed: " + String(error.c_str()));
    }
  } else {
    Serial.printf("[LABELS] HTTP request failed with code: %d\n", httpCode);
  }
  
  http.end();
}

// Fetch Bitcoin data from CoinGecko and mempool.space
void fetchBitcoinData()
{
  Serial.println("[BTC] Fetching Bitcoin data...");
  HTTPClient http;

  // Fetch BTC price from CoinGecko using configured currency
  String currencyLower = currency;
  currencyLower.toLowerCase(); // CoinGecko expects lowercase currency code
  String apiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + currencyLower;
  
  Serial.println("[BTC] Current currency variable: '" + currency + "'");
  Serial.println("[BTC] Requesting price from CoinGecko with URL: " + apiUrl);
  http.begin(apiUrl);
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("[BTC] CoinGecko response: " + payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["bitcoin"].is<JsonObject>()) {
      Serial.println("[BTC] Looking for price key: '" + currencyLower + "'");
      float price = doc["bitcoin"][currencyLower];
      bitcoinData.price = String((int)price); // Convert to integer string
      Serial.println("[BTC] Price updated: " + bitcoinData.price + " " + currency);
    } else {
      Serial.println("[BTC] Failed to parse CoinGecko JSON");
      bitcoinData.price = "Error";
    }
  } else {
    Serial.printf("[BTC] CoinGecko request failed: %d\n", httpCode);
    bitcoinData.price = "Error";
  }
  http.end();

  delay(100); // Small delay between requests

  // Fetch block height from mempool.space
  Serial.println("[BTC] Requesting block height from mempool.space...");
  http.begin("https://mempool.space/api/blocks/tip/height");
  http.setTimeout(5000);

  httpCode = http.GET();
  if (httpCode == 200) {
    bitcoinData.blockHigh = http.getString();
    bitcoinData.blockHigh.trim();
    Serial.println("[BTC] Block height updated: " + bitcoinData.blockHigh);
  } else {
    Serial.printf("[BTC] Mempool.space request failed: %d\n", httpCode);
    bitcoinData.blockHigh = "Error";
  }
  http.end();

  Serial.println("[BTC] Bitcoin data fetch completed");
  bitcoinData.lastUpdate = millis();
}

// Update Bitcoin ticker display if needed
void updateBitcoinTicker()
{
  // Only update if ticker is active and not in error/config/help modes
  if (!multiChannelConfig.btcTickerActive || deviceState.isInState(DeviceState::ERROR_RECOVERABLE) || deviceState.isInState(DeviceState::CONFIG_MODE) || deviceState.isInState(DeviceState::HELP_SCREEN)) {
    return;
  }

  unsigned long currentTime = millis();

  // Check if it's time for an update
  if (currentTime - bitcoinData.lastUpdate >= BTC_UPDATE_INTERVAL) {
    Serial.println("[BTC] Update interval reached, fetching new data...");
    fetchBitcoinData();

    // Refresh the display ONLY if we're STILL on the ticker screen
    // Double-check multiChannelConfig.btcTickerActive immediately before drawing to prevent race conditions
    if (multiChannelConfig.btcTickerActive && !deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && !deviceState.isInState(DeviceState::PRODUCT_SELECTION)) {
      // Final check right before screen update to prevent concurrent drawing
      if (multiChannelConfig.btcTickerActive) {
        btctickerScreen();
        Serial.println("[BTC] Screen refreshed with new data");
      }
    }
  }
}

// Update switch labels periodically
void updateSwitchLabels()
{
  // Skip if in error/config/help modes
  if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE) || deviceState.isInState(DeviceState::CONFIG_MODE) || deviceState.isInState(DeviceState::HELP_SCREEN)) {
    return;
  }

  unsigned long currentTime = millis();

  // Check if labels failed to load initially or if it's time for periodic update
  if (!labelsLoadedSuccessfully || (currentTime - productLabels.lastUpdate >= LABEL_UPDATE_INTERVAL)) {
    if (!labelsLoadedSuccessfully) {
      Serial.println("[LABELS] Labels not loaded successfully, retrying...");
    } else {
      Serial.println("[LABELS] Periodic update interval reached, fetching labels...");
    }
    fetchSwitchLabels();
  }
}

// HTTP-based Internet check (doesn't require WebSocket connection)
bool checkInternetConnectivity()
{
  HTTPClient http;
  http.setTimeout(3000); // 3 second timeout
  
  Serial.println("[HTTP] Testing Internet connection...");
  http.begin("http://clients3.google.com/generate_204"); // Google's connectivity check
  
  int httpCode = http.GET();
  http.end();
  
  bool hasInternet = (httpCode == 204 || httpCode == 301 || httpCode == 302 || httpCode > 0);
  Serial.printf("[HTTP] Internet check result: %s (HTTP code: %d)\n", hasInternet ? "OK" : "FAILED", httpCode);
  
  return hasInternet;
}

// TCP-based Server reachability check (test if LNbits server port is open)
bool checkServerReachability()
{
  WiFiClient client;
  Serial.printf("[TCP] Testing server reachability: %s:443...\n", lnbitsServer.c_str());
  
  bool serverReachable = client.connect(lnbitsServer.c_str(), 443, 2000); // 2 second timeout (reduced from 3)
  
  if (serverReachable) {
    Serial.println("[TCP] Server is reachable (port 443 open)");
    client.stop();
  } else {
    Serial.println("[TCP] Server is NOT reachable (port 443 closed/timeout)");
  }
  
  return serverReachable;
}

// ============================================================================
// WiFi State Monitoring - Updates DeviceState based on WiFi connectivity
// ============================================================================
void checkWiFiStatus() {
  WiFiState newWiFiState;
  
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi disconnected
    newWiFiState = WiFiState::DISCONNECTED;
  } else if (!webSocket.isConnected()) {
    // WiFi connected but WebSocket not yet connected
    newWiFiState = WiFiState::CONNECTING;
  } else {
    // WiFi connected and WebSocket operational
    newWiFiState = WiFiState::CONNECTED;
  }
  
  // Update state machine with WiFi status
  // This will automatically trigger state transitions on connection loss/gain
  deviceState.updateWiFiState(newWiFiState);
}

void checkAndReconnectWiFi()
{
  // Simplified version - just show error screen, don't block
  if (WiFi.status() != WL_CONNECTED && !deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    Serial.println("WiFi connection lost!");
    if (networkStatus.errors.wifi < 99) networkStatus.errors.wifi++;
    Serial.printf("[ERROR] WiFi error count: %d\n", networkStatus.errors.wifi);
    
    if (!deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
      // Only show error screen if not already showing one
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 1; // WiFi error (highest priority)
      wifiReconnectScreen();
    }
    
    // Start WiFi reconnect but don't block waiting for it
    if (!networkStatus.confirmed.wifi) {
      // First time - configure WiFi
      WiFi.disconnect();
      delay(50);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(true);
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.wifiPassword.c_str());
      Serial.println("[RECOVERY] WiFi reconnection started (non-blocking)");
    }
    // If WiFi was confirmed before, auto-reconnect will handle it
  }
  else if (WiFi.status() == WL_CONNECTED && networkStatus.errors.wifi > 0 && deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && currentErrorType == 1)
  {
    // WiFi recovered while on error screen
    Serial.println("[RECOVERY] WiFi recovered!");
    networkStatus.confirmed.wifi = true;
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
    needsQRRedraw = true;
    activityTracking.lastActivityTime = millis();
    productSelectionState.showTime = millis();
    
    // Force BTC data refresh after WiFi recovery
    if (multiChannelConfig.btcTickerMode != "off") {
      Serial.println("[RECOVERY] Forcing BTC data refresh after WiFi recovery");
      bitcoinData.lastUpdate = 0; // Force immediate update
      fetchBitcoinData(); // Fetch data now
    }
  }
}

void configMode()
{
  Serial.println("[BUTTON] Config mode button pressed");
  
  // CRITICAL: Ensure serial is ready after deep sleep wake-up
  Serial.flush();
  delay(100); // Give serial time to stabilize
  
  // Verify serial is actually working
  Serial.println("[SERIAL_TEST] Testing serial connection...");
  Serial.flush();
  delay(50);
  
  configModeScreen(); // Draw config screen IMMEDIATELY
  deviceState.transition(DeviceState::CONFIG_MODE); // Then set flag
  configModeStartTime = millis(); // Track start to enforce 2s guard for exit
  updateReadyLed();
  
  // Set touch controller pointer for SerialConfig to access
  extern void* touchControllerPtr;
  touchControllerPtr = (void*)&touch;
  
  Serial.println("Config mode screen displayed, entering serial config...");
  Serial.println("Touch screen anywhere to exit config mode.");
  Serial.flush();
  
  bool hasExistingData = (wifiConfig.ssid.length() > 0);
  Serial.printf("Has existing data: %s\n", hasExistingData ? "YES" : "NO");
  Serial.flush();
  
  configOverSerialPort(wifiConfig.ssid, wifiConfig.wifiPassword, hasExistingData);
}

void reportMode()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    Serial.println("[REPORT] Device woke up, not entering report mode");
    return; // Don't trigger report mode, just wake up
  }
  
  // Ignore if we just entered config mode (prevents triggering on button release)
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    Serial.println("[REPORT] Ignored - in config mode");
    return;
  }
  
  Serial.println("[BUTTON] Report mode button pressed");
  deviceState.transition(DeviceState::REPORT_SCREEN); // Set flag to interrupt WiFi reconnect loop
  
  // Disable product selection timer during report mode
  productSelectionState.showTime = 0;
  deviceState.transition(DeviceState::READY);
  
  Serial.println("[REPORT] Showing error report screen");
  errorReportScreen(networkStatus.errors.wifi, networkStatus.errors.internet, networkStatus.errors.server, networkStatus.errors.websocket);
  Serial.println("[REPORT] Error report shown, waiting 2s");
  vTaskDelay(pdMS_TO_TICKS(2000)); // First screen: 2 seconds
  
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

void handleTouchButton()
{
  // If in Help mode: Allow second click to switch to Report
  if (deviceState.isInState(DeviceState::HELP_SCREEN)) {
    // Check for new button press
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
      touchState.pressed = true;
      touchState.pressStartTime = millis();
      
      // This is the second click - switch from Help to Report
      Serial.println("[TOUCH] Second click during Help -> Switching to Report Mode");
      deviceState.transition(DeviceState::REPORT_SCREEN); // Abort Help
      touchState.clickCount = 0; // Reset
      
      // Check for Config mode (display touch)
      if (touch.available()) {
        uint16_t mainTouchX = touch.getX();
        uint16_t mainTouchY = touch.getY();
        bool isMainAreaTouch = false;
        
        // Touch area detection based on displayConfig.orientation
        if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
          isMainAreaTouch = (mainTouchY <= 305);
        } else {
          isMainAreaTouch = (mainTouchX <= 145);
        }
        
        if (isMainAreaTouch) {
          Serial.println("[TOUCH] + Display touch -> Config Mode");
          configMode();
          return;
        }
      }
      
      // No display touch -> Report Mode
      reportMode();
    }
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
      touchState.pressed = false;
    }
    return;
  }
  
  // If in Report mode: Button press aborts
  if (deviceState.isInState(DeviceState::REPORT_SCREEN)) {
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
      Serial.println("[TOUCH] Button press during Report - ABORTING");
      deviceState.transition(DeviceState::READY);
      touchState.pressed = true;
    }
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
      touchState.pressed = false;
    }
    touchState.clickCount = 0;
    return;
  }
  
  // FIRST: Check if click sequence timeout has expired (ALWAYS check, not just on touch events)
  if (touchState.clickCount > 0 && !touchState.pressed) {
    unsigned long timeSinceLastTouch = millis() - touchState.lastTime;
    
    // For 1 click: Wait 1 second for potential second click
    // If no second click after 1s → Start Help
    if (touchState.clickCount == 1 && timeSinceLastTouch > 1000 && !deviceState.isInState(DeviceState::HELP_SCREEN)) {
      Serial.println("[TOUCH] Timeout: 1 click, no second click -> Help");
      showHelp();
      touchState.clickCount = 0;
    }
    
    // For 2 clicks: Wait 1 second for potential third click
    // If no third click after 1s → Start Report
    else if (touchState.clickCount == 2 && timeSinceLastTouch > 1000 && !deviceState.isInState(DeviceState::REPORT_SCREEN)) {
      Serial.println("[TOUCH] Timeout: 2 clicks, no third click -> Report");
      reportMode();
      touchState.clickCount = 0;
    }
    
    // For 3 clicks: Wait 1 second for potential fourth click
    // If no fourth click after 1s → Reset (do nothing)
    else if (touchState.clickCount == 3 && timeSinceLastTouch > 1000) {
      Serial.println("[TOUCH] Timeout: 3 clicks, no fourth click -> Reset");
      touchState.clickCount = 0;
    }
    
    // If Help is running and more than 3s passed since last click: Reset
    if (deviceState.isInState(DeviceState::HELP_SCREEN) && timeSinceLastTouch > 3000) {
      touchState.clickCount = 0;
    }
  }
  
  // Config Mode Touch Exit: Any touch after 2s exits config mode
  if (deviceState.isInState(DeviceState::CONFIG_MODE) && configModeStartTime > 0 && (millis() - configModeStartTime) >= CONFIG_EXIT_GUARD_MS) {
    if (digitalRead(PIN_TOUCH_INT) == LOW) {
      Serial.println("[CONFIG] Touch detected - exiting config mode");
      delay(100);
      ESP.restart();
    }
  }
  
  // Check if touch interrupt is triggered (GPIO 16 LOW when touched)
  if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
    // Touch detected - read coordinates to check if it's the button area
    uint16_t touchX = touch.getX();
    uint16_t touchY = touch.getY();
    
    // Define touch button area based on HARDWARE position
    // Touch coordinates are hardware-based (0-170 x 0-320), don't rotate with display!
    // Physical button is ALWAYS at the same hardware location: high Y values (Y > 305)
    // - Vertical (rotation=0): Button at BOTTOM of display → Y > 305
    // - Horizontal (rotation=1): Button at RIGHT of display → STILL Y > 305 (not X!)
    bool inButtonArea = (touchY > 305);
    
    // FIRST: Wake from powerConfig.screensaver if active (regardless of touch location)
    if (deviceState.isInState(DeviceState::SCREENSAVER)) {
      Serial.printf("[TOUCH] Display touched at X=%d, Y=%d during powerConfig.screensaver - WAKING UP\n", touchX, touchY);
      deviceState.transition(DeviceState::READY);
      deactivateScreensaver();
      powerConfig.lastWakeUpTime = millis();
      activityTracking.lastActivityTime = millis();
      // Don't process button click, just wake up
      return;
    }
    
    // If not in button area, update activity timer but don't process as button click
    if (!inButtonArea) {
      activityTracking.lastActivityTime = millis();
      return;
    }
    
    Serial.printf("[TOUCH] Button area touched at X=%d, Y=%d (displayConfig.orientation=%s)\n", 
                  touchX, touchY, displayConfig.orientation.c_str());
    
    // Touch button pressed
    touchState.pressed = true;
    touchState.pressStartTime = millis();
    
    // Increment click count if within double-click window
    unsigned long timeSinceLastTouch = millis() - touchState.lastTime;
    
    if (timeSinceLastTouch < TOUCH_DOUBLE_CLICK_MS && timeSinceLastTouch > 100) {
      // Within double-click window AND minimum 100ms since last touch (debounce)
      touchState.clickCount++;
      Serial.printf("[TOUCH] Click within window (%lu ms since last) - count now: %d\n", 
                    timeSinceLastTouch, touchState.clickCount);
    } else if (timeSinceLastTouch <= 100) {
      // Too fast - likely bounce or accidental double-click, ignore
      Serial.printf("[TOUCH] Too fast (%lu ms), ignoring (debounce)\n", timeSinceLastTouch);
      return;
    } else {
      touchState.clickCount = 1; // Reset to 1 for new click sequence
      Serial.printf("[TOUCH] New click sequence (last click was %lu ms ago)\n", timeSinceLastTouch);
    }
    touchState.lastTime = millis();
    
    Serial.printf("[TOUCH] Button click count: %d\n", touchState.clickCount);
    
    // Process clicks:
    // - Click 1: Wait for timeout (1s) → Help
    // - Click 2: Wait for timeout (1s) → Report (unless click 3 comes)
    // - Click 3: Wait for timeout (1s) → Nothing (waiting for click 4)
    // - Click 4: Immediate Config Mode
    if (touchState.clickCount == 4) {
      // Fourth click within timeout -> Config Mode (IMMEDIATE, no waiting)
      Serial.println("[TOUCH] Fourth click -> Config Mode");
      deviceState.transition(DeviceState::READY);  // Reset state before entering config
      configMode();
      touchState.clickCount = 0;
    }
    // For clicks 1, 2, and 3: Do nothing, let timeout handler decide
  }
  else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
    // Touch released
    touchState.pressed = false;
    unsigned long pressDuration = millis() - touchState.pressStartTime;
    
    Serial.printf("[TOUCH] Button released after %lu ms\n", pressDuration);
  }
}

void handleExternalSingleClick() {
  // Same behavior as NEXT/touch: wake powerConfig.screensaver/backlight and navigate/toggle ticker
  if (wakeFromPowerSavingMode()) {
    return;
  }
  navigateToNextProduct();
}

void handleExternalButton() {
  static int lastStableState = HIGH;
  static int lastRawState = HIGH;
  unsigned long now = millis();
  int rawState = digitalRead(PIN_LED_BUTTON_SW); // Pull-up, pressed = LOW

  // Detect raw state change and start debounce timer
  if (rawState != lastRawState) {
    externalButtonState.lastChange = now;
    lastRawState = rawState;
  }

  // Still debouncing - wait for stable signal
  if ((now - externalButtonState.lastChange) < EXTERNAL_DEBOUNCE_MS) {
    return;
  }

  // Debounce complete - check if stable state changed
  if (rawState == lastStableState) {
    return; // No actual state change after debounce
  }

  // State has changed and is stable - this is a real edge!
  int state = rawState;

  // Falling edge: button pressed
  if (state == LOW && lastStableState == HIGH) {
    Serial.println("[EXT_BTN] Button pressed (falling edge detected)");
    externalButtonState.pressed = true;
    externalButtonState.holdActionFired = false;
    externalButtonState.pressStartTime = now;

    // Start/refresh multi-click window
    if (externalButtonState.clickCount == 0 || (now - externalButtonState.sequenceStart) > EXTERNAL_TRIPLE_WINDOW_MS) {
      externalButtonState.clickCount = 0;
      externalButtonState.sequenceStart = now;
    }
  }

  // Rising edge: button released
  if (state == HIGH && lastStableState == LOW) {
    Serial.println("[EXT_BTN] Button released (rising edge detected)");
    externalButtonState.pressed = false;
    unsigned long pressDuration = now - externalButtonState.pressStartTime;
    Serial.printf("[EXT_BTN] Press duration: %lu ms\n", pressDuration);

    // If a hold action already fired, reset state
    if (externalButtonState.holdActionFired) {
      externalButtonState.holdActionFired = false;
      externalButtonState.clickCount = 0;
      externalButtonState.sequenceStart = 0;
    } else {
      // Treat as short click
      externalButtonState.clickCount++;
      Serial.printf("[EXT_BTN] Click count: %d\n", externalButtonState.clickCount);

      // Triple-click within window -> Report
      if (externalButtonState.clickCount >= 3 && (now - externalButtonState.sequenceStart) <= EXTERNAL_TRIPLE_WINDOW_MS) {
        Serial.println("[EXT_BTN] Triple click -> Report Mode");
        reportMode();
        externalButtonState.clickCount = 0;
        externalButtonState.sequenceStart = 0;
      } else {
        // Immediate single-click action (wake/navigate)
        handleExternalSingleClick();

        // Reset window if expired
        if ((now - externalButtonState.sequenceStart) > EXTERNAL_TRIPLE_WINDOW_MS) {
          externalButtonState.clickCount = 0;
          externalButtonState.sequenceStart = 0;
        }
      }
    }
  }

  // CRITICAL: Update lastStableState at the end so next call can detect changes
  lastStableState = state;
}

// Separate function to check hold actions - called continuously
void checkExternalButtonHolds() {
  if (!externalButtonState.pressed || externalButtonState.holdActionFired) {
    return;
  }

  unsigned long pressDuration = millis() - externalButtonState.pressStartTime;

  // Second-press hold >=3s → Config (double-click, hold on second)
  if (externalButtonState.clickCount == 1 && pressDuration >= EXTERNAL_CONFIG_HOLD_MS) {
    Serial.println("[EXT_BTN] Second press held >=3s -> Config Mode");
    externalButtonState.holdActionFired = true;
    externalButtonState.clickCount = 0;
    externalButtonState.sequenceStart = 0;
    configMode();
    return;
  }

  // Single long hold (first press) >=2s → Help
  if (externalButtonState.clickCount == 0 && pressDuration >= EXTERNAL_HELP_HOLD_MS) {
    Serial.println("[EXT_BTN] Long hold >=2s -> Help");
    externalButtonState.holdActionFired = true;
    externalButtonState.clickCount = 0;
    externalButtonState.sequenceStart = 0;
    showHelp();
    return;
  }
}

void handleConfigExitButtons() {
  if (!deviceState.isInState(DeviceState::CONFIG_MODE) || configModeStartTime == 0 || (millis() - configModeStartTime) < CONFIG_EXIT_GUARD_MS) {
    return;
  }

  static int prevNextState = HIGH;
  static int prevExtState = HIGH;

  int nextState = digitalRead(PIN_BUTTON_1);
  int extState = digitalRead(PIN_LED_BUTTON_SW);

  // Positive flank (release) exits config
  if (prevNextState == LOW && nextState == HIGH) {
    Serial.println("[CONFIG] NEXT button release -> exit config mode");
    ESP.restart();
  }

  if (prevExtState == LOW && extState == HIGH) {
    Serial.println("[CONFIG] External button release -> exit config mode");
    ESP.restart();
  }

  prevNextState = nextState;
  prevExtState = extState;
}

void showHelp()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    Serial.println("[HELP] Device woke up, not entering help mode");
    return; // Don't trigger help mode, just wake up
  }
  
  Serial.println("[BUTTON] Help button pressed");
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

void Task1code(void *pvParameters)
{
  for (;;)
  {
    // Monitor WiFi state and update device state machine
    checkWiFiStatus();
    
    leftButton.tick();
    rightButton.tick();

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

void setup()
{
  Serial.setRxBufferSize(2048); // Increased for long JSON with LNURL
  Serial.begin(115200);

  int timer = 0;

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // External LED-button wiring: source 3.3V on LED pin when ready; input uses pull-up
  pinMode(PIN_LED_BUTTON_LED, OUTPUT);
  digitalWrite(PIN_LED_BUTTON_LED, LOW); // LED off until device is ready
  pinMode(PIN_LED_BUTTON_SW, INPUT_PULLUP);

  FFat.begin(FORMAT_ON_FAIL);
  readFiles(); // get the saved details and store in global variables

  Serial.println("\n[SETUP] readFiles() completed");
  Serial.println("[SETUP] currency = " + currency);
  Serial.println("[SETUP] multiChannelConfig.btcTickerMode = " + multiChannelConfig.btcTickerMode);

  initDisplay();
  startupScreen();

  // Initialize touch controller (independent of WiFi)
  touchState.available = touch.begin();
  if (touchState.available) {
    Serial.println("[TOUCH] ✓ Touch controller initialized successfully!");
  } else {
    Serial.println("[TOUCH] ✗ Touch controller NOT available (non-touch version)");
  }

  // CRITICAL: Start button task BEFORE WiFi setup so config mode works during reconnect!
  leftButton.setPressMs(3000); // 3 seconds for config mode (documented as 5 sec for users)
  leftButton.setDebounceMs(50); // 50ms debounce - fast response
  leftButton.attachClick(navigateToNextProduct); // Single click = Navigate products (duo/quattro mode)
  leftButton.attachLongPressStart(configMode); // Long press = Config mode
  rightButton.setDebounceMs(50); // 50ms debounce - fast response
  rightButton.setClickMs(400); // 400ms max for single click
  rightButton.attachClick(showHelp); // Single click = Help
  rightButton.attachDoubleClick(reportMode); // Double click = Report
  rightButton.attachLongPressStart(showHelp); // Long press = Help (prevents missed clicks)

  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1",   /* Name of the task */
      10000,     /* Stack size in words */
      NULL,      /* Task input parameter */
      1,         /* Priority of the task (increased from 0 to 1) */
      &Task1,    /* Task handle. */
      0);        /* Core where the task should run */
  
  Serial.println("Button task created - config mode available");

  // Start WiFi connection BEFORE showing startup screen (parallel execution!)
  WiFi.mode(WIFI_STA); // Set to Station mode
  WiFi.setSleep(false); // Disable WiFi power saving for stable connection
  WiFi.setAutoReconnect(true); // Enable auto-reconnect
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.wifiPassword.c_str());
  Serial.println("[STARTUP] WiFi connection started in background (Power Save: OFF)");

  // Show startup screen for 5 seconds
  Serial.println("[STARTUP] Showing startup screen for 5 seconds...");
  for (int i = 0; i < 50; i++) { // 50 * 100ms = 5 seconds
    vTaskDelay(pdMS_TO_TICKS(100));
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      Serial.println("[STARTUP] Config mode triggered during startup");
      return;
    }
  }
  Serial.println("[STARTUP] Startup screen completed, switching to initialization screen");
  
  // Switch to initialization screen
  initializationScreen();
  
  // Continue initialization for max 20 more seconds (total 25s)
  // Exit early if all connections are successful
  Serial.println("[STARTUP] Showing initialization screen (max 20s more) while connections establish...");
  
  const int MAX_INIT_TIME = 200; // 200 * 100ms = 20 seconds (5s startup + 20s init = 25s total)
  bool allConnectionsReady = false;
  bool wifiStarted = true;
  bool internetChecked = false;
  bool serverChecked = false;
  bool websocketStarted = false;
  
  for (int i = 0; i < MAX_INIT_TIME; i++) {
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check for config mode
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      Serial.println("[STARTUP] Config mode triggered during startup");
      return;
    }
    
    // Step 1: Check WiFi (runs continuously until connected)
    if (!networkStatus.confirmed.wifi && WiFi.status() == WL_CONNECTED) {
      networkStatus.confirmed.wifi = true;
      Serial.println("[STARTUP] WiFi connected!");
    }
    
    // Step 2: Check Internet (once WiFi is connected and not yet checked)
    if (networkStatus.confirmed.wifi && !internetChecked) {
      Serial.println("[STARTUP] Checking Internet...");
      bool hasInternet = checkInternetConnectivity();
      internetChecked = true;
      if (hasInternet) {
        networkStatus.confirmed.internet = true;
        Serial.println("[STARTUP] Internet OK!");
      } else {
        Serial.println("[STARTUP] No Internet connection");
        if (networkStatus.errors.internet < 99) networkStatus.errors.internet++;
      }
    }
    
    // Step 3: Check Server (once Internet is confirmed and not yet checked)
    if (networkStatus.confirmed.internet && !serverChecked) {
      Serial.println("[STARTUP] Checking Server...");
      bool serverReachable = checkServerReachability();
      serverChecked = true;
      if (serverReachable) {
        networkStatus.confirmed.server = true;
        Serial.println("[STARTUP] Server OK!");
      } else {
        Serial.println("[STARTUP] Server not reachable");
        if (networkStatus.errors.server < 99) networkStatus.errors.server++;
      }
    }
    
    // Step 4: Start WebSocket (once Server is confirmed and not yet started)
    if (networkStatus.confirmed.server && !websocketStarted) {
      Serial.println("[STARTUP] Starting WebSocket connection...");
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
    if (websocketStarted && !networkStatus.confirmed.websocket) {
      webSocket.loop(); // Process events
      if (webSocket.isConnected()) {
        networkStatus.confirmed.websocket = true;
        Serial.println("[STARTUP] WebSocket connected!");
      }
    }
    
    // Check if all connections are ready
    if (networkStatus.confirmed.wifi && networkStatus.confirmed.internet && networkStatus.confirmed.server && networkStatus.confirmed.websocket) {
      allConnectionsReady = true;
      Serial.printf("[STARTUP] All connections ready after %.1f seconds!\n", (i + 1) * 0.1);
      break; // Exit startup screen early
    }
    
    // Progress indicator every 5 seconds
    if ((i + 1) % 50 == 0) {
      Serial.printf("[STARTUP] Progress: %.1fs - WiFi:%d Internet:%d Server:%d WS:%d\n", 
                    (i + 1) * 0.1, networkStatus.confirmed.wifi, networkStatus.confirmed.internet, networkStatus.confirmed.server, networkStatus.confirmed.websocket);
    }
  }
  
  Serial.println("[STARTUP] Startup screen completed");
  
  // Determine what to show after startup screen
  if (allConnectionsReady) {
    Serial.println("[STARTUP] All connections successful - ready to show QR code");
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
  } else {
    // Show appropriate error screen based on what failed (priority order)
    if (!networkStatus.confirmed.wifi) {
      Serial.println("[STARTUP] WiFi failed - showing WiFi error");
      wifiReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 1;
      if (networkStatus.errors.wifi < 99) networkStatus.errors.wifi++;
      
      // Handle WiFi failure
      if (wifiConfig.ssid.length() == 0) {
        configMode();
        return;
      }
      // Don't call checkAndReconnectWiFi here - it will be called below
      // This allows the loop to continue and handle touch/buttons
    } else if (!networkStatus.confirmed.internet) {
      Serial.println("[STARTUP] Internet failed - showing Internet error");
      internetReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 2;
    } else if (!networkStatus.confirmed.server) {
      Serial.println("[STARTUP] Server failed - showing Server error");
      serverReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 3;
    } else if (!networkStatus.confirmed.websocket) {
      Serial.println("[STARTUP] WebSocket failed - showing WebSocket error");
      websocketReconnectScreen();
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 4;
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
    Serial.println("[MULTI-CHANNEL-CONTROL] Quattro mode - 4 products available");
  } else if (multiChannelConfig.mode == "duo") {
    maxProducts = 2;
    Serial.println("[MULTI-CHANNEL-CONTROL] Duo mode - 2 products available");
  } else {
    maxProducts = 1;
    Serial.println("[MULTI-CHANNEL-CONTROL] Single mode - 1 product");
  }
  
  // Initialize product navigation
  multiChannelConfig.currentProduct = 0; // Start at selection screen

  // Fetch initial Bitcoin data after setup is complete
  Serial.println("[BTC] Fetching initial Bitcoin data...");
  fetchBitcoinData();
  Serial.println("[BTC] Initial fetch complete");
  
  // Setup complete - device state already set appropriately above
}

void loop()
{
  // Wait for setup to complete before running loop
  if (deviceState.getState() == DeviceState::INITIALIZING)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }

  // Update ready LED state regularly
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
    if (powerConfig.deepSleep == "light") {
      Serial.println("Deep Sleep: light sleep mode");
    } else if (powerConfig.deepSleep == "freeze") {
      Serial.println("Deep Sleep: deep sleep (freeze) mode");
    } else {
      Serial.println("Deep Sleep: " + powerConfig.deepSleep);
    }
    
    Serial.println("Activation Time: " + String(powerConfig.activationTimeoutMs / 60000) + " minutes (" + String(powerConfig.activationTimeoutMs) + " ms)");
    Serial.println("screensaverActive: " + String(deviceState.isInState(DeviceState::SCREENSAVER)));
    Serial.println("deepSleepActive: " + String(deviceState.isInState(DeviceState::DEEP_SLEEP)));
    Serial.println("activityTracking.lastActivityTime: " + String(activityTracking.lastActivityTime));
    
    if (powerConfig.screensaver != "off" && powerConfig.deepSleep == "off") {
      Serial.println("\n⚡ MODE: SCREENSAVER ENABLED");
      Serial.println("   Backlight will turn off after inactivity");
    } else if (powerConfig.deepSleep != "off" && powerConfig.screensaver == "off") {
      Serial.println("\n⚡ MODE: DEEP SLEEP ENABLED (" + powerConfig.deepSleep + ")");
      Serial.println("   Device will sleep after inactivity");
    } else {
      Serial.println("\n⚡ MODE: POWER SAVING DISABLED");
    }
    Serial.println("================================\n");
  }
  
  // Screensaver and deep sleep checks are now inside the payment wait loop
  // to ensure they execute during payment waiting
  
  // If in config mode, do nothing - config mode is handled by button interrupt
  if (deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
  
  checkAndReconnectWiFi();
  Serial.println("[DEBUG] Returned from checkAndReconnectWiFi()");
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) return; // Exit if we entered config mode
  
  // Handle QR redraw after WiFi recovery (outside of deep call stack)
  if (needsQRRedraw) {
    Serial.println("[RECOVERY] Redrawing QR screen after WiFi recovery");
    redrawQRScreen();
    needsQRRedraw = false;
    Serial.println("[RECOVERY] QR screen redrawn successfully");
  }

  payloadStr = "";
  Serial.println("[DEBUG] About to check connections...");
  
  // CRITICAL: Only show QR screen ONCE on first loop if ALL connections confirmed
  bool allConnectionsConfirmed = networkStatus.confirmed.wifi && networkStatus.confirmed.internet && networkStatus.confirmed.server && networkStatus.confirmed.websocket;
  Serial.printf("[DEBUG] allConnectionsConfirmed: %d, firstLoop: %d\n", allConnectionsConfirmed, firstLoop);
  
  if (firstLoop && allConnectionsConfirmed && !deviceState.isInState(DeviceState::REPORT_SCREEN) && !(powerConfig.lastWakeUpTime > 0 && (millis() - powerConfig.lastWakeUpTime) < GRACE_PERIOD_MS)) {
    Serial.println("[SCREEN] All connections confirmed - Showing QR code screen (READY FOR ACTION)");
    if (lightningConfig.thresholdKey.length() > 0) {
      showThresholdQRScreen(); // THRESHOLD MODE (Multi-Channel-Control not compatible)
    } else if (multiChannelConfig.mode != "off") {
      // MULTI-CHANNEL-CONTROL MODE - Behavior depends on multiChannelConfig.btcTickerMode
      if (multiChannelConfig.btcTickerMode == "off") {
        // OFF: Show product selection screen for Duo/Quattro (no ticker)
        multiChannelConfig.currentProduct = -1; // Special value to indicate product selection screen
        productSelectionScreen();
        deviceState.transition(DeviceState::PRODUCT_SELECTION);
        multiChannelConfig.btcTickerActive = false;
        productSelectionState.showTime = millis();
        Serial.println("[BTC] BTC-Ticker OFF - Starting with product selection screen");
      } else if (multiChannelConfig.btcTickerMode == "always") {
        // ALWAYS: Show Bitcoin ticker (current behavior)
        multiChannelConfig.currentProduct = 0;
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        deviceState.transition(DeviceState::READY);
        productSelectionState.showTime = millis();
        Serial.println("[BTC] BTC-Ticker ALWAYS - Starting with Bitcoin ticker screen");
      } else if (multiChannelConfig.btcTickerMode == "selecting") {
        // SELECTING: Show product selection screen for Duo/Quattro
        multiChannelConfig.currentProduct = -1; // Special value to indicate product selection screen
        productSelectionScreen();
        deviceState.transition(DeviceState::PRODUCT_SELECTION);
        multiChannelConfig.btcTickerActive = false;
        productSelectionState.showTime = millis();
        Serial.println("[BTC] BTC-Ticker SELECTING - Starting with product selection screen");
      }
    } else if (specialModeConfig.mode != "standard") {
      // Generate LNURL for pin 12 before showing special mode QR
      String lnurlStr = generateLNURL(12);
      updateLightningQR(lnurlStr);
      showSpecialModeQRScreen(); // SPECIAL MODE
    } else {
      // SINGLE MODE - Behavior depends on multiChannelConfig.btcTickerMode
      if (multiChannelConfig.btcTickerMode == "always") {
        // ALWAYS: Show Bitcoin ticker first
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        productSelectionState.showTime = millis();
        Serial.println("[BTC] BTC-Ticker ALWAYS (Single mode) - Starting with Bitcoin ticker screen");
      } else {
        // OFF or SELECTING: Show normal QR (selecting mode shows ticker after NEXT button)
        String lnurlStr = generateLNURL(12);
        updateLightningQR(lnurlStr);
        showQRScreen(); // NORMAL MODE
        multiChannelConfig.btcTickerActive = false;
        Serial.println("[BTC] BTC-Ticker " + multiChannelConfig.btcTickerMode + " (Single mode) - Starting with QR screen");
      }
    }
    // Clear error screen flag once QR is shown
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
    // Start product selection timer
    productSelectionState.showTime = millis();
    deviceState.transition(DeviceState::READY);
  } else if (firstLoop && !allConnectionsConfirmed) {
    Serial.printf("[SCREEN] First loop - waiting for all connections (WiFi:%d, Internet:%d, Server:%d, WS:%d)\n", 
                  networkStatus.confirmed.wifi, networkStatus.confirmed.internet, networkStatus.confirmed.server, networkStatus.confirmed.websocket);
  }
  
  firstLoop = false; // Mark first loop as completed

  unsigned long lastWiFiCheck = millis();
  networkStatus.lastPingTime = millis(); // Initialize global variable
  unsigned long loopCount = 0;
  // Don't reset onErrorScreen/currentErrorType - they should persist across loop iterations
  
  Serial.println("[LOOP] Entering payment wait loop...");
  Serial.printf("[LOOP] Initial paymentStatus.paid state: %d\n", paymentStatus.paid);
  Serial.printf("[LOOP] Error state: onErrorScreen=%d, currentErrorType=%d\n", deviceState.isInState(DeviceState::ERROR_RECOVERABLE), currentErrorType);
  
  // Initialize ping/pong tracking
  networkStatus.lastPongTime = millis();
  networkStatus.waitingForPong = false;
  
  // Debug counter for loop iterations
  static unsigned long loopIterations = 0;
  static unsigned long lastLoopDebugPrint = 0;
  
  while (paymentStatus.paid == false)
  {
    loopIterations++;
    
    // Print debug info every 10 seconds
    if (millis() - lastLoopDebugPrint > 10000) {
      Serial.printf("[LOOP_DEBUG] Iterations: %lu, touchState.available: %d, inConfigMode: %d, onErrorScreen: %d\n", 
                    loopIterations, touchState.available, deviceState.isInState(DeviceState::CONFIG_MODE), deviceState.isInState(DeviceState::ERROR_RECOVERABLE));
      lastLoopDebugPrint = millis();
    }
    // Check if config mode was triggered during payment wait
    if (deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      Serial.println("[LOOP] Config mode detected, exiting payment loop");
      return;
    }
    
    // Check for touch input (if available)
    static unsigned long lastTouchEvent = 0;
    static bool wasTouched = false;
    static bool actionExecutedThisTouch = false; // Track if action already executed for current touch
    static unsigned long lastActionTime = 0; // Track when last action was executed for debouncing
    static unsigned long lastTouchDebugPrint = 0;
    
    if (touchState.available && !deviceState.isInState(DeviceState::CONFIG_MODE)) {
      // FIRST: Check touch interrupt for powerConfig.screensaver wake-up (even if no new data available)
      // This ensures we can wake from powerConfig.screensaver by touching anywhere on the screen
      int touchIntState = digitalRead(PIN_TOUCH_INT);
      
      // Debug: Print touch interrupt state every 5 seconds during powerConfig.screensaver
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
        // Give touch controller time to process and continue to next iteration
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      
      // Check for actual touch event
      // Note: Minimal debouncing for main area, button has its own 20ms debounce
      if (touch.available() && (millis() - lastTouchEvent > 10)) {
        uint8_t gesture = touch.getGesture();
        uint16_t x = touch.getX();
        uint16_t y = touch.getY();
        bool isTouched = touch.isPressed();
        
        // FIRST: Check if touch is in button area
        // Touch coordinates are hardware-based (0-170 x 0-320), don't rotate with display!
        // Physical button is ALWAYS at Y > 305, regardless of display rotation
        bool inButtonArea = (y > 305);
        
        if (inButtonArea) {
          // Update activity timer but don't process as product navigation
          activityTracking.lastActivityTime = millis();
          lastTouchEvent = millis();
          // Skip the rest of touch processing - button handler in Task1 will handle this
          goto skip_product_touch_processing;
        }
        
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
          // Update activity timer to prevent powerConfig.screensaver from activating again
          activityTracking.lastActivityTime = millis();
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
          // Also accept quick touch without gesture (GESTURE_NONE)
          // BUT only on NEW touch to prevent continuous triggering
          else if (gesture == GESTURE_NONE && isTouched && !wasTouched) {
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
              } else {
                // Show ticker for 10 seconds
                Serial.println("Show Bitcoin ticker for 10 seconds");
                btctickerScreen();
                multiChannelConfig.btcTickerActive = true;
                productSelectionState.showTime = millis();
              }
            }
            // Single Mode with SELECTING: Show ticker on demand
            else if (multiChannelConfig.mode == "off" && multiChannelConfig.btcTickerMode == "selecting") {
              if (multiChannelConfig.btcTickerActive) {
                // Already showing ticker - skip back to QR
                Serial.println("Skip from ticker to QR (Single mode)");
                multiChannelConfig.btcTickerActive = false;
                String lnurlStr = generateLNURL(12);
                updateLightningQR(lnurlStr);
                if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
                  showSpecialModeQRScreen();
                } else {
                  showQRScreen();
                }
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
                String lnurlStr = generateLNURL(12);
                updateLightningQR(lnurlStr);
                if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
                  showSpecialModeQRScreen();
                } else {
                  showQRScreen();
                }
                productSelectionState.showTime = millis(); // Start timeout to return to ticker
              }
              // If on QR, touch does nothing (automatic return to ticker via timeout)
            }
            // Multi-Channel-Control Mode: Navigate to next product
            else if (multiChannelConfig.mode != "off" && lightningConfig.thresholdKey.length() == 0) {
              multiChannelConfig.btcTickerActive = false; // Exit ticker on navigation
              Serial.println("Navigate to next product");
              navigateToNextProduct();
            } else {
              // Normal/Special/Threshold Mode: Return to QR screen
              Serial.println("Returning to QR screen");
              redrawQRScreen();
            }
            // Reset timer
            productSelectionState.showTime = millis();
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
          // Also accept quick touch without gesture (GESTURE_NONE)
          // React on touch RELEASE (falling edge) for cleaner detection without flicker
          else if (gesture == GESTURE_NONE && !isTouched && wasTouched) {
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
        
        // Remember touch state for next iteration
        wasTouched = isTouched;
        
        // Any touch resets activity timer (for powerConfig.screensaver)
        activityTracking.lastActivityTime = millis();
        
        skip_product_touch_processing:
        ; // Empty statement required after label
      }
    }
    
    // Check if it's time to show/hide Bitcoin ticker screen
    // Behavior depends on multiChannelConfig.btcTickerMode
    if (!deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && lightningConfig.thresholdKey.length() == 0) {
      if (multiChannelConfig.btcTickerMode == "always") {
        if (multiChannelConfig.mode != "off") {
          // ALWAYS mode Duo/Quattro: Show ticker after PRODUCT_SELECTION_DELAY on products
          if (!multiChannelConfig.btcTickerActive && productSelectionState.showTime > 0 && 
              (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
            Serial.println("[SCREEN] Showing Bitcoin ticker screen after timeout (ALWAYS mode - Duo/Quattro)");
            btctickerScreen();
            multiChannelConfig.btcTickerActive = true;
          }
        } else {
          // ALWAYS mode Single: Return to ticker after PRODUCT_TIMEOUT on QR
          if (!multiChannelConfig.btcTickerActive && productSelectionState.showTime > 0 && 
              (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
            Serial.println("[SCREEN] Returning to ticker after timeout (ALWAYS mode - Single)");
            btctickerScreen();
            multiChannelConfig.btcTickerActive = true;
            productSelectionState.showTime = 0; // Reset timer
          }
        }
      } else if (multiChannelConfig.btcTickerMode == "off" && multiChannelConfig.mode != "off") {
        // OFF mode with Duo/Quattro: Return to product selection after timeout on product
        if (productSelectionState.showTime > 0 && 
            (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
          // Check if we're on a product screen
          if (multiChannelConfig.currentProduct > 0) {
            Serial.println("[SCREEN] Timeout reached - returning to product selection screen (OFF mode - Duo/Quattro)");
            multiChannelConfig.currentProduct = -1;
            deviceState.transition(DeviceState::PRODUCT_SELECTION);
            productSelectionScreen();
            productSelectionState.showTime = 0; // Reset timer
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
            String lnurlStr = generateLNURL(12);
            updateLightningQR(lnurlStr);
            if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
              showSpecialModeQRScreen();
            } else {
              showQRScreen();
            }
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
                // Fallback: show product selection
                multiChannelConfig.currentProduct = -1;
                deviceState.transition(DeviceState::PRODUCT_SELECTION);
                productSelectionScreen();
              }
              productSelectionState.showTime = 0; // Reset timer
            } else if (multiChannelConfig.currentProduct > 0 && !deviceState.isInState(DeviceState::PRODUCT_SELECTION) && 
                      (millis() - productSelectionState.showTime) >= PRODUCT_SELECTION_DELAY) {
              // Product showing: Return to product selection after PRODUCT_SELECTION_DELAY
              Serial.println("[SCREEN] Timeout reached - returning to product selection screen (SELECTING mode - Duo/Quattro)");
              multiChannelConfig.currentProduct = -1;
              deviceState.transition(DeviceState::PRODUCT_SELECTION);
              productSelectionScreen();
              productSelectionState.showTime = 0; // Reset timer
            }
          }
        }
      }
    }
    
    // Check for powerConfig.screensaver/deep sleep timeout activation (inside payment loop)
    if (!deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.screensaver != "off" && powerConfig.deepSleep == "off") {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - activityTracking.lastActivityTime;
      
      // Debug output every 10 seconds
      static unsigned long lastDebugOutput = 0;
      if (currentTime - lastDebugOutput > 10000) {
        Serial.printf("[SCREENSAVER_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                      elapsedTime, powerConfig.activationTimeoutMs, (elapsedTime * 100.0 / powerConfig.activationTimeoutMs));
        lastDebugOutput = currentTime;
      }
      
      if (elapsedTime >= powerConfig.activationTimeoutMs) {
        Serial.println("[TIMEOUT] Screensaver timeout reached, activating powerConfig.screensaver");
        deviceState.transition(DeviceState::SCREENSAVER);
        activateScreensaver(powerConfig.screensaver);
        // Continue with payment loop - powerConfig.screensaver only turns off backlight
      }
    }
    
    if (!deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.deepSleep != "off" && powerConfig.screensaver == "off") {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - activityTracking.lastActivityTime;
      
      // Debug output every 10 seconds
      static unsigned long lastDebugOutputDeep = 0;
      if (currentTime - lastDebugOutputDeep > 10000) {
        Serial.printf("[DEEP_SLEEP_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                      elapsedTime, powerConfig.activationTimeoutMs, (elapsedTime * 100.0 / powerConfig.activationTimeoutMs));
        lastDebugOutputDeep = currentTime;
      }
      
      if (elapsedTime >= powerConfig.activationTimeoutMs) {
        Serial.println("[TIMEOUT] Deep sleep timeout reached, preparing for deep sleep");
        deviceState.transition(DeviceState::DEEP_SLEEP);
        
        // Flush serial output before sleep
        Serial.flush();
        
        // Prepare display for sleep
        prepareDeepSleep();
        
        // Give more time for display operations to complete
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Final serial flush
        Serial.println("[DEEP_SLEEP] Entering sleep mode now...");
        Serial.flush();
        
        // Enter deep sleep (will not return in freeze mode)
        setupDeepSleepWakeup(powerConfig.deepSleep);
        
        // Execution continues here after wake-up from light sleep
        // (Deep sleep/freeze mode will restart the device instead)
        
        // CRITICAL: Light sleep disconnects USB-Serial hardware
        // We need to reinitialize USB-CDC to make Serial work again
        Serial.println("[WAKE_UP] Device woke from light sleep");
        
        // Reinitialize USB-CDC peripheral after light sleep
        Serial.end();
        delay(100);
        Serial.setRxBufferSize(2048); // Same as in setup()
        Serial.begin(115200);
        delay(200); // Give USB-CDC time to enumerate
        
        Serial.println("[WAKE_UP] USB-Serial reinitialized after light sleep");
        Serial.flush();
        
        // Show boot-up screen first
        bootUpScreen();
        Serial.println("[WAKE_UP] Boot screen displayed");
        
        // Turn backlight back on
        pinMode(PIN_LCD_BL, OUTPUT);
        digitalWrite(PIN_LCD_BL, HIGH);
        Serial.println("[WAKE_UP] Backlight restored");
        
        // Check WiFi connection status
        Serial.println("[WAKE_UP] Checking WiFi connection...");
        int wifiCheckCount = 0;
        while (WiFi.status() != WL_CONNECTED && wifiCheckCount < 30) {
          delay(100);
          wifiCheckCount++;
          if (wifiCheckCount % 10 == 0) {
            Serial.printf("[WAKE_UP] Waiting for WiFi... (%d/30)\n", wifiCheckCount);
          }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("[WAKE_UP] WiFi connected");
        } else {
          Serial.println("[WAKE_UP] WiFi not connected after wake-up, will retry in loop");
          // WiFi reconnection will be handled by checkAndReconnectWiFi() in main loop
        }
        
        // Reset activity time and clear sleep flag
        activityTracking.lastActivityTime = millis();
        powerConfig.lastWakeUpTime = millis();
        deviceState.transition(DeviceState::READY);
        
        // Small delay before redrawing screen
        delay(500);
        
        // Redraw the appropriate QR screen
        redrawQRScreen();
        Serial.println("[WAKE_UP] Ready for payments");
      }
    }
    
    webSocket.loop();
    loopCount++;

    // Update Bitcoin ticker (checks interval internally, non-blocking)
    updateBitcoinTicker();
    
    // Update switch labels periodically (checks interval internally, non-blocking)
    updateSwitchLabels();
    
    // Log status every 200000 loops (roughly every 10-20 minutes)
    if (loopCount % 200000 == 0)
    {
      Serial.printf("[LOOP] Still waiting... WiFi: %d, WS Connected: %d, paymentStatus.paid: %d\n", 
                    WiFi.status() == WL_CONNECTED, webSocket.isConnected(), paymentStatus.paid);
    }
    
    // Check Internet connectivity every 30 seconds (independent of WebSocket)
    if (millis() - lastInternetCheck > 30000 && !deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      // CRITICAL: Check WiFi first! Don't show "No Internet" if WiFi is down
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[INTERNET] Skipping Internet check - WiFi is down");
        lastInternetCheck = millis();
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

              // Fetch BTC price using configured currency
              String currencyLower = currency;
              currencyLower.toLowerCase();
              String apiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + currencyLower;
              http.begin(apiUrl);
              http.setTimeout(5000);
              int httpCode = http.GET();
              if (httpCode == 200) {
                String payload = http.getString();
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);
                if (!error && doc["bitcoin"].is<JsonObject>()) {
                  float price = doc["bitcoin"][currencyLower];
                  bitcoinData.price = String((int)price);
                  Serial.println("[BTC] Recovery price updated: " + bitcoinData.price + " " + currency);
                }
              }
              http.end();
              
              delay(100);
              
              // Fetch block height
              http.begin("https://mempool.space/api/blocks/tip/height");
              http.setTimeout(5000);
              httpCode = http.GET();
              if (httpCode == 200) {
                bitcoinData.blockHigh = http.getString();
                bitcoinData.blockHigh.trim();
                Serial.println("[BTC] Recovery block height updated: " + bitcoinData.blockHigh);
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
              deviceState.transition(DeviceState::READY);
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
    
    // Send ping every 60 seconds to check if WebSocket connection is really alive
    // Only if WebSocket is connected!
    if (webSocket.isConnected() && millis() - networkStatus.lastPingTime > 60000 && !deviceState.isInState(DeviceState::CONFIG_MODE))
    {
      Serial.println("[PING] Sending WebSocket ping to verify connection...");
      webSocket.sendPing();
      networkStatus.lastPingTime = millis();
      networkStatus.waitingForPong = true;
    }
    
    // Check WiFi, Server and WebSocket every 5 seconds while waiting for payment
    // Note: Internet is checked separately every 30 seconds
    if (millis() - lastWiFiCheck > 5000)
    {
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
        if (onErrorScreen && currentErrorType < 4)
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
          
          // Wait for connection to establish (2 seconds)
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        if (webSocket.isConnected())
        {
          Serial.println("WebSocket reconnected successfully");
          networkStatus.confirmed.websocket = true; // Set confirmation
          onErrorScreen = false;
          currentErrorType = 0;
          
          // Fetch switch labels after successful reconnection
          Serial.println("[RECOVERY] Fetching switch labels after WebSocket reconnection...");
          fetchSwitchLabels();
          
          return;
        }
        else
        {
          // WebSocket reconnect failed after 3 attempts
          Serial.printf("WebSocket reconnect failed after %d attempts\n", reconnectAttempts);
          if (networkStatus.errors.websocket < 99) networkStatus.errors.websocket++;
          Serial.printf("[ERROR] WebSocket error count: %d\n", networkStatus.errors.websocket);
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
      if (wifiOk && serverOk && websocketOk)
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
        return;
      }
      
      lastWiFiCheck = millis();
    }
    if (paymentStatus.paid)
    {
      Serial.println("[PAYMENT] Payment detected!");
      Serial.printf("[PAYMENT] PayloadStr: %s\n", payloadStr.c_str());
      
      if (lightningConfig.thresholdKey.length() > 0) {
        // === THRESHOLD MODE ===
        Serial.println("[THRESHOLD] Processing payment in threshold mode...");
        
        // Parse JSON payload to get payment amount
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (error) {
          Serial.print("[THRESHOLD] JSON parse error: ");
          Serial.println(error.c_str());
          paymentStatus.paid = false;
          return;
        }
        
        // Extract payment amount from JSON
        JsonObject payment = doc["payment"];
        int payment_amount = payment["amount"]; // in mSats
        int payment_sats = payment_amount / 1000; // Convert to sats
        int threshold_sats = lightningConfig.thresholdAmount.toInt();
        
        Serial.printf("[THRESHOLD] Payment received: %d sats (%d mSats)\n", payment_sats, payment_amount);
        Serial.printf("[THRESHOLD] Threshold: %d sats\n", threshold_sats);
        
        // Check if payment meets or exceeds threshold
        if (payment_sats >= threshold_sats) {
          Serial.println("[THRESHOLD] *** PAYMENT >= THRESHOLD! Triggering GPIO! ***");
          Serial.printf("[THRESHOLD] Switching GPIO %d for %d ms\n", 
                        lightningConfig.thresholdPin.toInt(), lightningConfig.thresholdTime.toInt());
          
          // Trigger GPIO pin
          int pin = lightningConfig.thresholdPin.toInt();
          int duration = lightningConfig.thresholdTime.toInt();
          
          // Check if special mode is enabled
          if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
            Serial.println("[THRESHOLD] Using special mode: " + specialModeConfig.mode);
            // Wake from powerConfig.screensaver if active
            if (deviceState.isInState(DeviceState::SCREENSAVER)) {
              deviceState.transition(DeviceState::READY);
              deactivateScreensaver();
            }
            activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment start
            
            actionTimeScreen();
            executeSpecialMode(pin, duration, specialModeConfig.frequency, specialModeConfig.dutyCycleRatio);
          } else {
            Serial.println("[THRESHOLD] Using standard mode");
            // Wake from powerConfig.screensaver if active
            if (deviceState.isInState(DeviceState::SCREENSAVER)) {
              deviceState.transition(DeviceState::READY);
              deactivateScreensaver();
            }
            activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment start
            actionTimeScreen();
            pinMode(pin, OUTPUT);
            digitalWrite(pin, HIGH);
            Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
            
            delay(duration);
            
            digitalWrite(pin, LOW);
            Serial.printf("[RELAY] Pin %d set LOW\n", pin);
          }
          
          thankYouScreen();
          activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment
          delay(2000);
          
          // Return to threshold QR screen
          showThresholdQRScreen();
          Serial.println("[THRESHOLD] Ready for next payment");
          // Reset product selection timer
          productSelectionState.showTime = millis();
          deviceState.transition(DeviceState::READY);
        } else {
          Serial.printf("[THRESHOLD] Payment too small (%d < %d sats) - ignoring\n", 
                        payment_sats, threshold_sats);
        }
        
        paymentStatus.paid = false;
        
      } else {
        // === NORMAL MODE ===
        Serial.println("[NORMAL] Processing payment in normal mode...");
        
        int pin = getValue(payloadStr, '-', 0).toInt();
        int duration = getValue(payloadStr, '-', 1).toInt();
        
        Serial.printf("[RELAY] Pin: %d, Duration: %d ms\n", pin, duration);
        
        // Check if special mode is enabled
        if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
          Serial.println("[NORMAL] Using special mode: " + specialModeConfig.mode);
          activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment start
          if (deviceState.isInState(DeviceState::SCREENSAVER)) {
            deactivateScreensaver();
            deviceState.transition(DeviceState::READY);
          }
          actionTimeScreen();
          executeSpecialMode(pin, duration, specialModeConfig.frequency, specialModeConfig.dutyCycleRatio);
        } else {
          Serial.println("[NORMAL] Using standard mode");
          activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment start
          if (deviceState.isInState(DeviceState::SCREENSAVER)) {
            deactivateScreensaver();
            deviceState.transition(DeviceState::READY);
          }
          actionTimeScreen();
          pinMode(pin, OUTPUT);
          digitalWrite(pin, HIGH);
          Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
          
          // If Single mode (multiChannelConfig.mode == "off") and pin is 12, also control pin 13 in parallel
          if (multiChannelConfig.mode == "off" && pin == 12) {
            pinMode(13, OUTPUT);
            digitalWrite(13, HIGH);
            Serial.println("[RELAY] Pin 13 set HIGH (parallel to Pin 12 in Single mode)");
          }
          
          delay(duration);
          
          digitalWrite(pin, LOW);
          Serial.printf("[RELAY] Pin %d set LOW\n", pin);
          
          // Turn off pin 13 as well if it was activated
          if (multiChannelConfig.mode == "off" && pin == 12) {
            digitalWrite(13, LOW);
            Serial.println("[RELAY] Pin 13 set LOW (parallel to Pin 12 in Single mode)");
          }
        }
        
        thankYouScreen();
        activityTracking.lastActivityTime = millis(); // Reset powerConfig.screensaver timer on payment
        if (deviceState.isInState(DeviceState::SCREENSAVER)) {
          deactivateScreensaver();
          deviceState.transition(DeviceState::READY);
        }
        delay(2000);
        
        // Return to appropriate QR screen
        redrawQRScreen();
        Serial.println("[NORMAL] Ready for next payment");
        // Reset product selection timer
        productSelectionState.showTime = millis();
        
        paymentStatus.paid = false;
      }
    }
  }
  Serial.println("[LOOP] Exiting payment wait loop");
}
