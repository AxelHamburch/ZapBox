#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Network.h"
#include "DeviceState.h"
#include "GlobalState.h"
#include "Display.h"
#include "Log.h"

// Externals from main.cpp
extern StateManager deviceState;
extern String lnbitsServer;
extern String deviceId;
extern String payloadStr;
extern WebSocketsClient webSocket;
extern byte currentErrorType;
extern bool needsQRRedraw;
extern ExtensionConfig extensionConfig;
#if ENABLE_BITCOIN_DATA
extern void fetchBitcoinData();
#endif
extern void fetchSwitchLabels();

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  LOG_DEBUG("WebSocket", String("Event Type: ") + String(type) + String(" ConfigMode: ") + String((int)deviceState.isInState(DeviceState::CONFIG_MODE)));
  
  if (!deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    switch (type)
    {
    case WStype_DISCONNECTED:
      LOG_INFO("WebSocket", "Disconnected");
      break;
    case WStype_CONNECTED:
    {
      LOG_INFO("WebSocket", String("Connected to: ") + String((char*)payload));
      webSocket.sendTXT("Connected");
      networkStatus.lastPongTime = millis(); // Reset pong timer on connect
      networkStatus.waitingForPong = false;
      // Don't set networkStatus.confirmed.websocket = true here!
      // Let fetchSwitchLabels() validate the device config first
      // If labels load successfully (HTTP 200), it will set websocket = true
      // If instance doesn't exist (HTTP 404), it will set websocket = false
      LOG_INFO("WebSocket", "TCP connection established, fetching device config...");
      
      // Fetch switch labels from backend after successful connection
      fetchSwitchLabels();
    }
    break;
    case WStype_TEXT:
      LOG_DEBUG("WebSocket", String("Received: ") + String((char*)payload));
      payloadStr = (char *)payload;
      LOG_DEBUG("WebSocket", String("PayloadStr set to: ") + payloadStr);
      
      // Enqueue payment instead of just setting flag
      paymentQueue.enqueue(payloadStr);
      LOG_INFO("WebSocket", String("Payment enqueued. Queue size: ") + String(paymentQueue.size()));
      break;
    case WStype_PING:
      LOG_INFO("WebSocket", "Ping received from server");
      break;
    case WStype_PONG:
      LOG_INFO("WebSocket", "Pong received - connection alive");
      networkStatus.lastPongTime = millis();
      networkStatus.waitingForPong = false;
      break;
    case WStype_ERROR:
      LOG_ERROR("WebSocket", "Error occurred");
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
    LOG_DEBUG("WebSocket", "Event ignored - in config mode");
  }
}

// ─── NFC Bolt Card LNURLW handler ──────────────────────────────────────────
#ifdef ENABLE_NFC
/**
 * Called from the NFC FreeRTOS task (NFCBoltCard.cpp) when a valid LNURLW
 * is read from a Bolt Card (NTAG424 DNA).
 *
 * Determines the currently active relay pin (single mode → 12, multi-channel
 * mode → pin of the currently displayed product) and sends a WebSocket event
 * to the LNbits bitcoinswitch_extension, which is expected to:
 *   1. Create a Lightning invoice for the switch associated with <pin>
 *   2. Call the LNURLW endpoint (Bolt Card wallet) with that invoice
 *   3. Notify the device via the normal "paid" WS event once settled
 *
 * WS message format:
 *   {"event":"lnurlw", "lnurlw":"lnurlw://...", "pin":<activePin>}
 *
 * NOTE: The bitcoinswitch_extension server-side must be extended to handle
 * this "lnurlw" event (analogous to partytap_extension/views_ws.py).
 */
