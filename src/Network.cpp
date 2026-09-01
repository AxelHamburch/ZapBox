#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Network.h"
#include "PinConfig.h"
#include "DeviceState.h"
#include "GlobalState.h"
#include "Display.h"
#include "Log.h"
#include "NetTls.h"

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
extern bool labelsLoadedSuccessfully;

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
      networkStatus.lastPongTime       = millis(); // Reset pong timer on connect
      networkStatus.wsConnectedTime    = millis(); // Track connect time for internet-check skip
      networkStatus.lastServerPingTime = 0;        // Stale ping from a previous connection must not
                                                     // feed the half-open watchdog right after reconnect
      networkStatus.waitingForPong     = false;
      // Only fetch the switch config when we don't already have it.
      //
      // Fetching on EVERY connect opened a second HTTPS connection to the same
      // host ~1.5 s after each WebSocket handshake. On routers that can't hold
      // two connections to the same host:443 that left the WebSocket half-open
      // (device thinks connected, server has no connection and sends no pings),
      // the 60 s ping watchdog reconnected, the fetch fired again, and the device
      // never reached a stable connection. The periodic refresh in
      // updateSwitchLabels() keeps the config current instead — including a
      // retry loop while labelsLoadedSuccessfully is still false.
      if (labelsLoadedSuccessfully) {
        // fetchSwitchLabels() is normally what validates the device config and
        // confirms the WebSocket (API.cpp). With a cached config that validation
        // already happened, so confirm here — otherwise the connection stays
        // unconfirmed and triggers the WebSocket error screen.
        LOG_INFO("WebSocket", "TCP connection established — config cached, skipping fetch");
        networkStatus.confirmed.websocket = true;
      } else {
        // Don't set networkStatus.confirmed.websocket = true here!
        // Let fetchSwitchLabels() validate the device config first
        // If labels load successfully (HTTP 200), it will set websocket = true
        // If instance doesn't exist (HTTP 404), it will set websocket = false
        LOG_INFO("WebSocket", "TCP connection established, fetching device config...");
        fetchSwitchLabels();
      }
    }
    break;
    case WStype_TEXT:
      LOG_DEBUG("WebSocket", String("Received: ") + String((char*)payload));
      payloadStr = (char *)payload;
      LOG_DEBUG("WebSocket", String("PayloadStr set to: ") + payloadStr);

      // nfc_enrolled is a teach confirmation — do not trigger any relay.
      if (payloadStr.indexOf("\"nfc_enrolled\"") >= 0) {
        LOG_INFO("WebSocket", "nfc_enrolled event — card enrolled, no relay");
        if (authyConfig.enabled) {
          authyState.infoMsg   = "Card enrolled";
          authyState.infoUntil = millis() + 2000;
        }
        break;
      }

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

// ─── Device WebSocket channel (device → server) ─────────────────────────────
// Persistent second WebSocket to the zapbox_extension
// (/zapbox/api/v1/ws/nfc/<deviceId>). The core WS stays the trigger channel
// (paid events, pings, "No active connections" check — all unchanged); this
// one carries device-initiated events, currently the Bolt Card lnurlw.
//
// Why: some routers fail NEW TLS connections in phases (any destination)
// while established connections keep working in both directions — proven by
// a WS pong round-tripping while a fresh mempool.space handshake failed in
// the same second. A tap over this channel needs no fresh connection.
// When the channel is down (old extension, bad phase at boot), the tap falls
// back to the HTTPS POST below — compatible in both directions.
WebSocketsClient nfcWebSocket;
static char          nfcWsOutbox[512];        // lnurlw event queued by the NFC task
static volatile bool nfcWsOutboxPending = false;
static bool          nfcWsStarted       = false;
static bool          nfcWsHoldsTlsSlot  = false;
static uint32_t      nfcWsSlotHeldSince = 0;
// Escalating pause between failed connect phases. An extension without the
// endpoint (older release) would otherwise be probed with a fresh TLS
// handshake every few seconds, forever — precisely the connection pressure
// this channel exists to avoid. 30 s → 1 → 2 → 4 → 8 min, reset on connect.
static uint8_t       nfcWsFailedPhases   = 0;
static uint32_t      nfcWsNextAttemptDue = 0;

