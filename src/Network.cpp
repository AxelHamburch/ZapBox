#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Network.h"
#include "PinConfig.h"
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

    // NFC is only supported by zapbox_extension.
    // If the device is configured with bitcoinswitch_extension, the server-side
    // has no /api/v1/nfc/ endpoint – silently swallowing the tap would leave the
    // device stuck on the PENDING NFC screen.  Abort early and inform the user.
    if (extensionConfig.apiPath != "zapbox")
    {
        LOG_WARN("NFC", "Bolt Card tap detected – NFC is not supported by the active extension ("
                        + extensionConfig.apiPath + ").");
        LOG_WARN("NFC", "To use NFC / Bolt Cards, switch to zapbox_extension (apiPath = \"zapbox\").");
        LOG_WARN("NFC", "Tap ignored – device continues normal operation.");
        extensionConfig.nfcExtensionMismatch = true; // Signal main loop to show error screen
        return;
    }

    LOG_INFO("NFC", "LNURLW received from Bolt Card – sending WS event");

    // Block NFC payments when any sensor condition is active (GPIO sensors + IOExpander sensors)
    if (lightBarrierConfig.isAnyBlocking() || ioExpanderConfig.isAnySensorBlocking()) {
        LOG_WARN("NFC", "NFC tap blocked — sensor blocking active");
        return;
    }

    // Determine which relay pin is currently active.
    // In servo mode, use productToPin() which skips inactive channels.
    // Products 1-4 → RELAY_CHANNEL_PINS[0-3] (GPIO 12/13/10/11).
    // Products 5-12 → virtual IOExpander pins 200-207 (PCF8574 P0-P7).
    int activePin = RELAY_CHANNEL_PINS[0]; // Default: CH01 / GPIO 12
    if (multiChannelConfig.mode != "off" && multiChannelConfig.currentProduct > 0)
    {
        if (multiChannelConfig.mode == "servo") {
            activePin = servoConfig.productToPin(multiChannelConfig.currentProduct);
        } else {
            int prod = multiChannelConfig.currentProduct;
            if (prod >= 5 && prod <= 12) {
                activePin = 200 + (prod - 5); // IOExpander virtual pins 200-207
            } else {
                int idx = prod - 1;
                if (idx >= 0 && idx < RELAY_CHANNEL_MAX) {
                    activePin = RELAY_CHANNEL_PINS[idx];
                }
            }
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

    // Set pending flag BEFORE the POST so the main loop (Core 1) can show
    // the PENDING screen.  DO NOT call nfcPendingScreen() here – this function
    // runs on Core 0 (NFC task) and TFT_eSPI is NOT thread-safe.  Concurrent
    // SPI access from two cores corrupts the display controller (horizontal
    // stripes / offset images that persist until power-cycle).
    extensionConfig.nfcPaymentPending = true;
    extensionConfig.nfcPaymentPendingStart = millis(); // starts the timeout in main.cpp

    // Retry loop for connection-level failures (negative HTTP codes).
    // These mean the request never reached the server (SSL timeout, DNS failure,
    // connection refused), so retrying is safe — no duplicate payment risk.
    // This covers the common case where the SSL stack isn't fully ready shortly
    // after boot (first NFC tap fails, second succeeds).
    const int maxRetries = 2;
    int httpCode = 0;
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        if (attempt > 0) {
            LOG_WARN("NFC", String("Retrying NFC request (attempt ") + String(attempt + 1) + "/" + String(maxRetries + 1) + ")...");
            http.end();
            delay(1000); // Wait 1s before retry — let SSL/network stack stabilize
            http.begin(url);
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(15000);
        }
        httpCode = http.POST(body);
        if (httpCode >= 0) break; // Got a server response (success or HTTP error) — stop retrying
        LOG_WARN("NFC", String("Connection failed (HTTP ") + String(httpCode) + ") on attempt " + String(attempt + 1));
    }

    if (httpCode == 200) {
        // Timer was already started before the POST – do NOT reset it here.
        // The 45s timeout in main.cpp counts from the card tap, covering both
        // the HTTP POST duration and the subsequent WebSocket "paid" wait.
        LOG_INFO("NFC", "NFC payment initiated – waiting for WebSocket paid event");
    } else {
        String resp = http.getString();
        LOG_ERROR("NFC", String("NFC payment HTTP ") + String(httpCode) + " – " + resp);

        // Extract the "detail" field from the JSON response and store it for display.
        extensionConfig.nfcErrorDetail[0] = '\0';
        int detailStart = resp.indexOf("\"detail\":\"");
        if (detailStart >= 0) {
            detailStart += 10; // skip `"detail":"`
            int detailEnd = resp.indexOf("\"", detailStart);
            if (detailEnd > detailStart) {
                String detail = resp.substring(detailStart, detailEnd);
                detail.toCharArray(extensionConfig.nfcErrorDetail, sizeof(extensionConfig.nfcErrorDetail));
            }
        }

        // Any non-200 response is terminal: if the server returned an error code,
        // it will never send a WebSocket "paid" event for this tap.
        // Negative HTTP code → connection never reached server (retries exhausted).
        // HTTP 4xx / 5xx    → server processed the request and rejected it.
        String detailStr = String(extensionConfig.nfcErrorDetail);
        bool isTerminal = (httpCode < 0) || (httpCode >= 400);
        if (isTerminal) {
            extensionConfig.nfcPaymentPending = false;
            extensionConfig.nfcPaymentFailed  = true;
            if (httpCode < 0) {
                String connErr = "Connection failed";
                connErr.toCharArray(extensionConfig.nfcErrorDetail, sizeof(extensionConfig.nfcErrorDetail));
                LOG_WARN("NFC", "Connection failed after retries – showing NO LUCK immediately");
            } else {
                LOG_WARN("NFC", String("HTTP ") + String(httpCode) + " – showing NO LUCK immediately");
            }
        }
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
