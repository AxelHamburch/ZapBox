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

#define FORMAT_ON_FAIL true
#define PARAM_FILE "/config.json"

TaskHandle_t Task1;

String ssid = "";
String wifiPassword = "";
String switchStr = "";
const char *lightningPrefix = "lightning:";
String qrFormat = "bech32"; // "bech32" or "lud17"
String orientation = "h";
String theme = "black-white";
char lightning[300] = "";
String thresholdKey = "";
String thresholdAmount = "";
String thresholdPin = "";
String thresholdTime = "";
String thresholdLnurl = "";

// Screensaver and deep sleep configuration
String screensaver = "off";
String deepSleep = "off";
String activationTime = "5";

// Special mode configuration
String specialMode = "standard";
float frequency = 1.0;
float dutyCycleRatio = 1.0;

// Multi-Channel-Control / Product selection configuration
String multiControl = "off";
// lnurl13, lnurl10, lnurl11 removed - now dynamically generated

// Switch labels from backend (cached after WebSocket connect)
String label12 = "";
String label13 = "";
String label10 = "";
String label11 = "";

// Screensaver and Deep Sleep timeout tracking
unsigned long lastActivityTime = 0;  // Track last button press or activity
unsigned long activationTimeoutMs = 5 * 60 * 1000; // Default 5 minutes in milliseconds
bool screensaverActive = false;
bool deepSleepActive = false;
unsigned long lastWakeUpTime = 0;  // Track when device woke up from screensaver
const unsigned long GRACE_PERIOD_MS = 5000;  // 5 seconds grace period after wake-up

String payloadStr;
String lnbitsServer;
String deviceId;
bool paid = false;
bool inConfigMode = false;
bool inReportMode = false;
bool inHelpMode = false;
bool setupComplete = false; // Prevent loop from running before setup finishes
bool firstLoop = true; // Track first loop iteration

// Error counters (0-99, capped at 99)
uint8_t wifiErrorCount = 0;
uint8_t internetErrorCount = 0;
uint8_t serverErrorCount = 0;
uint8_t websocketErrorCount = 0;

// Connection confirmation tracking (for first successful connection)
bool wifiConfirmed = false;
bool internetConfirmed = false;
bool serverConfirmed = false;
bool websocketConfirmed = false;

// Error screen tracking
bool onErrorScreen = false;
byte currentErrorType = 0; // 0=none, 1=WiFi (highest), 2=Internet, 3=Server, 4=WebSocket (lowest)
unsigned long lastPingTime = 0;
unsigned long lastInternetCheck = 0; // Track when we last checked Internet connectivity
byte consecutiveWebSocketFailures = 0; // Track consecutive WebSocket failures to detect Internet issues

// Buttons
OneButton leftButton(PIN_BUTTON_1, true);
OneButton rightButton(PIN_BUTTON_2, true);

// Touch controller
TouchCST816S touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, PIN_TOUCH_RES, PIN_TOUCH_INT);
bool touchAvailable = false;

// Touch button state tracking (for click/double/triple detection)
unsigned long lastTouchTime = 0;
uint8_t touchClickCount = 0;
const unsigned long TOUCH_DOUBLE_CLICK_MS = 1000; // 1 second window for second click
const unsigned long TOUCH_LONG_PRESS_MS = 3000;  // 3 seconds for long press
bool touchPressed = false;
unsigned long touchPressStartTime = 0;

// Product selection screen tracking
bool onProductSelectionScreen = false;
unsigned long productSelectionShowTime = 0;
const unsigned long PRODUCT_SELECTION_DELAY = 5000; // 5 seconds (TEST: default 10000ms)

// Multi-Channel-Control product navigation
// 0 = "Select the product" screen
// 1 = Product 1 (Pin 12)
// 2 = Product 2 (Pin 13)
// 3 = Product 3 (Pin 10)
// 4 = Product 4 (Pin 11)
int currentProduct = 0;
int maxProducts = 1; // Will be set based on multiControl mode

WebSocketsClient webSocket;

//////////////////FORWARD DECLARATIONS///////////////////

void reportMode();
void configMode();
void showHelp();
void fetchSwitchLabels();
void updateLightningQR(String lnurlStr);
void navigateToNextProduct();
String generateLNURL(int pin);
String encodeBech32(const String& data);

//////////////////HELPERS///////////////////

// Bech32 encoding helper
const char* BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

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
    strcpy(lightning, lnurlStr.c_str());
  } else {
    // No prefix, add it (lowercase for consistency)
    strcpy(lightning, "lightning:");
    strcat(lightning, lnurlStr.c_str());
  }
  
  Serial.print("[QR] Updated lightning QR: ");
  Serial.println(lightning);
}

