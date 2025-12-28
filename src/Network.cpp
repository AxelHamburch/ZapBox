#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Network.h"
#include "DeviceState.h"
#include "GlobalState.h"
#include "Display.h"

// Externals from main.cpp
extern StateManager deviceState;
extern String lnbitsServer;
extern String payloadStr;
extern WebSocketsClient webSocket;
extern byte currentErrorType;
extern bool needsQRRedraw;
extern void fetchBitcoinData();
extern void fetchSwitchLabels();

// WebSocket event handler
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
  
  bool serverReachable = client.connect(lnbitsServer.c_str(), 443, 2000); // 2 second timeout
  
  if (serverReachable) {
    Serial.println("[TCP] Server is reachable (port 443 open)");
    client.stop();
  } else {
    Serial.println("[TCP] Server is NOT reachable (port 443 closed/timeout)");
  }
  
  return serverReachable;
}

// WiFi State Monitoring - Updates DeviceState based on WiFi connectivity
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
