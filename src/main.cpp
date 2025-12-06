#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
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

String payloadStr;
String lnbitsServer;
String deviceId;
bool paid = false;
byte testState = 0;
bool inConfigMode = false;
bool inReportMode = false;
// int relayPin;

// Error counters (0-9, capped at 9)
byte wifiErrorCount = 0;
byte internetErrorCount = 0;
byte websocketErrorCount = 0;

// Buttons
OneButton leftButton(PIN_BUTTON_1, true);
OneButton rightButton(PIN_BUTTON_2, true);

WebSocketsClient webSocket;

//////////////////HELPERS///////////////////

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
    StaticJsonDocument<1500> doc;
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
    Serial.println("");
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

void checkAndReconnectWiFi()
{
  if (WiFi.status() != WL_CONNECTED && !inConfigMode)
  {
    Serial.println("WiFi connection lost! Reconnecting...");
    if (wifiErrorCount < 9) wifiErrorCount++;
    Serial.printf("[ERROR] WiFi error count: %d\n", wifiErrorCount);
    wifiReconnectScreen();
    
    // Keep trying to reconnect forever (unless config mode is triggered)
    while (WiFi.status() != WL_CONNECTED && !inConfigMode)
    {
      WiFi.disconnect();
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN); // Force scanning for all APs, not just the first one
      WiFi.begin(ssid.c_str(), wifiPassword.c_str());
      
      int reconnectTimer = 0;
      while (WiFi.status() != WL_CONNECTED && reconnectTimer < 10000 && !inConfigMode)
      {
        delay(500);
        Serial.print(".");
        reconnectTimer += 500;
      }
      
      // Exit if config mode was triggered
      if (inConfigMode)
      {
        Serial.println("\nConfig mode triggered during WiFi reconnect!");
        break;
      }
      
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nWiFi reconnected!");
        
        // Reconnect WebSocket after WiFi is back
        webSocket.disconnect();
        delay(100);
        webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
        Serial.println("WebSocket reconnected!");
        break;
      }
      else
      {
        Serial.println("\nRetrying WiFi connection...");
      }
    }
  }
}

void configMode()
{
  Serial.println("Config mode triggered");
  inConfigMode = true;
  delay(100); // Give loop() time to check the flag
  configModeScreen();
  Serial.println("Config mode screen displayed, entering serial config...");
  bool hasExistingData = (ssid.length() > 0);
  Serial.printf("Has existing data: %s\n", hasExistingData ? "YES" : "NO");
  configOverSerialPort(ssid, wifiPassword, hasExistingData);
}

void reportMode()
{
  Serial.println("[REPORT] Report mode activated");
  errorReportScreen(wifiErrorCount, internetErrorCount, websocketErrorCount);
  delay(4200);
  wifiReconnectScreen();
  delay(2100);
  internetReconnectScreen();
  delay(2100);
  websocketReconnectScreen();
  delay(2100);
  
  // Show QR screen again immediately
  if (thresholdKey.length() > 0) {
    showThresholdQRScreen();
  } else {
    showQRScreen();
  }
  
  inReportMode = true;
  // Flag will cause loop to restart
}

void showHelp()
{
  stepOneScreen();
  delay(3000);
  stepTwoScreen();
  delay(3000);
  stepThreeScreen();
  delay(3000);
  
  // Show appropriate screen based on mode
  if (thresholdKey.length() > 0) {
    showThresholdQRScreen();
  } else {
    showQRScreen();
  }
}

