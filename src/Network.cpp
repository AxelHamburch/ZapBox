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
extern bool onErrorScreen;
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
      networkStatus.lastPongTime    = millis(); // Reset pong timer on connect
      networkStatus.wsConnectedTime = millis(); // Track connect time for internet-check skip
      networkStatus.waitingForPong  = false;
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
      networkStatus.lastServerPingTime = millis();
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

    // Mini-PoS: a Bolt Card can only pay a pending invoice. On the amount
    // entry screen (no invoice) the tap is silently ignored.
    if (miniPosConfig.enabled && !miniPosState.invoicePending) {
        LOG_INFO("NFC", "Mini-PoS: no invoice pending – Bolt Card tap ignored");
        return;
    }

    // Block NFC payments when any sensor condition is active
    if (lightBarrierConfig.isAnyBlocking()) {
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
    // Mini-PoS: tell the extension to pay the pending invoice instead of
    // creating a new one from the switch configuration.
    if (miniPosConfig.enabled && miniPosState.invoicePending) {
        url += "&minipos_hash=" + miniPosState.paymentHash;
    }
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

// ─── PIN Submit ─────────────────────────────────────────────────────────────
/**
 * Called from main.cpp (Core 1) when the user has entered 4 PIN digits.
 * POSTs pin + session_id to zapbox_extension so it can unblock the suspended
 * LNURLW callback coroutine and append &pin=XXXX to the boltcards URL.
 *
 * Endpoint: POST /<apiPath>/api/v1/nfc/pin_submit?session_id=<id>&pin=<xxxx>
 * The server responds: {"status":"OK"} on success.
 * Errors (wrong PIN, card blocked) are relayed back via WebSocket as
 * {"event":"pin_error", ...} and handled in processPaymentEvent() (main.cpp).
 */
void sendPinSubmit(const String &sessionId, const String &pin)
{
    LOG_INFO("PIN", "Submitting PIN to server");

    HTTPClient http;
    String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
                 + "/api/v1/nfc/pin_submit?session_id=" + sessionId
                 + "&pin=" + pin;
    LOG_INFO("PIN", String("PIN submit URL: ") + url);
    http.begin(url);
    http.setConnectTimeout(5000);
    http.setTimeout(7000);

    int httpCode = http.POST("");
    if (httpCode == 200) {
        LOG_INFO("PIN", "PIN submitted – waiting for WS response");
    } else {
        String resp = http.getString();
        LOG_ERROR("PIN", String("PIN submit HTTP ") + String(httpCode) + " – " + resp);
        // Connection failure: show error immediately (no WS event will follow)
        if (httpCode < 0) {
            pinPadState.errorMsg  = "Connection failed";
            pinPadState.showError = true;
            pinPadState.errorStart = millis();
        }
    }
    http.end();
}
#endif // ENABLE_NFC

// HTTP-based Internet check (doesn't require WebSocket connection)
// Tries multiple times to account for DNS/DHCP stabilization delays
bool checkInternetConnectivity()
{
  LOG_INFO("Network", "Testing Internet connection...");

  // Multiple URLs tried in order — stops as soon as one succeeds.
  // Google's generate_204 is first: it responds reliably and quickly.
  // 1.1.1.1 is IP-based (no DNS) but can read-timeout on some networks.
  static const char* const checkUrls[] = {
    "http://clients3.google.com/generate_204",           // Google captive-portal check (fastest)
    "http://connectivitycheck.gstatic.com/generate_204", // Google fallback
    "http://1.1.1.1",                                    // Cloudflare — no DNS needed
  };
  const int numUrls = sizeof(checkUrls) / sizeof(checkUrls[0]);

  for (int i = 0; i < numUrls; i++) {
    HTTPClient http;
    http.setConnectTimeout(2000); // 2 s TCP connect timeout
    http.setTimeout(2000);        // 2 s read timeout
    http.begin(checkUrls[i]);
    int httpCode = http.GET();
    http.end();

    if (httpCode > 0) {
      LOG_INFO("Network", String("Internet check: OK (HTTP ") + String(httpCode)
               + ") via " + String(checkUrls[i]));
      return true;
    }
    LOG_INFO("Network", String("Internet check: ") + String(checkUrls[i])
             + " failed (" + String(httpCode) + ")");
  }

  // TCP fallbacks — no DNS needed, no HTTP needed. Works on routers that
  // block HTTP to external IPs. Port 53 (DNS) is almost never firewalled.
  struct { const char* ip; uint16_t port; const char* label; } tcpFallbacks[] = {
    { "8.8.8.8",  53, "Google DNS 8.8.8.8:53"     },
    { "8.8.4.4",  53, "Google DNS 8.8.4.4:53"     },
    { "1.1.1.1",  53, "Cloudflare DNS 1.1.1.1:53" },
  };
  for (auto& t : tcpFallbacks) {
    WiFiClient c;
    bool ok = c.connect(t.ip, t.port, 2000);
    if (ok) c.stop();
    if (ok) {
      LOG_INFO("Network", String("Internet check: OK (TCP) via ") + t.label);
      return true;
    }
  }

  // Last resort: TCP connection to the LNbits server itself.
  if (lnbitsServer.length() > 0) {
    WiFiClient client;
    bool ok = client.connect(lnbitsServer.c_str(), 443, 3000);
    if (ok) client.stop();
    if (ok) {
      LOG_INFO("Network", String("Internet check: OK (LNbits TCP) via ") + lnbitsServer + ":443");
      return true;
    }
    LOG_INFO("Network", String("Internet check: LNbits TCP also failed — ") + lnbitsServer + ":443");
  }

  LOG_INFO("Network", "Internet check: FAILED (all URLs tried)");
  return false;
}

// TCP-based Server reachability check (test if LNbits server port is open).
// Retries up to 3 times with a 2 s pause — a single transient DNS failure
// (common on Fritzbox at startup) must not cause a 20-second startup delay.
bool checkServerReachability()
{
  LOG_INFO("Network", String("Testing server: ") + lnbitsServer + String(":443..."));

  const int   maxAttempts  = 3;
  const int   retryDelayMs = 2000;

  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    WiFiClient client;
    bool ok = client.connect(lnbitsServer.c_str(), 443, 2000);
    if (ok) {
      client.stop();
      if (attempt > 1) {
        LOG_INFO("Network", String("Server reachable on attempt ") + String(attempt));
      } else {
        LOG_INFO("Network", "Server reachable (port 443 open)");
      }
      return true;
    }
    if (attempt < maxAttempts) {
      LOG_WARN("Network", String("Server check attempt ") + String(attempt)
               + "/" + String(maxAttempts) + " failed — retrying in 2 s...");
      delay(retryDelayMs);
    }
  }

  LOG_WARN("Network", "Server NOT reachable (port 443 closed/timeout)");
  return false;
}

