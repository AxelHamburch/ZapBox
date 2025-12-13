#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <HTTPClient.h>
#include <OneButton.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "FFat.h"

#include "PinConfig.h"
#include "Display.h"
#include "SerialConfig.h"

#define FORMAT_ON_FAIL true
#define PARAM_FILE "/config.json"

TaskHandle_t Task1;

String ssid = "";
String wifiPassword = "";
String switchStr = "";
const char *lightningPrefix = "LIGHTNING:";
const char *lnurl = "";
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

// Error screen tracking
bool onErrorScreen = false;
byte currentErrorType = 0; // 0=none, 1=WiFi (highest), 2=Internet, 3=Server, 4=WebSocket (lowest)
unsigned long lastPingTime = 0;
unsigned long lastInternetCheck = 0; // Track when we last checked Internet connectivity
byte consecutiveWebSocketFailures = 0; // Track consecutive WebSocket failures to detect Internet issues

// Buttons
OneButton leftButton(PIN_BUTTON_1, true);
OneButton rightButton(PIN_BUTTON_2, true);

WebSocketsClient webSocket;

//////////////////HELPERS///////////////////

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
  } else if (specialMode != "standard" && specialMode != "") {
    showSpecialModeQRScreen();
    Serial.println("[DISPLAY] Special mode QR screen displayed");
  } else {
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
    if (protocolIndex == -1) {
      Serial.println("Invalid switchStr: " + switchStr);
      lnbitsServer = "";
      deviceId = "";
    } else {
      int domainIndex = switchStr.indexOf("/", protocolIndex + 3);
      if (domainIndex == -1) {
        Serial.println("Invalid switchStr: " + switchStr);
        lnbitsServer = "";
        deviceId = "";
      } else {
        int uidLength = 22; // Length of device ID
        lnbitsServer = switchStr.substring(protocolIndex + 3, domainIndex);
        deviceId = switchStr.substring(switchStr.length() - uidLength);
      }
    }

    Serial.println("Socket: " + switchStr);
    Serial.println("LNbits server: " + lnbitsServer);
    Serial.println("Switch device ID: " + deviceId);

    const JsonObject maRoot3 = doc[3];
    const char *maRoot3Char = maRoot3["value"];
    String lnurlTemp = String(maRoot3Char);
    lnurlTemp.trim(); // Remove leading/trailing whitespace
    lnurl = lnurlTemp.c_str();

    // Copy values into lightning char, avoiding duplicate prefix
    String lnurlStr = String(lnurl);
    lnurlStr.toUpperCase(); // Convert to uppercase for comparison
    
    if (lnurlStr.startsWith("LIGHTNING:")) {
      // LNURL already has prefix, use as-is but ensure uppercase prefix
      String originalLnurl = String(lnurl);
      if (originalLnurl.startsWith("LIGHTNING:")) {
        strcpy(lightning, lnurl);
      } else if (originalLnurl.startsWith("lightning:")) {
        strcpy(lightning, "LIGHTNING:");
        strcat(lightning, lnurl + 10); // Skip "lightning:" (10 chars)
      }
    } else {
      // No prefix, add it
      strcpy(lightning, lightningPrefix);
      strcat(lightning, lnurl);
    }

    Serial.print("LNURL: ");
    Serial.println(lnurl);
    Serial.print("QR: ");
    Serial.println(lightning);

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

    // Display mode based on threshold configuration
    Serial.println("\n================================");
    if (thresholdKey.length() > 0) {
      Serial.println("       THRESHOLD MODE");
      Serial.println("================================");
      Serial.println("Threshold Key: " + thresholdKey);
      Serial.println("Threshold Amount: " + thresholdAmount + " sats");
      Serial.println("GPIO Pin: " + thresholdPin);
      Serial.println("Control Time: " + thresholdTime + " ms");
      
      // Process threshold LNURL (add LIGHTNING: prefix if not present)
      if (thresholdLnurl.length() > 0) {
        thresholdLnurl.trim(); // Remove leading/trailing whitespace
        String thresholdLnurlStr = thresholdLnurl;
        thresholdLnurlStr.toUpperCase();
        
        if (thresholdLnurlStr.startsWith("LIGHTNING:")) {
          // Already has prefix, use as-is but ensure uppercase prefix
          if (thresholdLnurl.startsWith("LIGHTNING:")) {
            strcpy(lightning, thresholdLnurl.c_str());
          } else if (thresholdLnurl.startsWith("lightning:")) {
            strcpy(lightning, "LIGHTNING:");
            strcat(lightning, thresholdLnurl.c_str() + 10);
          }
        } else {
          // No prefix, add it
          strcpy(lightning, lightningPrefix);
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
        
        // IMPORTANT: Check if Internet is still available
        Serial.println("[RECOVERY] Checking Internet after WiFi reconnect...");
        bool hasInternet = checkInternetConnectivity();
        
        if (!hasInternet) {
          // Internet still down - show Internet error
          Serial.println("[RECOVERY] Internet still down!");
          internetReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 2; // Internet error
          break; // Exit WiFi reconnect loop but keep error screen
        }
        
        // Internet OK - check Server
        Serial.println("[RECOVERY] Internet OK, checking Server...");
        bool serverReachable = checkServerReachability();
        
        if (!serverReachable) {
          // Server still down - show Server error
          Serial.println("[RECOVERY] Server still down!");
          serverReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 3; // Server error
          // Try to connect WebSocket anyway (will fail but that's expected)
        } else {
          // Server OK - clear error and reconnect WebSocket
          Serial.println("[RECOVERY] Server OK, reconnecting WebSocket...");
          onErrorScreen = false;
          currentErrorType = 0;
        }
        
        // Reconnect WebSocket after WiFi is back
        webSocket.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (thresholdKey.length() > 0) {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
        } else {
          webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
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
  
  errorReportScreen(wifiErrorCount, internetErrorCount, serverErrorCount, websocketErrorCount);
  vTaskDelay(pdMS_TO_TICKS(4200));
  wifiReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(2100));
  internetReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(2100));
  serverReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(2100));
  websocketReconnectScreen();
  vTaskDelay(pdMS_TO_TICKS(2100));
  
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
    if (thresholdKey.length() > 0) {
      showThresholdQRScreen();
    } else if (specialMode != "standard") {
      showSpecialModeQRScreen();
    } else {
      showQRScreen();
    }
  }
  
  inReportMode = false; // Clear flag AFTER showing final screen
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
  
  stepOneScreen();
  vTaskDelay(pdMS_TO_TICKS(3000));
  stepTwoScreen();
  vTaskDelay(pdMS_TO_TICKS(3000));
  stepThreeScreen();
  vTaskDelay(pdMS_TO_TICKS(3000));
  
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
    if (thresholdKey.length() > 0) {
      showThresholdQRScreen();
    } else if (specialMode != "standard") {
      showSpecialModeQRScreen();
    } else {
      showQRScreen();
    }
  }
}