static void nfcWebSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type) {
    case WStype_CONNECTED:
        LOG_INFO("NFC-WS", "Device channel connected – taps use the persistent channel");
        nfcWsFailedPhases   = 0; // endpoint exists — connect retries at full speed again
        nfcWsNextAttemptDue = 0;
        break;
    case WStype_DISCONNECTED:
        // Drop a queued-but-unsent tap: after a reconnect it would arrive
        // seconds late and its request_id would be rejected as stale anyway.
        nfcWsOutboxPending = false;
        break;
    case WStype_TEXT: {
        JsonDocument doc;
        if (deserializeJson(doc, payload, length)) break;
        if (strcmp(doc["event"] | "", "lnurlw_result") != 0) break;
        uint32_t reqId = (uint32_t) String(doc["request_id"] | "").toInt();
        if (reqId != extensionConfig.nfcRequestId) {
            LOG_WARN("NFC-WS", "Stale lnurlw_result ignored");
            break;
        }
        const char *status = doc["status"] | "";
        if (strcmp(status, "OK") == 0) {
            // Same as HTTP 200: invoice submitted — the relay trigger arrives
            // via the core WebSocket ("paid" event). PENDING screen continues;
            // a PIN-protected card additionally gets pin_required via core WS.
            LOG_INFO("NFC", "NFC payment initiated – waiting for WebSocket paid event");
        } else {
            const char *detail = doc["detail"] | "Payment failed";
            strlcpy(extensionConfig.nfcErrorDetail, detail, sizeof(extensionConfig.nfcErrorDetail));
            extensionConfig.nfcPaymentPending = false;
            extensionConfig.nfcPaymentFailed  = true;
            LOG_WARN("NFC", String("Device channel result: ERROR – ") + detail);
        }
        break;
    }
    default:
        break;
    }
}