// Helper function: Navigate to next product in Multi-Channel-Control mode
void navigateToNextProduct() {
  if (multiControl == "off") return; // Single mode, no navigation
  
  currentProduct++;
  
  // Loop through products (1-2 for duo, 1-4 for quattro)
  if (multiControl == "duo" && currentProduct > 2) {
    currentProduct = 1; // Loop back to first product
  } else if (multiControl == "quattro" && currentProduct > 4) {
    currentProduct = 1; // Loop back to first product
  }
  
  Serial.printf("[NAV] Navigate to product: %d\n", currentProduct);
  
  // Always show product QR screen (no selection screen)
  onProductSelectionScreen = false;
  if (true) {
    // Show product QR screen
    String label = "";
    int pin = 0;
    
    switch(currentProduct) {
      case 1: // Pin 12
        label = (label12.length() > 0) ? label12 : "Pin 12";
        pin = 12;
        break;
      case 2: // Pin 13
        label = (label13.length() > 0) ? label13 : "Pin 13";
        pin = 13;
        break;
      case 3: // Pin 10
        label = (label10.length() > 0) ? label10 : "Pin 10";
        pin = 10;
        break;
      case 4: // Pin 11
        label = (label11.length() > 0) ? label11 : "Pin 11";
        pin = 11;
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
  }
  
  // Reset product selection timer after every navigation
  productSelectionShowTime = millis();
  Serial.println("[NAV] Product selection timer reset");
}

// Helper function: Wake from power saving modes
bool wakeFromPowerSavingMode() {
  // Check if we're in grace period after wake-up
  if (lastWakeUpTime > 0 && (millis() - lastWakeUpTime) < GRACE_PERIOD_MS) {
    Serial.println("[WAKE] Ignored - in grace period after wake-up");
    return true; // Indicate we're in grace period
  }
  
  // Reset activity timer
  lastActivityTime = millis();
  
  // If screensaver or deep sleep was active, deactivate and return true
  if (screensaverActive || deepSleepActive) {
    Serial.println("[WAKE] Waking from power saving mode");
    screensaverActive = false;
    deepSleepActive = false;
    deactivateScreensaver();
    lastWakeUpTime = millis();
    lastActivityTime = millis();
    return true; // Indicate we just woke up
  }
  
  return false; // Normal operation, no wake-up needed
}

// Helper function: Redraw appropriate QR screen based on mode
void redrawQRScreen() {
  Serial.println("[DISPLAY] Redrawing QR screen...");
  if (thresholdKey.length() > 0) {
    showThresholdQRScreen();
    Serial.println("[DISPLAY] Threshold QR screen displayed");
  } else if (multiControl != "off") {
    // Multi-Channel-Control mode: Show current product or selection screen
    if (currentProduct == 0) {
      productSelectionScreen();
      onProductSelectionScreen = true;
      Serial.println("[DISPLAY] Product selection screen displayed");
    } else {
      String label = "";
      int displayPin = 0;
      
      switch(currentProduct) {
        case 1:
          label = (label12.length() > 0) ? label12 : "Pin 12";
          displayPin = 12;
          break;
        case 2:
          label = (label13.length() > 0) ? label13 : "Pin 13";
          displayPin = 13;
          break;
        case 3:
          label = (label10.length() > 0) ? label10 : "Pin 10";
          displayPin = 10;
          break;
        case 4:
          label = (label11.length() > 0) ? label11 : "Pin 11";
          displayPin = 11;
          break;
      }
      
      // Generate LNURL dynamically for current product's pin
      String lnurlStr = generateLNURL(displayPin);
      updateLightningQR(lnurlStr);
      showProductQRScreen(label, displayPin);
      onProductSelectionScreen = false;
      Serial.printf("[DISPLAY] Product %d QR screen displayed\n", currentProduct);
    }
  } else if (specialMode != "standard" && specialMode != "") {
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
  
  // Execute cycles until duration is reached
  while (elapsed < duration_ms) {
    // Check for config mode interrupt
    if (inConfigMode) {
      Serial.println("[SPECIAL] Interrupted by config mode");
      digitalWrite(pin, LOW);
      break;
    }
    
    cycleCount++;
    
    // PIN HIGH
    digitalWrite(pin, HIGH);
    Serial.printf("[SPECIAL] Cycle %d: Pin HIGH\n", cycleCount);
    delay(onTime_ms);
    
    // PIN LOW
    digitalWrite(pin, LOW);
    Serial.printf("[SPECIAL] Cycle %d: Pin LOW\n", cycleCount);
    delay(offTime_ms);
    
    elapsed = millis() - startTime;
  }
  
  // Ensure pin is LOW at the end
  digitalWrite(pin, LOW);
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
    ssid = maRoot0Char;
    Serial.println("SSID: " + ssid);

    const JsonObject maRoot1 = doc[1];
    const char *maRoot1Char = maRoot1["value"];
    wifiPassword = maRoot1Char;
    Serial.println("Wifi pass: " + wifiPassword);

    const JsonObject maRoot2 = doc[2];
    const char *maRoot2Char = maRoot2["value"];
    switchStr = maRoot2Char;
    
    // Parse WebSocket URL (works with both ws:// and wss://)
    int protocolIndex = switchStr.indexOf("://");
    Serial.printf("DEBUG: switchStr='%s', protocolIndex=%d\n", switchStr.c_str(), protocolIndex);
    
    if (protocolIndex == -1) {
      Serial.println("Invalid switchStr: " + switchStr);
      lnbitsServer = "";
      deviceId = "";
    } else {
      int domainIndex = switchStr.indexOf("/", protocolIndex + 3);
      Serial.printf("DEBUG: domainIndex=%d\n", domainIndex);
      
      if (domainIndex == -1) {
        Serial.println("Invalid switchStr: " + switchStr);
        lnbitsServer = "";
        deviceId = "";
      } else {
        int uidLength = 22; // Length of device ID
        lnbitsServer = switchStr.substring(protocolIndex + 3, domainIndex);
        deviceId = switchStr.substring(switchStr.length() - uidLength);
        
        Serial.printf("DEBUG: Extracted server from index %d to %d\n", protocolIndex + 3, domainIndex);
      }
    }

    Serial.println("Socket: " + switchStr);
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

    const JsonObject maRoot4 = doc[4];
    const char *maRoot4Char = maRoot4["value"];
    orientation = maRoot4Char;
    Serial.println("Screen orientation: " + orientation);

    const JsonObject maRoot5 = doc[5];
    if (!maRoot5.isNull()) {
      const char *maRoot5Char = maRoot5["value"];
      theme = maRoot5Char;
    }
    Serial.println("Theme: " + theme);

    // Read threshold configuration (optional)
    const JsonObject maRoot6 = doc[6];
    if (!maRoot6.isNull()) {
      const char *maRoot6Char = maRoot6["value"];
      thresholdKey = maRoot6Char;
    }

    const JsonObject maRoot7 = doc[7];
    if (!maRoot7.isNull()) {
      const char *maRoot7Char = maRoot7["value"];
      thresholdAmount = maRoot7Char;
    }

    const JsonObject maRoot8 = doc[8];
    if (!maRoot8.isNull()) {
      const char *maRoot8Char = maRoot8["value"];
      thresholdPin = maRoot8Char;
    }

    const JsonObject maRoot9 = doc[9];
    if (!maRoot9.isNull()) {
      const char *maRoot9Char = maRoot9["value"];
      thresholdTime = maRoot9Char;
    }

    const JsonObject maRoot10 = doc[10];
    if (!maRoot10.isNull()) {
      const char *maRoot10Char = maRoot10["value"];
      thresholdLnurl = maRoot10Char;
    }

    // Read special mode configuration (Index 11-13)
    const JsonObject maRoot11 = doc[11];
    if (!maRoot11.isNull()) {
      const char *maRoot11Char = maRoot11["value"];
      specialMode = maRoot11Char;
    }

    const JsonObject maRoot12 = doc[12];
    if (!maRoot12.isNull()) {
      const char *maRoot12Char = maRoot12["value"];
      frequency = String(maRoot12Char).toFloat();
      if (frequency < 0.1) frequency = 0.1;  // Min 0.1 Hz
      if (frequency > 10.0) frequency = 10.0; // Max 10 Hz
    }

    const JsonObject maRoot13 = doc[13];
    if (!maRoot13.isNull()) {
      const char *maRoot13Char = maRoot13["value"];
      dutyCycleRatio = String(maRoot13Char).toFloat();
      if (dutyCycleRatio < 0.1) dutyCycleRatio = 0.1;   // Min 1:10
      if (dutyCycleRatio > 10.0) dutyCycleRatio = 10.0; // Max 10:1
    }

    // Read screensaver and deep sleep configuration (optional, indices 14-16)
    const JsonObject maRoot14 = doc[14];
    if (!maRoot14.isNull()) {
      const char *maRoot14Char = maRoot14["value"];
      screensaver = maRoot14Char;
    }

    const JsonObject maRoot15 = doc[15];
    if (!maRoot15.isNull()) {
      const char *maRoot15Char = maRoot15["value"];
      deepSleep = maRoot15Char;
    }

    const JsonObject maRoot16 = doc[16];
    if (!maRoot16.isNull()) {
      const char *maRoot16Char = maRoot16["value"];
      activationTime = maRoot16Char;
      // Validate activation time (1-120 minutes)
      int actTime = String(activationTime).toInt();
      if (actTime < 1) activationTime = "1";
      if (actTime > 120) activationTime = "120";
    }

    // Read multi-channel-control configuration (index 17)
    const JsonObject maRoot17 = doc[17];
    if (!maRoot17.isNull()) {
      const char *maRoot17Char = maRoot17["value"];
      multiControl = maRoot17Char;
    }
    // Indices 18-20 removed (lnurl13, lnurl10, lnurl11 - now auto-generated)

    // Apply predefined mode settings
    if (specialMode == "blink") {
      frequency = 1.0;
      dutyCycleRatio = 1.0;
      Serial.println("[CONFIG] Applied 'blink' preset: 1 Hz, 1:1");
    } else if (specialMode == "pulse") {
      frequency = 2.0;
      dutyCycleRatio = 0.25; // 1:4 = 0.25
      Serial.println("[CONFIG] Applied 'pulse' preset: 2 Hz, 1:4");
    } else if (specialMode == "fast-blink") {
      frequency = 5.0;
      dutyCycleRatio = 1.0;
      Serial.println("[CONFIG] Applied 'fast-blink' preset: 5 Hz, 1:1");
    }
    
    Serial.println("Special Mode: " + specialMode);
    Serial.print("Frequency: ");
    Serial.print(frequency);
    Serial.println(" Hz");
    Serial.print("Duty Cycle Ratio: ");
    Serial.println(dutyCycleRatio);

    // Display Multi-Channel-Control configuration
    Serial.print("Multi-Channel-Control Mode: ");
    if (multiControl == "off") {
      Serial.println("Single (Pin 12 only)");
    } else if (multiControl == "duo") {
      Serial.println("Duo (Pins 12, 13) - LNURLs auto-generated");
    } else if (multiControl == "quattro") {
      Serial.println("Quattro (Pins 12, 13, 10, 11) - LNURLs auto-generated");
    }

    // Display mode based on threshold configuration
    Serial.println("\n================================");
    if (thresholdKey.length() > 0) {
      Serial.println("       THRESHOLD MODE");
      Serial.println("================================");
      Serial.println("Threshold Key: " + thresholdKey);
      Serial.println("Threshold Amount: " + thresholdAmount + " sats");
      Serial.println("GPIO Pin: " + thresholdPin);
      Serial.println("Control Time: " + thresholdTime + " ms");
      
      // Process threshold LNURL (add lightning: prefix if not present)
      if (thresholdLnurl.length() > 0) {
        thresholdLnurl.trim(); // Remove leading/trailing whitespace
        
        if (thresholdLnurl.startsWith("lightning:") || thresholdLnurl.startsWith("LIGHTNING:")) {
          // Already has prefix, convert to lowercase lightning:
          if (thresholdLnurl.startsWith("LIGHTNING:")) {
            strcpy(lightning, "lightning:");
            strcat(lightning, thresholdLnurl.c_str() + 10);
          } else {
            strcpy(lightning, thresholdLnurl.c_str());
          }
        } else {
          // No prefix, add it (lowercase)
          strcpy(lightning, "lightning:");
          strcat(lightning, thresholdLnurl.c_str());
        }
        Serial.print("Threshold LNURL: ");
        Serial.println(thresholdLnurl);
        Serial.print("Threshold QR: ");
        Serial.println(lightning);
      }
    } else {
      Serial.println("        NORMAL MODE");
      Serial.println("================================");
    }

    // Display screensaver and deep sleep configuration
    Serial.println("\n================================");
    Serial.println("   POWER SAVING CONFIGURATION");
    Serial.println("================================");
    Serial.println("Screensaver: " + screensaver);
    Serial.println("Deep Sleep: " + deepSleep);
    Serial.println("Activation Time: " + activationTime + " minutes");
    
    // Convert activation time from minutes to milliseconds
    int activationTimeMinutes = String(activationTime).toInt();
    activationTimeoutMs = activationTimeMinutes * 60 * 1000UL;
    Serial.println("Activation Timeout: " + String(activationTimeoutMs) + " ms");
    
    // Determine and display active power saving mode
    if (screensaver != "off" && deepSleep == "off") {
      Serial.println("\n⚡ POWER SAVING MODE: SCREENSAVER");
      Serial.println("   Display backlight will turn off after " + activationTime + " minutes");
      Serial.println("   Press BOOT or IO14 button to wake up");
    } else if (deepSleep != "off" && screensaver == "off") {
      Serial.println("\n⚡ POWER SAVING MODE: DEEP SLEEP (" + deepSleep + ")");
      Serial.println("   Device will enter " + deepSleep + " sleep after " + activationTime + " minutes");
      Serial.println("   Press BOOT or IO14 button to wake up");
      Serial.println("Deep Sleep enabled - configuring GPIO wake-up sources");
      // Wake-up sources will be configured in setupDeepSleepWakeup() when sleep is triggered
    } else {
      Serial.println("\n⚡ POWER SAVING MODE: DISABLED");
      Serial.println("   Device will stay active continuously");
    }
    
    // Initialize last activity time
    lastActivityTime = millis();
    Serial.println("Last Activity Time initialized: " + String(lastActivityTime) + " ms");
    
    Serial.println("================================\n");
  }
  else
  {
    Serial.println("Config file not found - using defaults");
    orientation = "h";
    strcpy(lightning, "LIGHTNING:lnurl1dp68gurn8ghj7ctsdyhxkmmvwp5jucm0d9hkuegpr4r33");
    Serial.println("\n================================");
    Serial.println("        NORMAL MODE");
    Serial.println("================================\n");
  }
  paramFile.close();
}

//////////////////WEBSOCKET///////////////////

unsigned long lastPongTime = 0;
bool waitingForPong = false;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  Serial.printf("[WS Event] Type: %d, ConfigMode: %d\n", type, inConfigMode);
  
  if (inConfigMode == false)
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
      lastPongTime = millis(); // Reset pong timer on connect
      waitingForPong = false;
      websocketConfirmed = true; // Mark WebSocket as confirmed on first connect
      Serial.println("[CONFIRMED] WebSocket connection confirmed!");
      
      // Fetch switch labels from backend after successful connection
      fetchSwitchLabels();
    }
    break;
    case WStype_TEXT:
      Serial.printf("[WS] Received text: %s\n", payload);
      payloadStr = (char *)payload;
      Serial.printf("[WS] PayloadStr set to: %s\n", payloadStr.c_str());
      paid = true;
      Serial.println("[WS] 'paid' flag set to TRUE");
      break;
    case WStype_PING:
      Serial.println("[WS] Received Ping");
      break;
    case WStype_PONG:
      Serial.println("[WS] Received Pong - connection alive!");
      lastPongTime = millis();
      waitingForPong = false;
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
      // Clear existing labels
      label12 = "";
      label13 = "";
      label10 = "";
      label11 = "";
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        
        // Store label based on pin number
        if (pin == 12) {
          label12 = labelStr;
          Serial.println("[LABELS] Pin 12 label: " + label12);
        } else if (pin == 13) {
          label13 = labelStr;
          Serial.println("[LABELS] Pin 13 label: " + label13);
        } else if (pin == 10) {
          label10 = labelStr;
          Serial.println("[LABELS] Pin 10 label: " + label10);
        } else if (pin == 11) {
          label11 = labelStr;
          Serial.println("[LABELS] Pin 11 label: " + label11);
        }
      }
      
      Serial.println("[LABELS] Successfully fetched and cached all labels");
    } else {
      Serial.println("[LABELS] JSON parsing failed: " + String(error.c_str()));
    }
  } else {
    Serial.printf("[LABELS] HTTP request failed with code: %d\n", httpCode);
  }
  
  http.end();
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