void Task1code(void *pvParameters)
{
  for (;;)
  {
    leftButton.tick();
    rightButton.tick();
    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay - allows other tasks and proper button debouncing
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

  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN); // Force scanning for all APs, not just the first one
  WiFi.begin(ssid.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to WiFi");
  
  // Wait for WiFi connection, showing startup screen
  while (WiFi.status() != WL_CONNECTED && timer < 15000)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    timer = timer + 500;
  }
  
  // If WiFi connected, continue normally
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
  }
  else
  {
    // After 15 seconds without WiFi, check if we have credentials
    Serial.println("\nNo WiFi connection after 15 seconds");
    if (ssid.length() == 0)
    {
      // No credentials saved, go to config mode
      configMode();
    }
    else
    {
      // Have credentials but no connection, go to WiFi reconnect mode
      Serial.println("Entering WiFi reconnect mode...");
      checkAndReconnectWiFi();
    }
  }

  // Connect to WebSocket based on mode
  if (thresholdKey.length() > 0) {
    // THRESHOLD MODE - Connect to wallet
    Serial.println("[WS] Connecting in THRESHOLD MODE...");
    webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
  } else {
    // NORMAL MODE - Connect to specific switch
    Serial.println("[WS] Connecting in NORMAL MODE...");
    webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
  }
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(1000);

  // Button task already created earlier (before WiFi setup)
  
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
  
  // On first loop, wait longer to allow button press for config mode
  // This prevents QR screen from showing before user can press button
  int waitIterations = firstLoop ? 30 : 20; // 3s first time, 2s after
  firstLoop = false;
  
  // Wait but check for button presses every 100ms
  // This allows quick config mode entry without showing QR screen first
  for (int i = 0; i < waitIterations; i++) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (inConfigMode) return; // Exit immediately if config mode triggered
  }
  
  if (inConfigMode) return; // Final check before drawing screen
  
  // CRITICAL: Only show QR screen if no error is active AND not in report mode AND not in grace period!
  if (!onErrorScreen && !inReportMode && !(lastWakeUpTime > 0 && (millis() - lastWakeUpTime) < GRACE_PERIOD_MS)) {
    Serial.println("[SCREEN] Showing QR code screen (READY FOR ACTION)");
    if (thresholdKey.length() > 0) {
      showThresholdQRScreen(); // THRESHOLD MODE
    } else if (specialMode != "standard") {
      showSpecialModeQRScreen(); // SPECIAL MODE
    } else {
      showQRScreen(); // NORMAL MODE
    }
  } else if (inReportMode) {
    Serial.println("[SCREEN] Skipping QR screen - in report mode");
  } else if (lastWakeUpTime > 0 && (millis() - lastWakeUpTime) < GRACE_PERIOD_MS) {
    Serial.println("[SCREEN] Skipping QR screen - in grace period after wake-up");
  } else {
    Serial.printf("[SCREEN] Skipping QR screen - error active (type %d)\n", currentErrorType);
  }

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
      if (WiFi.status() == WL_CONNECTED) {
        bool hasInternet = checkInternetConnectivity();
        if (!hasInternet && !onErrorScreen) {
          Serial.println("[INTERNET] Internet connection lost!");
          if (internetErrorCount < 99) internetErrorCount++;
          Serial.printf("[ERROR] Internet error count: %d\n", internetErrorCount);
          internetReconnectScreen();
          onErrorScreen = true;
          currentErrorType = 2; // Internet error
          webSocket.disconnect();
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
      
      // Step 1: WiFi check (always)
      if (!wifiOk) {
        // WiFi down - skip all other checks
        serverOk = false;
        websocketOk = false;
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
      
      if (!wifiOk && !inConfigMode)
      {
        currentErrorType = 1; // WiFi error (highest priority)
        checkAndReconnectWiFi();
        if (inConfigMode) return;
        return;
      }
      
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
        }
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
          currentErrorType = 4;
          onErrorScreen = true;
          return;
        }
      }
      
      // Auto-recovery: All connections restored
      if (onErrorScreen && wifiOk && serverOk && websocketOk)
      {
        Serial.printf("[RECOVERY] All connections recovered (was error type %d)\n", currentErrorType);
        Serial.println("[SCREEN] Clearing error screen, returning to normal operation");
        onErrorScreen = false;
        currentErrorType = 0;
        consecutiveWebSocketFailures = 0; // Reset failure counter
        waitingForPong = false;
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
            
            specialModeScreen();
            executeSpecialMode(pin, duration, frequency, dutyCycleRatio);
          } else {
            Serial.println("[THRESHOLD] Using standard mode");
            // Wake from screensaver if active
            if (screensaverActive) {
              screensaverActive = false;
              deactivateScreensaver();
            }
            lastActivityTime = millis(); // Reset screensaver timer on payment start
            switchedOnScreen();
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
          specialModeScreen();
          executeSpecialMode(pin, duration, frequency, dutyCycleRatio);
        } else {
          Serial.println("[NORMAL] Using standard mode");
          lastActivityTime = millis(); // Reset screensaver timer on payment start
          if (screensaverActive) {
            deactivateScreensaver();
            screensaverActive = false;
          }
          switchedOnScreen();
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
        if (specialMode != "standard") {
          showSpecialModeQRScreen();
        } else {
          showQRScreen();
        }
        Serial.println("[NORMAL] Ready for next payment");
        
        paid = false;
      }
    }
  }
  Serial.println("[LOOP] Exiting payment wait loop");
}