// Called from the main loop (Core 1). Services the channel, flushes taps
// queued by the NFC task (Core 0 — sendTXT is not safe across cores), and
// gates its own TLS handshake through the NetTls slot.
void serviceNfcWebSocket()
{
    if (extensionConfig.apiPath != "zapbox" || !nfcConfig.boltcardActive ||
        deviceState.isInState(DeviceState::CONFIG_MODE) || WiFi.status() != WL_CONNECTED) {
        if (nfcWsHoldsTlsSlot) { nfcWsHoldsTlsSlot = false; netTlsGive(); }
        return;
    }

    // Between failed connect phases: stay quiet until the backoff expires.
    // (During an active phase nfcWsNextAttemptDue lies in the past.)
    if (!nfcWebSocket.isConnected() &&
        (int32_t)(millis() - nfcWsNextAttemptDue) < 0) {
        return;
    }

    if (!nfcWsStarted) {
        // Open only after the main WebSocket is validated and up: its handshake
        // has priority, and the router settle window separates the two connects.
        if (!networkStatus.confirmed.websocket || !webSocket.isConnected()) return;
        if (!netTlsTryTake("NFC-WS")) return;
        nfcWsHoldsTlsSlot  = true;
        nfcWsSlotHeldSince = millis();
        LOG_INFO("NFC-WS", String("Opening device channel: /zapbox/api/v1/ws/nfc/") + deviceId);
        nfcWebSocket.beginSSL(lnbitsServer, 443, "/zapbox/api/v1/ws/nfc/" + deviceId);
        nfcWebSocket.onEvent(nfcWebSocketEvent);
        nfcWebSocket.setReconnectInterval(5000);
        // Device-side protocol pings keep the channel alive and detect a
        // half-open socket; the server answers pongs automatically.
        nfcWebSocket.enableHeartbeat(20000, 6000, 3);
        nfcWsStarted = true;
    }

    if (nfcWebSocket.isConnected()) {
        if (nfcWsHoldsTlsSlot) { nfcWsHoldsTlsSlot = false; netTlsGive(); }
        if (nfcWsOutboxPending) {
            nfcWebSocket.sendTXT(nfcWsOutbox);
            nfcWsOutboxPending = false;
            LOG_INFO("NFC-WS", "lnurlw event sent over device channel");
        }
        nfcWebSocket.loop();
        return;
    }

    // Disconnected → loop() drives a TLS handshake. Same hold pattern as
    // serviceWebSocket(): keep the slot across the connecting phase, capped
    // so a permanently failing channel can't starve other TLS users.
    if (!nfcWsHoldsTlsSlot) {
        if (!webSocket.isConnected()) return; // main WS recovers first
        if (!netTlsTryTake("NFC-WS")) return;
        nfcWsHoldsTlsSlot  = true;
        nfcWsSlotHeldSince = millis();
    } else if (millis() - nfcWsSlotHeldSince > 15000) {
        // Connect phase over without a connection — release the slot and back
        // off before the next phase (see nfcWsFailedPhases above).
        nfcWsHoldsTlsSlot = false;
        netTlsGive();
        if (nfcWsFailedPhases < 5) nfcWsFailedPhases++;
        uint32_t pauseMs = 15000UL << nfcWsFailedPhases; // 30s, 1, 2, 4, 8 min
        nfcWsNextAttemptDue = millis() + pauseMs;
        LOG_INFO("NFC-WS", String("Channel not connecting (extension too old?) – next attempt in ")
                           + String(pauseMs / 1000) + " s");
        return;
    }
    nfcWebSocket.loop();
}

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

    // Numeric selection: a Bolt Card can only pay the product QR currently
    // shown. On the keypad/select/ticker screens the tap is silently ignored.
    // Only applies in multi-channel mode — Single channel and Mini-PoS handle
    // their own guards and must not be blocked here.
    #ifdef BOARD_JC3248W535C
    if (t35AmbientConfig.numericSelect &&
        multiChannelConfig.mode != "off" &&
        !miniPosState.invoicePending &&
        !productSelectState.qrActive) {
        LOG_INFO("NFC", "Numeric selection: no product QR shown – Bolt Card tap ignored");
        return;
    }
    #endif

    // Block NFC payments when any sensor condition is active
    if (lightBarrierConfig.isAnyBlocking()) {
        LOG_WARN("NFC", "NFC tap blocked — sensor blocking active");
        return;
    }

    // Determine which relay pin is currently active.
    // In servo mode, use productToPin() which skips inactive channels.
    // Products 1-4 → RELAY_CHANNEL_PINS[0-3] (GPIO 12/13/10/11).
    // Products 5-12 → virtual IOExpander pins 200-207 (PCF8574 P0-P7).
    // The PCF8575 (300-315) and MCP23017 (400-415) pins have no product number —
    // product browsing only covers 1-12. They are reached via numeric selection
    // (qrPin below), a Bolt Card tap on the displayed QR, or a direct LNbits trigger.
    int activePin = RELAY_CHANNEL_PINS[0]; // Default: CH01 / GPIO 12
    #ifdef BOARD_JC3248W535C
    // Numeric selection: the displayed product QR defines the pin directly
    // (supports direct GPIOs and virtual pins 200-207 / 300-315 / 400-415)
    if (t35AmbientConfig.numericSelect && productSelectState.qrActive &&
        productSelectState.qrPin > 0) {
        activePin = productSelectState.qrPin;
    } else
    #endif
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
    // Own TLS client so the handshake can be bounded. This matters more than the
    // connect timeout: setConnectTimeout() only limits the TCP connect (the
    // select() on the socket), NOT the TLS handshake — and WiFiClientSecure
    // defaults that to 120 s. On a router that silently drops the handshake the
    // POST then sat on a half-open port-443 socket for a full two minutes, long
    // enough to take the WebSocket down with it. Same pattern as fetchBitcoinData().
    WiFiClientSecure secureClient;
    secureClient.setInsecure();           // same trust model as http.begin(url)
    secureClient.setHandshakeTimeout(10); // seconds

    String body = String("{\"lnurlw\":\"") + lnurlw + String("\"}" );

    // Set pending flag BEFORE the POST so the main loop (Core 1) can show
    // the PENDING screen.  DO NOT call nfcPendingScreen() here – this function
    // runs on Core 0 (NFC task) and TFT_eSPI is NOT thread-safe.  Concurrent
    // SPI access from two cores corrupts the display controller (horizontal
    // stripes / offset images that persist until power-cycle).
    extensionConfig.nfcPaymentPending = true;
    extensionConfig.nfcPaymentPendingStart = millis(); // starts the timeout in main.cpp
    // Snapshot the request id. If the client gives up on this tap (60s pending
    // timeout, PIN cancel, ...) before this blocking call returns, nfcRequestId
    // is bumped and this snapshot goes stale — see the isTerminal check below.
    uint32_t myRequestId = ++extensionConfig.nfcRequestId;

    // Preferred transport: the persistent device channel (serviceNfcWebSocket).
    // The tap then rides an ESTABLISHED connection — no fresh TLS handshake,
    // which is exactly what flaky routers drop in bad phases. The reply arrives
    // as an "lnurlw_result" event (nfcWebSocketEvent); a lost reply is covered
    // by the pending timeout in main.cpp. sendTXT() is not safe from this task
    // (Core 0), so the payload is handed to Core 1 via the outbox.
    if (nfcWebSocket.isConnected() && !nfcWsOutboxPending) {
        JsonDocument doc;
        doc["event"]      = "lnurlw";
        doc["request_id"] = String(myRequestId);
        doc["lnurlw"]     = lnurlw;
        doc["pin"]        = activePin;
        if (miniPosConfig.enabled && miniPosState.invoicePending) {
            doc["minipos_hash"] = miniPosState.paymentHash;
        }
        size_t written = serializeJson(doc, nfcWsOutbox, sizeof(nfcWsOutbox));
        if (written > 0 && written < sizeof(nfcWsOutbox) - 1) {
            nfcWsOutboxPending = true;
            LOG_INFO("NFC", "Tap queued on device channel (no new TLS connection)");
            return;
        }
        LOG_WARN("NFC", "Payload too large for device channel – falling back to HTTPS");
    }

    LOG_INFO("NFC", String("Sending NFC request to: ") + url);

    // Retry loop for connection-level failures (negative HTTP codes).
    // These mean the request never reached the server (SSL timeout, DNS failure,
    // connection refused), so retrying is safe — no duplicate payment risk.
    // This covers the common case where the SSL stack isn't fully ready shortly
    // after boot (first NFC tap fails, second succeeds).
    //
    // Two attempts, not three: whenever attempt 1 failed on a struggling link,
    // every further attempt failed too — the failures come in phases rather than
    // independently. The third attempt therefore only added ~10 s of PENDING
    // before the user saw NO LUCK. Failing sooner lets them tap again, which is
    // a better chance than retrying inside the same bad phase.
    const int maxRetries = 1;
    int httpCode = 0;
    bool holdingSlot = false;
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        if (attempt > 0) {
            // If WiFi itself dropped (e.g. the WebSocket is fighting for the same
            // TLS/socket resources and lost), further attempts will just time out
            // again for another 15s each — abort now instead of prolonging the
            // outage and holding the NFC task in a dead retry loop.
            if (WiFi.status() != WL_CONNECTED) {
                LOG_WARN("NFC", "WiFi down – aborting NFC retries early");
                httpCode = -1;
                break;
            }
            LOG_WARN("NFC", String("Retrying NFC request (attempt ") + String(attempt + 1) + "/" + String(maxRetries + 1) + ")...");
            http.end();
            delay(1000); // Wait 1s before retry — let SSL/network stack stabilize
        }

        // Only one TLS connection may be established at a time (see NetTls.h).
        // Taken PER ATTEMPT and released again on failure: holding it across the
        // whole retry sequence starved the WebSocket reconnect on Core 1 for the
        // full duration of a hanging POST.
        if (!netTlsTake("NFC", 20000)) {
            LOG_WARN("NFC", "TLS slot busy – aborting attempt");
            httpCode = -1;
            break;
        }
        holdingSlot = true;

        http.begin(secureClient, url);
        http.addHeader("Content-Type", "application/json");
        http.setConnectTimeout(8000);
        http.setTimeout(15000); // LNURLW resolution can take several seconds

        httpCode = http.POST(body);
        if (httpCode >= 0) break; // Got a server response — keep the slot to read it
        LOG_WARN("NFC", String("Connection failed (HTTP ") + String(httpCode) + ") on attempt " + String(attempt + 1));
        // Let the WebSocket have the connection while we back off before retrying.
        netTlsGive();
        holdingSlot = false;
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
            if (extensionConfig.nfcRequestId != myRequestId) {
                // The client already gave up on this tap (60s timeout / PIN cancel)
                // while this call was stuck retrying. Showing NO LUCK now would
                // reopen a flow the user already saw finish — ignore it.
                LOG_WARN("NFC", "Stale NFC response ignored – client already gave up on this tap");
            } else {
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
    }
    http.end();
    if (holdingSlot) netTlsGive(); // connection closed – starts the router settle window
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
    WiFiClientSecure secureClient;         // bounded handshake — see nfcLnurlwReceived()
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(10);  // seconds
    String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
                 + "/api/v1/nfc/pin_submit?session_id=" + sessionId
                 + "&pin=" + pin;
    LOG_INFO("PIN", String("PIN submit URL: ") + url);
    if (!netTlsTake("PIN", 10000)) {
        LOG_WARN("PIN", "TLS slot busy – PIN submit aborted");
        pinPadState.errorMsg   = "Connection failed";
        pinPadState.showError  = true;
        pinPadState.errorStart = millis();
        return;
    }
    http.begin(secureClient, url);
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
    netTlsGive();
}
#endif // ENABLE_NFC