void checkAndReconnectWiFi()
{
  if (WiFi.status() != WL_CONNECTED && !inConfigMode)
  {
    Serial.println("WiFi connection lost! Reconnecting...");
    if (wifiErrorCount < 99) wifiErrorCount++;
    Serial.printf("[ERROR] WiFi error count: %d\n", wifiErrorCount);
    onErrorScreen = true;
    currentErrorType = 1; // WiFi error (highest priority)
    wifiReconnectScreen();
    
    // Keep trying to reconnect forever (unless config mode is triggered)
    while (WiFi.status() != WL_CONNECTED && !inConfigMode)
    {
      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(50)); // Brief delay for button task
      
      WiFi.mode(WIFI_STA); // Ensure Station mode
      WiFi.setSleep(false); // Disable WiFi power saving
      WiFi.setAutoReconnect(true); // Enable auto-reconnect
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.begin(ssid.c_str(), wifiPassword.c_str());
      
      unsigned long reconnectStartTime = millis();
      unsigned long lastDotTime = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - reconnectStartTime) < 10000 && !inConfigMode)
      {
        // If Help or Report button pressed, wait for them to finish
        if (inHelpMode || inReportMode) {
          Serial.println("\nButton pressed, pausing WiFi reconnect...");
          while (inHelpMode || inReportMode) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for button action to complete
          }
          Serial.println("Button action complete, resuming WiFi reconnect");
          // Reset timer after button action
          reconnectStartTime = millis();
          lastDotTime = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Allow button task to run
        
        // Print dot every 500ms
        if (millis() - lastDotTime >= 500) {
          Serial.print(".");
          lastDotTime = millis();
        }
      }
      
      // Check if config mode was triggered during wait
      if (inConfigMode) {
        Serial.println("\nConfig mode triggered during WiFi reconnect");
        break;
      }
      
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nWiFi reconnected!");
        wifiConfirmed = true; // Confirm WiFi
        
        // IMPORTANT: Check if Internet is still available
        Serial.println("[RECOVERY] Checking Internet after WiFi reconnect...");
        bool hasInternet = checkInternetConnectivity();
        
        if (!hasInternet) {
          // Internet still down - show Internet error
          Serial.println("[RECOVERY] Internet still down!");
          internetReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 2; // Internet error
          internetConfirmed = false; // Clear Internet confirmation
          break; // Exit WiFi reconnect loop but keep error screen
        }
        
        // Internet OK - check Server
        Serial.println("[RECOVERY] Internet OK, checking Server...");
        internetConfirmed = true; // Confirm Internet
        bool serverReachable = checkServerReachability();
        
        if (!serverReachable) {
          // Server still down - show Server error
          Serial.println("[RECOVERY] Server still down!");
          serverReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 3; // Server error
          serverConfirmed = false; // Clear Server confirmation
          // Try to connect WebSocket anyway (will fail but that's expected)
        } else {
          // Server OK - reconnect WebSocket
          Serial.println("[RECOVERY] Server OK, reconnecting WebSocket...");
          serverConfirmed = true; // Confirm Server
          // Don't clear error yet - wait for WebSocket confirmation
          websocketReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 4; // WebSocket error (will be cleared on connect)
        }
        
        // Reconnect WebSocket after WiFi is back
        webSocket.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (thresholdKey.length() > 0) {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
        } else {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
        }
        
        // Wait up to 5 seconds for WebSocket to connect
        Serial.println("[RECOVERY] Waiting for WebSocket connection...");
        int wsWaitCount = 0;
        while (!webSocket.isConnected() && wsWaitCount < 50) { // 50 * 100ms = 5 seconds
          webSocket.loop(); // Process WebSocket events
          vTaskDelay(pdMS_TO_TICKS(100));
          wsWaitCount++;
          if (wsWaitCount % 10 == 0) {
            Serial.printf("[RECOVERY] Waiting... (%d/50)\n", wsWaitCount);
          }
        }
        
        if (webSocket.isConnected()) {
          Serial.println("[RECOVERY] WebSocket reconnected successfully!");
          websocketConfirmed = true; // Confirm WebSocket connection
          onErrorScreen = false; // Clear error screen
          currentErrorType = 0; // No error
          
          // Redraw QR screen immediately after successful recovery
          Serial.println("[RECOVERY] Redrawing QR screen after WiFi recovery");
          redrawQRScreen();
          
          // Reset product selection timer so it shows after 5 seconds
          productSelectionShowTime = millis();
          Serial.println("[RECOVERY] Product selection timer reset");
        } else {
          Serial.println("[RECOVERY] WebSocket connection timeout after 5 seconds");
          if (websocketErrorCount < 99) websocketErrorCount++;
          // Keep showing WebSocket error screen
        }
        
        break;
      }
      else
      {
        Serial.println("\nRetrying WiFi connection...");
        vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay before next attempt
      }
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
  inConfigMode = true; // Then set flag
  
  Serial.println("Config mode screen displayed, entering serial config...");
  Serial.flush();
  
  bool hasExistingData = (ssid.length() > 0);
  Serial.printf("Has existing data: %s\n", hasExistingData ? "YES" : "NO");
  Serial.flush();
  
  configOverSerialPort(ssid, wifiPassword, hasExistingData);
}