void nfcLnurlwReceived(const String &lnurlw)
{
#if ENABLE_NFC_TEST
  // Hardware-Test-Modus: Kein Server nötig.
  // Serial + Bildschirm zeigen den eingelesenen LNURLW.
  Serial.println("[NFC-TEST] Bolt Card gelesen:");
  Serial.println(lnurlw);
  nfcTestScreen(lnurlw); // Display.h
  return;
#endif
    LOG_INFO("NFC", "LNURLW received from Bolt Card – sending WS event");

    // Determine which relay pin is currently active.
    int activePin = 12; // Default: single mode uses pin 12.
    if (multiChannelConfig.mode != "off" && multiChannelConfig.currentProduct > 0)
    {
        switch (multiChannelConfig.currentProduct)
        {
            case 1: activePin = 12; break;
            case 2: activePin = 13; break;
            case 3: activePin = 10; break;
            case 4: activePin = 11; break;
            default: activePin = 12; break;
        }
    }

    LOG_INFO("NFC", String("Active relay pin for NFC payment: ") + String(activePin));

    // POST to extension: /<apiPath>/api/v1/nfc/<deviceId>?pin=<pin>
    // Body: {"lnurlw":"lnurlw://..."}
    // The extension creates an invoice, resolves the LNURLW withdraw request,
    // submits the invoice to the Bolt Card wallet, and the normal tasks.py
    // "paid" handler sends the relay trigger (pin-duration) via WebSocket.
    HTTPClient http;
    String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
                 + "/api/v1/nfc/" + deviceId + "?pin=" + String(activePin);
    LOG_INFO("NFC", String("Sending NFC request to: ") + url);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000); // LNURLW resolution can take several seconds

    String body = String("{\"lnurlw\":\"") + lnurlw + String("\"}" );

    // Set pending flag and show screen BEFORE sending the POST request.
    // The WebSocket "paid" event can arrive before http.POST() returns,
    // so we must set the flag early. We also call nfcPendingScreen() here
    // directly from the NFC task – the most reliable time to update the display.
    extensionConfig.nfcPaymentPending = true;
    extensionConfig.nfcPaymentPendingStart = millis();
    nfcPendingScreen();

    int httpCode = http.POST(body);

    if (httpCode == 200) {
        LOG_INFO("NFC", "NFC payment initiated – waiting for WebSocket paid event");
    } else {
        String resp = http.getString();
        LOG_ERROR("NFC", String("NFC payment failed: HTTP ") + String(httpCode) + " – " + resp);
        // Reset flag – no payment was initiated
        extensionConfig.nfcPaymentPending = false;
    }
    http.end();
}
#endif // ENABLE_NFC

// HTTP-based Internet check (doesn't require WebSocket connection)
// Tries multiple times to account for DNS/DHCP stabilization delays
bool checkInternetConnectivity()
{
  HTTPClient http;
  http.setTimeout(3000); // 3 second timeout per attempt
  
  LOG_INFO("Network", "Testing Internet connection...");
  
  // Try up to 3 times with small delays between attempts
  // First attempt might fail if DNS isn't ready yet
  const int maxAttempts = 3;
  const int delayBetweenAttempts = 500; // 500ms between retries
  
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    http.begin("http://clients3.google.com/generate_204"); // Google's connectivity check
    int httpCode = http.GET();
    http.end();
    
    bool hasInternet = (httpCode == 204 || httpCode == 301 || httpCode == 302 || httpCode > 0);
    
    if (hasInternet) {
      LOG_INFO("Network", String("Internet check: OK (HTTP ") + String(httpCode) + String(") - attempt ") + String(attempt));
      return true;
    }
    
    // If not last attempt, wait before retrying
    if (attempt < maxAttempts) {
      delay(delayBetweenAttempts);
    }
  }
  
  // All attempts failed
  LOG_INFO("Network", String("Internet check: FAILED after ") + String(maxAttempts) + String(" attempts"));
  return false;
}

// TCP-based Server reachability check (test if LNbits server port is open)
bool checkServerReachability()
{
  WiFiClient client;
  LOG_INFO("Network", String("Testing server: ") + lnbitsServer + String(":443..."));
  
  bool serverReachable = client.connect(lnbitsServer.c_str(), 443, 2000); // 2 second timeout
  
  if (serverReachable) {
    LOG_INFO("Network", "Server reachable (port 443 open)");
    client.stop();
  } else {
    LOG_WARN("Network", "Server NOT reachable (port 443 closed/timeout)");
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
    LOG_WARN("Network", "WiFi connection lost");
    if (networkStatus.errors.wifi < 99) networkStatus.errors.wifi++;
    LOG_ERROR("Network", String("WiFi error count: ") + String(networkStatus.errors.wifi));
    
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
      LOG_INFO("Network", "WiFi reconnection started (non-blocking)");
    }
    // If WiFi was confirmed before, auto-reconnect will handle it
  }
  else if (WiFi.status() == WL_CONNECTED && networkStatus.errors.wifi > 0 && deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && currentErrorType == 1)
  {
    // WiFi recovered while on error screen
    LOG_INFO("Network", "WiFi recovered");
    networkStatus.confirmed.wifi = true;
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
    needsQRRedraw = true;
    activityTracking.lastActivityTime = millis();
    productSelectionState.showTime = millis();
    
#if ENABLE_BITCOIN_DATA
    // Force BTC data refresh after WiFi recovery
    if (multiChannelConfig.btcTickerMode != "off") {
      LOG_INFO("Network", "Forcing BTC data refresh after WiFi recovery");
      bitcoinData.lastUpdate = 0; // Force immediate update
      fetchBitcoinData(); // Fetch data now
    }
#endif
  }
}