// ─── Authy teach PIN submit ─────────────────────────────────────────────────
/**
 * Open a PIN-protected LNURL-auth teach session. Unlike the payment PIN
 * (which is relayed via WebSocket), the teach endpoint answers synchronously:
 *
 *   POST /<apiPath>/api/v1/auth/teach/start?device_id=<id>&pin=<xxxxxx>
 *   → {"status":"OK","ttl":300}
 *   → {"status":"ERROR","reason":"wrong_pin","remaining":2}
 *   → {"status":"ERROR","reason":"locked"}  (3 wrong PINs — re-enable in LNbits)
 *
 * Returns true when the session opened. On failure the error is written into
 * pinPadState so the existing PIN pad screen shows the remaining attempts.
 */
bool submitTeachPin(const String &pin)
{
    LOG_INFO("Teach", "Submitting teach PIN to server");
    if (!netTlsTake("Teach", 10000)) {
        LOG_WARN("Teach", "TLS slot busy – teach PIN submit aborted");
        pinPadState.errorMsg   = "Connection failed";
        pinPadState.showError  = true;
        pinPadState.errorStart = millis();
        return false;
    }
    HTTPClient http;
    WiFiClientSecure secureClient;         // bounded handshake — see nfcLnurlwReceived()
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(10);  // seconds
    String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
                 + "/api/v1/auth/teach/start?device_id=" + deviceId
                 + "&pin=" + pin;
    http.begin(secureClient, url);
    http.setConnectTimeout(5000);
    http.setTimeout(7000);

    int httpCode = http.POST("");
    String resp = http.getString();
    http.end();
    netTlsGive();

    if (httpCode != 200) {
        LOG_ERROR("Teach", String("Teach start HTTP ") + String(httpCode));
        pinPadState.errorMsg   = (httpCode < 0) ? "Connection failed" : "Server error";
        pinPadState.showError  = true;
        pinPadState.errorStart = millis();
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        pinPadState.errorMsg   = "Bad response";
        pinPadState.showError   = true;
        pinPadState.errorStart  = millis();
        return false;
    }
    const char *status = doc["status"] | "ERROR";
    if (strcmp(status, "OK") == 0) {
        LOG_INFO("Teach", "Teach session opened");
        return true;
    }

    const char *reason = doc["reason"] | "wrong_pin";
    memset(pinPadState.digits, 0, sizeof(pinPadState.digits));
    pinPadState.numDigits  = 0;
    pinPadState.showError  = true;
    pinPadState.errorStart = millis();
    if (strcmp(reason, "locked") == 0) {
        pinPadState.errorMsg = "Teach locked";
        pinPadState.blocked  = true;
    } else if (strcmp(reason, "no_teach_pin") == 0) {
        pinPadState.errorMsg = "No teach PIN set";
        pinPadState.blocked  = true;
    } else {
        int remaining = doc["remaining"] | 0;
        pinPadState.errorMsg = String(remaining) + " attempt" + (remaining == 1 ? "" : "s") + " left";
        pinPadState.blocked  = (remaining <= 0);
    }
    LOG_WARN("Teach", String("Teach PIN rejected: ") + reason);
    return false;
}

/**
 * Close an open teach session (user cancel / done).
 *   POST /<apiPath>/api/v1/auth/teach/stop?device_id=<id>
 */
void stopTeachSession()
{
    if (!netTlsTake("Teach", 5000)) {
        LOG_WARN("Teach", "TLS slot busy – teach stop skipped (server times out on its own)");
        return;
    }
    HTTPClient http;
    WiFiClientSecure secureClient;         // bounded handshake — see nfcLnurlwReceived()
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(10);  // seconds
    String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
                 + "/api/v1/auth/teach/stop?device_id=" + deviceId;
    http.begin(secureClient, url);
    http.setConnectTimeout(5000);
    http.setTimeout(7000);
    http.POST("");
    http.end();
    netTlsGive();
    LOG_INFO("Teach", "Teach session stop requested");
}

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