void reportMode()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    Serial.println("[REPORT] Device woke up, not entering report mode");
    return; // Don't trigger report mode, just wake up
  }
  
  // Ignore if we just entered config mode (prevents triggering on button release)
  if (inConfigMode) {
    Serial.println("[REPORT] Ignored - in config mode");
    return;
  }
  
  Serial.println("[BUTTON] Report mode button pressed");
  inReportMode = true; // Set flag to interrupt WiFi reconnect loop
  
  // Disable product selection timer during report mode
  productSelectionShowTime = 0;
  onProductSelectionScreen = false;
  
  errorReportScreen(wifiErrorCount, internetErrorCount, serverErrorCount, websocketErrorCount);
  vTaskDelay(pdMS_TO_TICKS(2000)); // First screen: 2 seconds
  wifiReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  internetReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  serverReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  websocketReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
  
  // Show appropriate screen based on current error state
  if (onErrorScreen) {
    // Check which error is active and show corresponding screen (priority order)
    if (WiFi.status() != WL_CONNECTED) {
      wifiReconnectScreen();
    } else if (!checkInternetConnectivity()) {
      internetReconnectScreen();
    } else if (waitingForPong && (millis() - lastPingTime > 10000)) {
      serverReconnectScreen();
    } else if (!webSocket.isConnected()) {
      websocketReconnectScreen();
    }
  } else {
    // No error active, show QR screen
    redrawQRScreen();
    // Reset product selection timer
    productSelectionShowTime = millis();
  }
  
  inReportMode = false; // Clear flag AFTER showing final screen
}

void handleTouchButton()
{
  // If in Help mode: Allow second click to switch to Report
  if (inHelpMode) {
    // Check for new button press
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchPressed) {
      touchPressed = true;
      touchPressStartTime = millis();
      
      // This is the second click - switch from Help to Report
      Serial.println("[TOUCH] Second click during Help -> Switching to Report Mode");
      inHelpMode = false; // Abort Help
      touchClickCount = 0; // Reset
      
      // Check for Config mode (display touch)
      if (touch.available()) {
        uint16_t mainTouchX = touch.getX();
        uint16_t mainTouchY = touch.getY();
        bool isMainAreaTouch = false;
        
        if (orientation == "v") {
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
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchPressed) {
      touchPressed = false;
    }
    return;
  }
  
  // If in Report mode: Button press aborts
  if (inReportMode) {
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchPressed) {
      Serial.println("[TOUCH] Button press during Report - ABORTING");
      inReportMode = false;
      touchPressed = true;
    }
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchPressed) {
      touchPressed = false;
    }
    touchClickCount = 0;
    return;
  }
  
  // FIRST: Check if click sequence timeout has expired (ALWAYS check, not just on touch events)
  if (touchClickCount > 0 && !touchPressed) {
    unsigned long timeSinceLastTouch = millis() - lastTouchTime;
    
    // For 1 click: Wait 1 second for potential second click
    // If no second click after 1s → Start Help
    if (touchClickCount == 1 && timeSinceLastTouch > 1000 && !inHelpMode) {
      Serial.println("[TOUCH] Timeout: 1 click, no second click -> Help");
      showHelp();
      touchClickCount = 0;
    }
    
    // For 2 clicks: Wait 1 second for potential third click
    // If no third click after 1s → Start Report
    else if (touchClickCount == 2 && timeSinceLastTouch > 1000 && !inReportMode) {
      Serial.println("[TOUCH] Timeout: 2 clicks, no third click -> Report");
      reportMode();
      touchClickCount = 0;
    }
    
    // For 3 clicks: Wait 1 second for potential fourth click
    // If no fourth click after 1s → Reset (do nothing)
    else if (touchClickCount == 3 && timeSinceLastTouch > 1000) {
      Serial.println("[TOUCH] Timeout: 3 clicks, no fourth click -> Reset");
      touchClickCount = 0;
    }
    
    // If Help is running and more than 3s passed since last click: Reset
    if (inHelpMode && timeSinceLastTouch > 3000) {
      touchClickCount = 0;
    }
  }
  
  // Check if touch interrupt is triggered (GPIO 16 LOW when touched)
  if (digitalRead(PIN_TOUCH_INT) == LOW && !touchPressed) {
    // Touch detected - read coordinates to check if it's the button area
    uint16_t touchX = touch.getX();
    uint16_t touchY = touch.getY();
    
    // Define touch button area based on HARDWARE position
    // Touch coordinates are hardware-based (0-170 x 0-320), don't rotate with display!
    // Physical button is ALWAYS at the same hardware location: high Y values (Y > 305)
    // - Vertical (rotation=0): Button at BOTTOM of display → Y > 305
    // - Horizontal (rotation=1): Button at RIGHT of display → STILL Y > 305 (not X!)
    bool inButtonArea = (touchY > 305);
    
    // Ignore touches outside button area
    if (!inButtonArea) {
      return;
    }
    
    Serial.printf("[TOUCH] Button area touched at X=%d, Y=%d (orientation=%s)\n", 
                  touchX, touchY, orientation.c_str());
    
    // Touch button pressed
    touchPressed = true;
    touchPressStartTime = millis();
    
    // Increment click count if within double-click window
    unsigned long timeSinceLastTouch = millis() - lastTouchTime;
    
    if (timeSinceLastTouch < TOUCH_DOUBLE_CLICK_MS && timeSinceLastTouch > 100) {
      // Within double-click window AND minimum 100ms since last touch (debounce)
      touchClickCount++;
      Serial.printf("[TOUCH] Click within window (%lu ms since last) - count now: %d\n", 
                    timeSinceLastTouch, touchClickCount);
    } else if (timeSinceLastTouch <= 100) {
      // Too fast - likely bounce or accidental double-click, ignore
      Serial.printf("[TOUCH] Too fast (%lu ms), ignoring (debounce)\n", timeSinceLastTouch);
      return;
    } else {
      touchClickCount = 1; // Reset to 1 for new click sequence
      Serial.printf("[TOUCH] New click sequence (last click was %lu ms ago)\n", timeSinceLastTouch);
    }
    lastTouchTime = millis();
    
    Serial.printf("[TOUCH] Button click count: %d\n", touchClickCount);
    
    // Process clicks:
    // - Click 1: Wait for timeout (1s) → Help
    // - Click 2: Wait for timeout (1s) → Report (unless click 3 comes)
    // - Click 3: Wait for timeout (1s) → Nothing (waiting for click 4)
    // - Click 4: Immediate Config Mode
    if (touchClickCount == 4) {
      // Fourth click within timeout -> Config Mode (IMMEDIATE, no waiting)
      Serial.println("[TOUCH] Fourth click -> Config Mode");
      inHelpMode = false;
      inReportMode = false;
      configMode();
      touchClickCount = 0;
    }
    // For clicks 1, 2, and 3: Do nothing, let timeout handler decide
  }
  else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchPressed) {
    // Touch released
    touchPressed = false;
    unsigned long pressDuration = millis() - touchPressStartTime;
    
    Serial.printf("[TOUCH] Button released after %lu ms\n", pressDuration);
  }
}