// After AUTH_FAIL: pause briefly, then try once more.
static bool     wifiAuthFailed      = false;
static unsigned long wifiAuthRetryAt = 0;   // millis() when next retry is allowed
static const unsigned long WIFI_AUTH_RETRY_MS = 1000; // 1 s between retries

void initWiFiEventHandler() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      uint8_t reason = info.wifi_sta_disconnected.reason;
      // Reason 202 = AUTH_FAIL (wrong password), 201 = AUTH_EXPIRE, 15 = 4WAY_HANDSHAKE_TIMEOUT
      if (reason == 202 || reason == 201 || reason == 15) {
        WiFi.setAutoReconnect(false); // stop continuous storm of retries
        wifiAuthFailed = true;
        wifiAuthRetryAt = millis() + WIFI_AUTH_RETRY_MS;
        LOG_ERROR("Network", String("WiFi auth failed (reason ") + String(reason) + ") — retrying in 1 s");
      }
    }
  });
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
  if (WiFi.status() != WL_CONNECTED && !deviceState.isInState(DeviceState::CONFIG_MODE))
  {
    if (!deviceState.isInState(DeviceState::ERROR_RECOVERABLE)) {
      deviceState.transition(DeviceState::ERROR_RECOVERABLE);
      currentErrorType = 1;
      wifiReconnectScreen();
    }

    // After AUTH_FAIL: wait 1 s, then try once more (transient auth issues are common)
    if (wifiAuthFailed) {
      if (millis() < wifiAuthRetryAt) {
        return; // still in cool-down, stay on NO WIFI screen
      }
      // Cool-down expired — attempt one reconnect
      wifiAuthFailed = false;
      WiFi.setAutoReconnect(true);
      WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.wifiPassword.c_str());
      LOG_INFO("Network", "WiFi auth retry after 1 s cool-down");
      return;
    }

    LOG_WARN("Network", "WiFi connection lost");
    if (networkStatus.errors.wifi < 99) networkStatus.errors.wifi++;

    // Start WiFi reconnect but don't block waiting for it
    if (!networkStatus.confirmed.wifi) {
      WiFi.disconnect();
      delay(50);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(true);
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.wifiPassword.c_str());
      LOG_INFO("Network", "WiFi reconnection started (non-blocking)");
    }
  }
  else if (WiFi.status() == WL_CONNECTED && networkStatus.errors.wifi > 0 && deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && currentErrorType == 1)
  {
    // WiFi recovered while on error screen
    LOG_INFO("Network", "WiFi recovered");
    networkStatus.confirmed.wifi = true;
    deviceState.transition(DeviceState::READY);
    currentErrorType = 0;
    onErrorScreen = false;
    needsQRRedraw = true;
    activityTracking.lastActivityTime = millis();
    productSelectionState.showTime = millis();
    
#if ENABLE_BITCOIN_DATA
    // Mark BTC data stale so the periodic updater fetches it without blocking here.
    bitcoinData.lastUpdate = 0;
#endif
  }
}