void Task1code(void *pvParameters)
{
  for (;;)
  {
    leftButton.tick();
    rightButton.tick();
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

  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN); // Force scanning for all APs, not just the first one
  WiFi.begin(ssid.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to WiFi");
  
  // Wait for WiFi connection, showing startup screen
  while (WiFi.status() != WL_CONNECTED && timer < 15000)
  {
    delay(500);
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

  leftButton.setPressTicks(3000);
  leftButton.attachClick(reportMode);
  leftButton.attachLongPressStart(configMode);
  rightButton.attachClick(showHelp);

  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1",   /* Name of the task */
      10000,     /* Stack size in words */
      NULL,      /* Task input parameter */
      0,         /* Priority of the task */
      &Task1,    /* Task handle. */
      0);        /* Core where the task should run */
}

void loop()
{
  // If in config mode, do nothing - config mode is handled by button interrupt
  if (inConfigMode)
  {
    delay(100);
    return;
  }
  
  checkAndReconnectWiFi();
  if (inConfigMode) return; // Exit if we entered config mode

  payloadStr = "";
  delay(2000);
  
  if (inConfigMode) return; // Check again before drawing screen
  
  // Show appropriate QR screen based on mode
  if (thresholdKey.length() > 0) {
    showThresholdQRScreen(); // THRESHOLD MODE
  } else {
    showQRScreen(); // NORMAL MODE
  }

  unsigned long lastWiFiCheck = millis();
  unsigned long lastPingTime = millis();
  unsigned long loopCount = 0;
  bool onErrorScreen = false;
  
  Serial.println("[LOOP] Entering payment wait loop...");
  Serial.printf("[LOOP] Initial paid state: %d\n", paid);
  
  // Initialize ping/pong tracking
  lastPongTime = millis();
  waitingForPong = false;
  
  while (paid == false)
  {
    // Check if report mode was triggered - return to show QR screen again
    if (inReportMode)
    {
      Serial.println("[REPORT] Report mode finished, restarting loop");
      inReportMode = false;
      return;
    }
    
    // Check if config mode was triggered during payment wait
    if (inConfigMode)
    {
      Serial.println("[LOOP] Config mode detected, exiting payment loop");
      return;
    }
    
    webSocket.loop();
    loopCount++;
    
    // Log status every 200000 loops (roughly every 10-20 minutes)
    if (loopCount % 200000 == 0)
    {
      Serial.printf("[LOOP] Still waiting... WiFi: %d, WS Connected: %d, paid: %d\n", 
                    WiFi.status() == WL_CONNECTED, webSocket.isConnected(), paid);
    }
    
    // Send ping every 60 seconds to check if connection is really alive
    if (millis() - lastPingTime > 60000 && !inConfigMode)
    {
      Serial.println("[PING] Sending WebSocket ping to verify connection...");
      webSocket.sendPing();
      lastPingTime = millis();
      waitingForPong = true;
    }
    
    // Check if pong response is missing (connection might be dead - no internet/server)
    if (waitingForPong && (millis() - lastPingTime > 10000) && !inConfigMode)
    {
      Serial.println("[PING] No pong response for 10 seconds - no internet connection!");
      if (!onErrorScreen)
      {
        if (internetErrorCount < 9) internetErrorCount++;
        Serial.printf("[ERROR] Internet error count: %d\n", internetErrorCount);
        internetReconnectScreen(); // Show "NO INTERNET" screen
        onErrorScreen = true;
      }
      // Force disconnect to trigger reconnect logic
      webSocket.disconnect();
      waitingForPong = false;
    }
    
    // Check WiFi and WebSocket every 5 seconds while waiting for payment
    if (millis() - lastWiFiCheck > 5000)
    {
      Serial.println("[CHECK] Performing 5-second check...");
      
      // Check if WiFi is still connected
      if (WiFi.status() != WL_CONNECTED && !inConfigMode)
      {
        Serial.println("[CHECK] WiFi lost, attempting reconnect...");
        // WiFi lost, try to reconnect
        checkAndReconnectWiFi();
        if (inConfigMode) return; // Exit if we entered config mode
        // After reconnect, restart loop to show QR screen again
        return;
      }
      
      // Check if WebSocket is still connected (only if WiFi is OK)
      if (!webSocket.isConnected() && WiFi.status() == WL_CONNECTED && !inConfigMode)
      {
        if (!onErrorScreen)
        {
          Serial.println("[CHECK] WebSocket disconnected! Attempting reconnect...");
          if (websocketErrorCount < 9) websocketErrorCount++;
          Serial.printf("[ERROR] WebSocket error count: %d\n", websocketErrorCount);
          websocketReconnectScreen();
          onErrorScreen = true;
        }
        
        // Try to reconnect WebSocket up to 5 times (or until config mode is triggered)
        int reconnectAttempts = 0;
        while (!webSocket.isConnected() && reconnectAttempts < 5 && WiFi.status() == WL_CONNECTED && !inConfigMode)
        {
          webSocket.disconnect();
          delay(500);
          
          // Exit if config mode was triggered
          if (inConfigMode)
          {
            Serial.println("[WS] Config mode triggered during WebSocket reconnect!");
            return;
          }
          
          Serial.printf("[WS] Reconnect attempt %d/5...\n", reconnectAttempts + 1);
          
          // Reconnect based on mode
          if (thresholdKey.length() > 0) {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + thresholdKey);
          } else {
            webSocket.beginSSL(lnbitsServer, 443, "/api/v1/ws/" + deviceId);
          }
          
          delay(2000); // Wait for connection
          reconnectAttempts++;
        }
        
        if (webSocket.isConnected())
        {
          Serial.println("[WS] Reconnected successfully!");
          onErrorScreen = false;
          // Return to restart loop and show QR screen again
          return;
        }
        else
        {
          Serial.println("[WS] Reconnect failed, will retry in 5 seconds...");
          // Stay on websocket error screen, will retry in next 5-second check
        }
      }
      else if (webSocket.isConnected() && onErrorScreen)
      {
        // WebSocket came back (maybe through library's auto-reconnect)
        Serial.println("[CHECK] WebSocket reconnected automatically!");
        onErrorScreen = false;
        // Return to restart loop and show QR screen again
        return;
      }
      else if (webSocket.isConnected())
      {
        Serial.println("[CHECK] WebSocket is connected OK");
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
        StaticJsonDocument<1900> doc;
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
          
          switchedOnScreen();
          
          // Trigger GPIO pin
          int pin = thresholdPin.toInt();
          int duration = thresholdTime.toInt();
          
          pinMode(pin, OUTPUT);
          digitalWrite(pin, HIGH);
          Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
          
          delay(duration);
          
          digitalWrite(pin, LOW);
          Serial.printf("[RELAY] Pin %d set LOW\n", pin);
          
          thankYouScreen();
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
        
        switchedOnScreen();
        
        int pin = getValue(payloadStr, '-', 0).toInt();
        int duration = getValue(payloadStr, '-', 1).toInt();
        
        Serial.printf("[RELAY] Pin: %d, Duration: %d ms\n", pin, duration);
        
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        Serial.printf("[RELAY] Pin %d set HIGH\n", pin);
        
        delay(duration);
        
        digitalWrite(pin, LOW);
        Serial.printf("[RELAY] Pin %d set LOW\n", pin);
        
        thankYouScreen();
        delay(2000);
        
        // Return to QR screen
        showQRScreen();
        Serial.println("[NORMAL] Ready for next payment");
        
        paid = false;
      }
    }
  }
  Serial.println("[LOOP] Exiting payment wait loop");
}