void showHelp()
{
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    Serial.println("[HELP] Device woke up, not entering help mode");
    return; // Don't trigger help mode, just wake up
  }
  
  Serial.println("[BUTTON] Help button pressed");
  inHelpMode = true; // Set flag to interrupt WiFi reconnect loop
  
  // Disable product selection timer during help mode
  productSelectionShowTime = 0;
  onProductSelectionScreen = false;
  
  stepOneScreen();
  
  // Check for button press during first screen
  unsigned long screenStart = millis();
  while (millis() - screenStart < 1000 && inHelpMode) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!inHelpMode) break; // Button pressed in handleTouchButton()
  }
  if (!inHelpMode) return; // Exit Help early
  
  stepTwoScreen();
  
  // Check for button press during second screen
  screenStart = millis();
  while (millis() - screenStart < 1000 && inHelpMode) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!inHelpMode) break;
  }
  if (!inHelpMode) return; // Exit Help early
  
  stepThreeScreen();
  
  // Check for button press during third screen
  screenStart = millis();
  while (millis() - screenStart < 1000 && inHelpMode) { // TEST: 1s (default: 3000ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!inHelpMode) break;
  }
  
  inHelpMode = false; // Clear flag
  
  // Return to error screen if one was active, otherwise show QR screen
  if (onErrorScreen) {
    // Check which error is active and show corresponding screen
    if (WiFi.status() != WL_CONNECTED) {
      wifiReconnectScreen();
    } else if (waitingForPong && (millis() - lastPingTime > 10000)) {
      internetReconnectScreen();
    } else if (!webSocket.isConnected()) {
      websocketReconnectScreen();
    }
  } else {
    // No error active, show QR screen
    redrawQRScreen();
    // Reset product selection timer
    productSelectionShowTime = millis();
    onProductSelectionScreen = false;
  }
}

void Task1code(void *pvParameters)
{
  for (;;)
  {
    leftButton.tick();
    rightButton.tick();
    
    // Handle touch button if available
    if (touchAvailable) {
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

  FFat.begin(FORMAT_ON_FAIL);
  readFiles(); // get the saved details and store in global variables

  initDisplay();
  startupScreen();

  // CRITICAL: Start button task BEFORE WiFi setup so config mode works during reconnect!
  leftButton.setPressMs(3000); // 3 seconds for config mode (documented as 5 sec for users)
  leftButton.setDebounceMs(100); // 100ms debounce to prevent accidental report mode
  leftButton.attachClick(reportMode);
  leftButton.attachLongPressStart(configMode);
  rightButton.setDebounceMs(200); // 200ms debounce
  rightButton.attachClick(showHelp);
  rightButton.attachLongPressStart(showHelp); // Also trigger help on long press (prevents missed clicks)

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
  WiFi.begin(ssid.c_str(), wifiPassword.c_str());
  Serial.println("[STARTUP] WiFi connection started in background (Power Save: OFF)");

  // Show startup screen for 5 seconds
  Serial.println("[STARTUP] Showing startup screen for 6 seconds...");
  for (int i = 0; i < 50; i++) { // 50 * 100ms = 5 seconds
    vTaskDelay(pdMS_TO_TICKS(100));
    if (inConfigMode) {
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
    if (inConfigMode) {
      Serial.println("[STARTUP] Config mode triggered during startup");
      return;
    }
    
    // Step 1: Check WiFi (runs continuously until connected)
    if (!wifiConfirmed && WiFi.status() == WL_CONNECTED) {
      wifiConfirmed = true;
      Serial.println("[STARTUP] WiFi connected!");
    }
    
    // Step 2: Check Internet (once WiFi is connected and not yet checked)
    if (wifiConfirmed && !internetChecked) {
      Serial.println("[STARTUP] Checking Internet...");
      bool hasInternet = checkInternetConnectivity();
      internetChecked = true;
      if (hasInternet) {
        internetConfirmed = true;
        Serial.println("[STARTUP] Internet OK!");
      } else {
        Serial.println("[STARTUP] No Internet connection");
        if (internetErrorCount < 99) internetErrorCount++;
      }
    }
    
    // Step 3: Check Server (once Internet is confirmed and not yet checked)
    if (internetConfirmed && !serverChecked) {
      Serial.println("[STARTUP] Checking Server...");
      bool serverReachable = checkServerReachability();
      serverChecked = true;
      if (serverReachable) {
        serverConfirmed = true;
        Serial.println("[STARTUP] Server OK!");
      } else {
        Serial.println("[STARTUP] Server not reachable");
        if (serverErrorCount < 99) serverErrorCount++;
      }
    }
    
    // Step 4: Start WebSocket (once Server is confirmed and not yet started)
    if (serverConfirmed && !websocketStarted) {
      Serial.println("[STARTUP] Starting WebSocket connection...");
      if (thresholdKey.length() > 0) {
        webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
      } else {
        webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
      }
      webSocket.onEvent(webSocketEvent);
      webSocket.setReconnectInterval(1000);
      websocketStarted = true;
    }
    
    // Step 5: Process WebSocket events and check connection
    if (websocketStarted && !websocketConfirmed) {
      webSocket.loop(); // Process events
      if (webSocket.isConnected()) {
        websocketConfirmed = true;
        Serial.println("[STARTUP] WebSocket connected!");
      }
    }
    
    // Check if all connections are ready
    if (wifiConfirmed && internetConfirmed && serverConfirmed && websocketConfirmed) {
      allConnectionsReady = true;
      Serial.printf("[STARTUP] All connections ready after %.1f seconds!\n", (i + 1) * 0.1);
      break; // Exit startup screen early
    }
    
    // Progress indicator every 5 seconds
    if ((i + 1) % 50 == 0) {
      Serial.printf("[STARTUP] Progress: %.1fs - WiFi:%d Internet:%d Server:%d WS:%d\n", 
                    (i + 1) * 0.1, wifiConfirmed, internetConfirmed, serverConfirmed, websocketConfirmed);
    }
  }
  
  Serial.println("[STARTUP] Startup screen completed");
  
  // Determine what to show after startup screen
  if (allConnectionsReady) {
    Serial.println("[STARTUP] All connections successful - ready to show QR code");
    onErrorScreen = false;
    currentErrorType = 0;
  } else {
    // Show appropriate error screen based on what failed (priority order)
    if (!wifiConfirmed) {
      Serial.println("[STARTUP] WiFi failed - showing WiFi error");
      wifiReconnectScreen();
      onErrorScreen = true;
      currentErrorType = 1;
      if (wifiErrorCount < 99) wifiErrorCount++;
      
      // Handle WiFi failure
      if (ssid.length() == 0) {
        configMode();
        return;
      } else {
        checkAndReconnectWiFi();
        return;
      }
    } else if (!internetConfirmed) {
      Serial.println("[STARTUP] Internet failed - showing Internet error");
      internetReconnectScreen();
      onErrorScreen = true;
      currentErrorType = 2;
    } else if (!serverConfirmed) {
      Serial.println("[STARTUP] Server failed - showing Server error");
      serverReconnectScreen();
      onErrorScreen = true;
      currentErrorType = 3;
    } else if (!websocketConfirmed) {
      Serial.println("[STARTUP] WebSocket failed - showing WebSocket error");
      websocketReconnectScreen();
      onErrorScreen = true;
      currentErrorType = 4;
      if (websocketErrorCount < 99) websocketErrorCount++;
      
      // Start WebSocket if not yet started
      if (!websocketStarted) {
        if (thresholdKey.length() > 0) {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
        } else {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
        }
        webSocket.onEvent(webSocketEvent);
        webSocket.setReconnectInterval(1000);
      }
    }
  }

  // Button task already created earlier (before WiFi setup)
  
  // Initialize touch controller
  Serial.println("\n========================================");
  Serial.println("[TOUCH] Initializing touch controller...");
  Serial.printf("[TOUCH] I2C Pins: SDA=%d, SCL=%d\n", PIN_IIC_SDA, PIN_IIC_SCL);
  Serial.printf("[TOUCH] Touch Pins: RST=%d, IRQ=%d\n", PIN_TOUCH_RES, PIN_TOUCH_INT);
  
  touchAvailable = touch.begin();
  
  if (touchAvailable) {
    Serial.println("[TOUCH] ✓ Touch controller initialized successfully!");
    Serial.println("[TOUCH] Swipe in any direction or tap left/right side to navigate");
  } else {
    Serial.println("[TOUCH] ✗ Touch controller NOT available (non-touch version)");
  }
  Serial.println("========================================\n");
  
  // Set maxProducts based on multiControl mode
  if (multiControl == "quattro") {
    maxProducts = 4;
    Serial.println("[MULTI-CHANNEL-CONTROL] Quattro mode - 4 products available");
  } else if (multiControl == "duo") {
    maxProducts = 2;
    Serial.println("[MULTI-CHANNEL-CONTROL] Duo mode - 2 products available");
  } else {
    maxProducts = 1;
    Serial.println("[MULTI-CHANNEL-CONTROL] Single mode - 1 product");
  }
  
  // Initialize product navigation
  currentProduct = 0; // Start at selection screen
  
  setupComplete = true; // Allow loop to run now
}

void loop()
{
  // Wait for setup to complete before running loop
  if (!setupComplete)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
  
  // Display power saving status on first loop iteration
  static bool firstLoopStatusShown = false;
  if (!firstLoopStatusShown) {
    firstLoopStatusShown = true;
    Serial.println("\n================================");
    Serial.println("   POWER SAVING STATUS");
    Serial.println("================================");
    
    // Display screensaver status with clear description
    if (screensaver == "backlight") {
      Serial.println("Screensaver: backlight off");
    } else {
      Serial.println("Screensaver: " + screensaver);
    }
    
    // Display deep sleep status with clear description
    if (deepSleep == "light") {
      Serial.println("Deep Sleep: light sleep mode");
    } else if (deepSleep == "freeze") {
      Serial.println("Deep Sleep: deep sleep (freeze) mode");
    } else {
      Serial.println("Deep Sleep: " + deepSleep);
    }
    
    Serial.println("Activation Time: " + String(activationTimeoutMs / 60000) + " minutes (" + String(activationTimeoutMs) + " ms)");
    Serial.println("screensaverActive: " + String(screensaverActive));
    Serial.println("deepSleepActive: " + String(deepSleepActive));
    Serial.println("lastActivityTime: " + String(lastActivityTime));
    
    if (screensaver != "off" && deepSleep == "off") {
      Serial.println("\n⚡ MODE: SCREENSAVER ENABLED");
      Serial.println("   Backlight will turn off after inactivity");
    } else if (deepSleep != "off" && screensaver == "off") {
      Serial.println("\n⚡ MODE: DEEP SLEEP ENABLED (" + deepSleep + ")");
      Serial.println("   Device will sleep after inactivity");
    } else {
      Serial.println("\n⚡ MODE: POWER SAVING DISABLED");
    }
    Serial.println("================================\n");
  }
  
  // Screensaver and deep sleep checks are now inside the payment wait loop
  // to ensure they execute during payment waiting
  
  // If in config mode, do nothing - config mode is handled by button interrupt
  if (inConfigMode)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
  
  checkAndReconnectWiFi();
  if (inConfigMode) return; // Exit if we entered config mode

  payloadStr = "";
  
  // CRITICAL: Only show QR screen ONCE on first loop if ALL connections confirmed
  bool allConnectionsConfirmed = wifiConfirmed && internetConfirmed && serverConfirmed && websocketConfirmed;
  
  if (firstLoop && allConnectionsConfirmed && !inReportMode && !(lastWakeUpTime > 0 && (millis() - lastWakeUpTime) < GRACE_PERIOD_MS)) {
    Serial.println("[SCREEN] All connections confirmed - Showing QR code screen (READY FOR ACTION)");
    if (thresholdKey.length() > 0) {
      showThresholdQRScreen(); // THRESHOLD MODE (Multi-Channel-Control not compatible)
    } else if (multiControl != "off") {
      // MULTI-CHANNEL-CONTROL MODE - Start with product selection screen
      currentProduct = 0;
      productSelectionScreen();
      onProductSelectionScreen = true;
      productSelectionShowTime = millis();
      Serial.println("[MULTI-CHANNEL-CONTROL] Starting in product selection mode");
    } else if (specialMode != "standard") {
      // Generate LNURL for pin 12 before showing special mode QR
      String lnurlStr = generateLNURL(12);
      updateLightningQR(lnurlStr);
      showSpecialModeQRScreen(); // SPECIAL MODE
    } else {
      // Generate LNURL for pin 12 before showing normal QR
      String lnurlStr = generateLNURL(12);
      updateLightningQR(lnurlStr);
      showQRScreen(); // NORMAL MODE
    }
    // Clear error screen flag once QR is shown
    onErrorScreen = false;
    currentErrorType = 0;
    // Start product selection timer
    productSelectionShowTime = millis();
    onProductSelectionScreen = false;
  } else if (firstLoop && !allConnectionsConfirmed) {
    Serial.printf("[SCREEN] First loop - waiting for all connections (WiFi:%d, Internet:%d, Server:%d, WS:%d)\n", 
                  wifiConfirmed, internetConfirmed, serverConfirmed, websocketConfirmed);
  }
  
  firstLoop = false; // Mark first loop as completed

  unsigned long lastWiFiCheck = millis();
  lastPingTime = millis(); // Initialize global variable
  unsigned long loopCount = 0;
  // Don't reset onErrorScreen/currentErrorType - they should persist across loop iterations
  
  Serial.println("[LOOP] Entering payment wait loop...");
  Serial.printf("[LOOP] Initial paid state: %d\n", paid);
  Serial.printf("[LOOP] Error state: onErrorScreen=%d, currentErrorType=%d\n", onErrorScreen, currentErrorType);
  
  // Initialize ping/pong tracking
  lastPongTime = millis();
  waitingForPong = false;
  
  while (paid == false)
  {
    // Check if config mode was triggered during payment wait
    if (inConfigMode)
    {
      Serial.println("[LOOP] Config mode detected, exiting payment loop");
      return;
    }
    
    // Check for touch input (if available and not on error screen)
    static unsigned long lastTouchEvent = 0;
    static bool wasTouched = false;
    
    if (touchAvailable && !onErrorScreen) {
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
          lastActivityTime = millis();
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
        
        // Handle touch on product selection screen
        if (onProductSelectionScreen) {
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
          // Renamed for horizontal orientation with button on right:
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
            Serial.printf("[TOUCH] >>> %s - ", actionName.c_str());
            onProductSelectionScreen = false;
            
            // Multi-Channel-Control Mode: Navigate to next product
            if (multiControl != "off" && thresholdKey.length() == 0) {
              Serial.println("Navigate to next product");
              navigateToNextProduct();
            } else {
              // Normal/Special/Threshold Mode: Return to QR screen
              Serial.println("Returning to QR screen");
              redrawQRScreen();
            }
            // Reset timer
            productSelectionShowTime = millis();
          }
        }
        // Handle touch on product QR screen (Multi-Channel-Control mode only)
        // Allow navigation when showing product QR code
        else if (multiControl != "off" && thresholdKey.length() == 0 && !onProductSelectionScreen) {
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
          // BUT only on NEW touch (not held) to prevent continuous triggering
          else if (gesture == GESTURE_NONE && isTouched && !wasTouched) {
            if (y < 160 || y > 160) { // Left or right side
              navigate = true;
              actionName = "QUICK TOUCH";
            }
          }
          
          if (navigate) {
            Serial.printf("[TOUCH] >>> %s on product screen - Navigate to next product\n", actionName.c_str());
            navigateToNextProduct();
          }
        }
        
        // Update last touch time if any gesture detected or touch state changed
        if (gesture != GESTURE_NONE || (isTouched != wasTouched)) {
          lastTouchEvent = millis();
        }
        
        // Remember touch state for next iteration
        wasTouched = isTouched;
        
        // Any touch resets activity timer (for screensaver)
        lastActivityTime = millis();
        
        skip_product_touch_processing:
        ; // Empty statement required after label
      }
    }
    
    // Check if it's time to show product selection screen
    // Only show in Multi-Channel-Control mode or if multi-channel-control is off with old behavior
    // Not shown in Threshold mode (not compatible)
    if (!onErrorScreen && !onProductSelectionScreen && 
        productSelectionShowTime > 0 && 
        (millis() - productSelectionShowTime) >= PRODUCT_SELECTION_DELAY &&
        multiControl != "off" && thresholdKey.length() == 0) {
      Serial.println("[SCREEN] Showing product selection screen after 5 seconds (Multi-Channel-Control)");
      currentProduct = 0;
      productSelectionScreen();
      onProductSelectionScreen = true;
      // Don't reset timer - we want to stay on this screen until swipe
    }
    
    // Check for screensaver/deep sleep timeout activation (inside payment loop)
    if (!screensaverActive && !deepSleepActive && screensaver != "off" && deepSleep == "off") {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - lastActivityTime;
      
      // Debug output every 10 seconds
      static unsigned long lastDebugOutput = 0;
      if (currentTime - lastDebugOutput > 10000) {
        Serial.printf("[SCREENSAVER_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                      elapsedTime, activationTimeoutMs, (elapsedTime * 100.0 / activationTimeoutMs));
        lastDebugOutput = currentTime;
      }
      
      if (elapsedTime >= activationTimeoutMs) {
        Serial.println("[TIMEOUT] Screensaver timeout reached, activating screensaver");
        screensaverActive = true;
        activateScreensaver(screensaver);
        // Continue with payment loop - screensaver only turns off backlight
      }
    }
    
    if (!deepSleepActive && deepSleep != "off" && screensaver == "off") {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - lastActivityTime;
      
      // Debug output every 10 seconds
      static unsigned long lastDebugOutputDeep = 0;
      if (currentTime - lastDebugOutputDeep > 10000) {
        Serial.printf("[DEEP_SLEEP_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                      elapsedTime, activationTimeoutMs, (elapsedTime * 100.0 / activationTimeoutMs));
        lastDebugOutputDeep = currentTime;
      }
      
      if (elapsedTime >= activationTimeoutMs) {
        Serial.println("[TIMEOUT] Deep sleep timeout reached, preparing for deep sleep");
        deepSleepActive = true;
        
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
        setupDeepSleepWakeup(deepSleep);
        
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
        lastActivityTime = millis();
        lastWakeUpTime = millis();
        deepSleepActive = false;
        
        // Small delay before redrawing screen
        delay(500);
        
        // Redraw the appropriate QR screen
        redrawQRScreen();
        Serial.println("[WAKE_UP] Ready for payments");
      }
    }
    
    webSocket.loop();
    loopCount++;
    
    // Log status every 200000 loops (roughly every 10-20 minutes)
    if (loopCount % 200000 == 0)
    {
      Serial.printf("[LOOP] Still waiting... WiFi: %d, WS Connected: %d, paid: %d\n", 
                    WiFi.status() == WL_CONNECTED, webSocket.isConnected(), paid);
    }
    
    // Check Internet connectivity every 30 seconds (independent of WebSocket)
    if (millis() - lastInternetCheck > 30000 && !inConfigMode)
    {
      // CRITICAL: Check WiFi first! Don't show "No Internet" if WiFi is down
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[INTERNET] Skipping Internet check - WiFi is down");
        lastInternetCheck = millis();
      } else if (WiFi.status() == WL_CONNECTED) {
        bool hasInternet = checkInternetConnectivity();
        if (!hasInternet) {
          if (!onErrorScreen || currentErrorType > 2) {
            Serial.println("[INTERNET] Internet connection lost!");
            if (internetErrorCount < 99) internetErrorCount++;
            Serial.printf("[ERROR] Internet error count: %d\n", internetErrorCount);
            internetReconnectScreen();
            onErrorScreen = true;
            currentErrorType = 2; // Internet error
            // Reset product selection screen
            onProductSelectionScreen = false;
          }
          internetConfirmed = false; // Clear confirmation
          serverConfirmed = false; // Also clear server/websocket (they depend on Internet)
          websocketConfirmed = false;
          webSocket.disconnect();
        } else {
          // Internet OK - set confirmation
          if (!internetConfirmed) {
            internetConfirmed = true;
            Serial.println("[CONFIRMED] Internet connection confirmed!");
          }
        }
        lastInternetCheck = millis();
      }
    }
    
    // Send ping every 60 seconds to check if WebSocket connection is really alive
    // Only if WebSocket is connected!
    if (webSocket.isConnected() && millis() - lastPingTime > 60000 && !inConfigMode)
    {
      Serial.println("[PING] Sending WebSocket ping to verify connection...");
      webSocket.sendPing();
      lastPingTime = millis();
      waitingForPong = true;
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
        wifiConfirmed = false; // Clear all confirmations when WiFi is down
        internetConfirmed = false;
        serverConfirmed = false;
        websocketConfirmed = false;
        checkAndReconnectWiFi();
        if (inConfigMode) return;
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
      
      if (wifiOk && !serverOk && !inConfigMode)
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
          if (serverErrorCount < 99) serverErrorCount++;
          Serial.printf("[ERROR] Server error count: %d\n", serverErrorCount);
          Serial.println("[SCREEN] Showing Server error screen (type 3)");
          serverReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 3; // Server error
          // Reset product selection screen
          onProductSelectionScreen = false;
        }
        // Clear server/websocket confirmations
        serverConfirmed = false;
        websocketConfirmed = false;
        // Note: WebSocket can't connect if server is down - that's expected
        webSocket.disconnect();
        waitingForPong = false;
        return;
      }
      
      if (wifiOk && serverOk && !websocketOk && !inConfigMode)
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
          // Reset product selection screen
          onProductSelectionScreen = false;
        }
        
        // Try to reconnect WebSocket (up to 3 attempts)
        int reconnectAttempts = 0;
        int sslErrorCount = 0; // Count SSL connection errors (detected by error events)
        
        while (!webSocket.isConnected() && reconnectAttempts < 3 && !inConfigMode)
        {
          if (WiFi.status() != WL_CONNECTED || inConfigMode) return;
          
          webSocket.disconnect();
          vTaskDelay(pdMS_TO_TICKS(500));
          
          reconnectAttempts++;
          Serial.printf("WebSocket reconnect attempt %d/3\n", reconnectAttempts);
          
          if (thresholdKey.length() > 0) {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
          } else {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
          }
          
          // Wait for connection to establish (2 seconds)
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        if (webSocket.isConnected())
        {
          Serial.println("WebSocket reconnected successfully");
          websocketConfirmed = true; // Set confirmation
          onErrorScreen = false;
          currentErrorType = 0;
          return;
        }
        else
        {
          // WebSocket reconnect failed after 3 attempts
          Serial.printf("WebSocket reconnect failed after %d attempts\n", reconnectAttempts);
          if (websocketErrorCount < 99) websocketErrorCount++;
          Serial.printf("[ERROR] WebSocket error count: %d\n", websocketErrorCount);
          Serial.println("[SCREEN] Showing WebSocket error screen (type 4)");
          websocketReconnectScreen();
          websocketConfirmed = false; // Clear confirmation
          currentErrorType = 4;
          onErrorScreen = true;
          // Reset product selection screen
          onProductSelectionScreen = false;
          return;
        }
      }
      
      // Auto-recovery: All connections restored - set confirmation flags
      if (wifiOk && serverOk && websocketOk)
      {
        // Set all confirmation flags (so QR screen can be shown)
        if (!wifiConfirmed) {
          wifiConfirmed = true;
          Serial.println("[CONFIRMED] WiFi connection confirmed!");
        }
        if (!serverConfirmed) {
          serverConfirmed = true;
          Serial.println("[CONFIRMED] Server connection confirmed!");
        }
        if (!websocketConfirmed) {
          websocketConfirmed = true;
          Serial.println("[CONFIRMED] WebSocket connection confirmed!");
        }
        
        // Clear error state and redraw QR screen
        if (onErrorScreen) {
          Serial.printf("[RECOVERY] All connections recovered (was error type %d)\n", currentErrorType);
          Serial.println("[SCREEN] Clearing error screen and redrawing QR code");
          onErrorScreen = false;
          currentErrorType = 0;
          consecutiveWebSocketFailures = 0; // Reset failure counter
          waitingForPong = false;
          
          // Redraw QR screen immediately
          redrawQRScreen();
          Serial.println("[SCREEN] QR screen displayed after recovery");
          // Reset product selection timer
          productSelectionShowTime = millis();
          onProductSelectionScreen = false;
        }
        return;
      }
      
      lastWiFiCheck = millis();
    }
    if (paid)
    {
      Serial.println("[PAYMENT] Payment detected!");
      Serial.printf("[PAYMENT] PayloadStr: %s\n", payloadStr.c_str());
      
      if (thresholdKey.length() > 0) {
        // === THRESHOLD MODE ===
        Serial.println("[THRESHOLD] Processing payment in threshold mode...");
        
        // Parse JSON payload to get payment amount
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payloadStr);
        
        if (error) {
          Serial.print("[THRESHOLD] JSON parse error: ");
          Serial.println(error.c_str());
          paid = false;
          return;
        }
        
        // Extract payment amount from JSON
        JsonObject payment = doc["payment"];
        int payment_amount = payment["amount"]; // in mSats
        int payment_sats = payment_amount / 1000; // Convert to sats
        int threshold_sats = thresholdAmount.toInt();
        
        Serial.printf("[THRESHOLD] Payment received: %d sats (%d mSats)\n", payment_sats, payment_amount);
        Serial.printf("[THRESHOLD] Threshold: %d sats\n", threshold_sats);
        
        // Check if payment meets or exceeds threshold
        if (payment_sats >= threshold_sats) {
          Serial.println("[THRESHOLD] *** PAYMENT >= THRESHOLD! Triggering GPIO! ***");
          Serial.printf("[THRESHOLD] Switching GPIO %d for %d ms\n", 
                        thresholdPin.toInt(), thresholdTime.toInt());
          
          // Trigger GPIO pin
          int pin = thresholdPin.toInt();
          int duration = thresholdTime.toInt();
          
          // Check if special mode is enabled
          if (specialMode != "standard" && specialMode != "") {
            Serial.println("[THRESHOLD] Using special mode: " + specialMode);
            // Wake from screensaver if active
            if (screensaverActive) {
              screensaverActive = false;
              deactivateScreensaver();
            }
            lastActivityTime = millis(); // Reset screensaver timer on payment start
            
            actionTimeScreen();
            executeSpecialMode(pin, duration, frequency, dutyCycleRatio);
          } else {
            Serial.println("[THRESHOLD] Using standard mode");
            // Wake from screensaver if active
            if (screensaverActive) {
              screensaverActive = false;
              deactivateScreensaver();
            }
            lastActivityTime = millis(); // Reset screensaver timer on payment start
            actionTimeScreen();
            pinMode(pin, OUTPUT);
            digitalWrite(pin, HIGH);
            Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
            
            delay(duration);
            
            digitalWrite(pin, LOW);
            Serial.printf("[RELAY] Pin %d set LOW\n", pin);
          }
          
          thankYouScreen();
          lastActivityTime = millis(); // Reset screensaver timer on payment
          delay(2000);
          
          // Return to threshold QR screen
          showThresholdQRScreen();
          Serial.println("[THRESHOLD] Ready for next payment");
          // Reset product selection timer
          productSelectionShowTime = millis();
          onProductSelectionScreen = false;
        } else {
          Serial.printf("[THRESHOLD] Payment too small (%d < %d sats) - ignoring\n", 
                        payment_sats, threshold_sats);
        }
        
        paid = false;
        
      } else {
        // === NORMAL MODE ===
        Serial.println("[NORMAL] Processing payment in normal mode...");
        
        int pin = getValue(payloadStr, '-', 0).toInt();
        int duration = getValue(payloadStr, '-', 1).toInt();
        
        Serial.printf("[RELAY] Pin: %d, Duration: %d ms\n", pin, duration);
        
        // Check if special mode is enabled
        if (specialMode != "standard" && specialMode != "") {
          Serial.println("[NORMAL] Using special mode: " + specialMode);
          lastActivityTime = millis(); // Reset screensaver timer on payment start
          if (screensaverActive) {
            deactivateScreensaver();
            screensaverActive = false;
          }
          actionTimeScreen();
          executeSpecialMode(pin, duration, frequency, dutyCycleRatio);
        } else {
          Serial.println("[NORMAL] Using standard mode");
          lastActivityTime = millis(); // Reset screensaver timer on payment start
          if (screensaverActive) {
            deactivateScreensaver();
            screensaverActive = false;
          }
          actionTimeScreen();
          pinMode(pin, OUTPUT);
          digitalWrite(pin, HIGH);
          Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
          
          delay(duration);
          
          digitalWrite(pin, LOW);
          Serial.printf("[RELAY] Pin %d set LOW\n", pin);
        }
        
        thankYouScreen();
        lastActivityTime = millis(); // Reset screensaver timer on payment
        if (screensaverActive) {
          deactivateScreensaver();
          screensaverActive = false;
        }
        delay(2000);
        
        // Return to appropriate QR screen
        redrawQRScreen();
        Serial.println("[NORMAL] Ready for next payment");
        // Reset product selection timer
        productSelectionShowTime = millis();
        
        paid = false;
      }
    }
  }
  Serial.println("[LOOP] Exiting payment wait loop");
}