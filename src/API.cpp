#include "API.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Log.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// External references to main.cpp
extern StateManager deviceState;
extern MultiChannelConfig multiChannelConfig;
extern ProductLabels productLabels;
extern BitcoinData bitcoinData;
extern NetworkStatus networkStatus;
extern String lnbitsServer;
extern String deviceId;
extern String currency;
extern bool labelsLoadedSuccessfully;
extern bool labelsValidationAttempted;
extern ExtensionConfig extensionConfig;

// External constants from main.cpp
const unsigned long LABEL_UPDATE_INTERVAL = 300000; // 5 minutes

// Retry backoff for failed label fetches
static unsigned long lastFetchAttempt = 0;
static const unsigned long RETRY_BACKOFF = 30000; // 30 seconds between retries
static bool apiPathLoaded = false; // NVS path loaded only once per boot

// Load persisted apiPath from NVS (called first time fetchSwitchLabels runs)
static void loadApiPathIfNeeded() {
  if (apiPathLoaded) return;
  apiPathLoaded = true;
  Preferences prefs;
  prefs.begin("zapbox", true); // read-only
  String saved = prefs.getString("apiPath", "");
  prefs.end();
  if (saved.length() > 0 && (saved == "zapbox" || saved == "bitcoinswitch")) {
    extensionConfig.apiPath = saved;
    Serial.println("[LABELS] Loaded saved extension path from NVS: " + saved);
  }
}

#if ENABLE_BITCOIN_DATA
const unsigned long BTC_UPDATE_INTERVAL = 300000; // 5 minutes
const unsigned long BTC_ERROR_RETRY_INTERVAL = 60000; // 1 minute retry for errors
static bool btcDataHasError = false; // Track if last BTC fetch had errors

// External function declarations from main.cpp
extern void btctickerScreen();
extern void updateBtctickerValues(); // Partial update function
#endif

/**
 * Fetch switch labels and configuration from LNbits server.
 */
void fetchSwitchLabels()
{
  loadApiPathIfNeeded();
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    Serial.println("[LABELS] Cannot fetch labels - server or deviceId not configured");
    lastFetchAttempt = millis(); // apply backoff so this doesn't spam every loop tick
    return;
  }

  // SAFETY: Abort if WiFi is being torn down for config mode.
  // configMode() sets CONFIG_MODE on Core 0 before WiFi.disconnect();
  // if we start an HTTPS request while WiFi is shutting down, the SSL
  // stack will crash with LoadProhibited.
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    Serial.println("[LABELS] Skipping fetch - CONFIG_MODE active");
    return;
  }

  // Update last attempt time to prevent rapid retries
  lastFetchAttempt = millis();

  HTTPClient http;
  // Try primary extension path. On 404 or connection error, try the other extension.
  // Detected path is persisted in NVS so restarts go directly to the right path.
  String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath + "/api/v1/public/" + deviceId;
  Serial.println("[LABELS] Fetching switch configurations from: " + url);
  http.begin(url);
  http.setTimeout(4000);
  int httpCode = http.GET();

  // Auto-detect extension path: try fallback on 404 or any connection error
  bool shouldTryFallback = (httpCode == 404) || (httpCode <= 0);
  if (shouldTryFallback) {
    String fallbackPath = (extensionConfig.apiPath == "bitcoinswitch") ? "zapbox" : "bitcoinswitch";
    Serial.println("[LABELS] " + extensionConfig.apiPath + (httpCode == 404 ? " returned 404" : " connection error") + " - trying fallback: " + fallbackPath);
    http.end();
    url = "https://" + lnbitsServer + "/" + fallbackPath + "/api/v1/public/" + deviceId;
    Serial.println("[LABELS] Fetching switch configurations from: " + url);
    http.begin(url);
    http.setTimeout(5000);
    httpCode = http.GET();
    if (httpCode == 200) {
      extensionConfig.apiPath = fallbackPath;
      Serial.println("[LABELS] Auto-detected extension: /" + fallbackPath + "/api/v1/ (saved for payments)");
      // Persist so next boot uses the correct path directly
      Preferences prefs;
      prefs.begin("zapbox", false);
      prefs.putString("apiPath", fallbackPath);
      prefs.end();
    }
  }

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("[LABELS] Received response: " + payload);
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Extract currency from response (optional field – not all extensions send it)
      const char* currencyChar = doc["currency"];
      if (currencyChar != nullptr) {
        String oldCurrency = currency;
        currency = String(currencyChar);
        currency.toUpperCase(); // Ensure uppercase for display and API calls
        LOG_INFO("LABELS", "Currency set by server: " + currency +
                           (oldCurrency != currency ? " (was: " + oldCurrency + ")" : ""));
      } else {
        // Server did not include a currency field – keep the current value (default: USD).
        // This is normal for extensions that don't configure currency per-device.
        LOG_INFO("LABELS", "No currency in API response – using: " + currency);
      }
      
      // Clear existing labels and durations (12 channels: 4 shared + 8 headless-only)
      for (int i = 0; i < 12; i++) {
        productLabels.labels[i] = "";
        productLabels.durations[i] = 0;
      }
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        int pinDuration = switchObj["duration"].as<int>(); // Action time in ms (0 if not set)
        
        // Store label and duration based on pin number using array index (0-11)
        int pinIndex = getPinIndex(pin);
        if (pinIndex >= 0 && pinIndex < 12) {
          productLabels.labels[pinIndex] = labelStr;
          productLabels.durations[pinIndex] = pinDuration;
          Serial.println("[LABELS] Pin " + String(pin) + " label: " + labelStr + " duration: " + String(pinDuration) + " ms");
        }
      }
      
      Serial.println("[LABELS] Successfully fetched and cached all labels");
      labelsLoadedSuccessfully = true; // Mark labels as successfully loaded
      labelsValidationAttempted = true; // Mark validation as completed
      productLabels.lastUpdate = millis(); // Update timestamp
      
      // WebSocket connection is valid - device config exists on server
      // This is the ONLY place where we confirm WebSocket after validation
      Serial.println("[LABELS] Device config validated - confirming WebSocket connection");
      networkStatus.confirmed.websocket = true;
      
#if ENABLE_BITCOIN_DATA
      // Mark BTC data as stale so the periodic updater in loop() refreshes it
      // without blocking here. Calling fetchBitcoinData() inline caused setup()
      // to stall for 20+ s on SSL timeouts, preventing touch from responding.
      bitcoinData.lastUpdate = 0;
      Serial.println("[LABELS] BTC update scheduled (will fetch at next ticker cycle)");
      if (multiChannelConfig.btcTickerActive) {
        Serial.println("[LABELS] Ticker active - refreshing display");
        btctickerScreen();
      }
#endif
    } else {
      Serial.println("[LABELS] JSON parsing failed: " + String(error.c_str()));
    }
  } else {
    Serial.printf("[LABELS] HTTP request failed with code: %d\n", httpCode);
    
    // HTTP 404 means the bitcoinswitch instance was deleted on the server
    // This is a critical configuration error - invalidate WebSocket connection
    if (httpCode == 404) {
      Serial.println("[LABELS] ERROR: Device not found (404) - tried /bitcoinswitch/ and /zapbox/ paths.");
      Serial.println("[LABELS] The configured device ID does not exist on the server.");
      Serial.println("[LABELS] Marking WebSocket as unconfirmed to trigger error LED pattern.");
      networkStatus.confirmed.websocket = false;
      labelsValidationAttempted = true; // Mark validation as completed (failed)
      labelsLoadedSuccessfully = false;
    }
    // Other HTTP errors (500, timeout, etc.) - could be temporary
    else if (httpCode < 0) {
      Serial.println("[LABELS] Connection error - could not reach server (will retry soon)");
      labelsValidationAttempted = true; // Mark as attempted so startup doesn't hang
      labelsLoadedSuccessfully = false;
    } else if (httpCode >= 500) {
      Serial.println("[LABELS] Server error - may be temporary, will retry");
      labelsValidationAttempted = true;
      labelsLoadedSuccessfully = false;
    }
  }
  
  http.end();
}

#if ENABLE_BITCOIN_DATA
/**
 * Fetch Bitcoin price and block height from external APIs (sequential).
 */
void fetchBitcoinData()
{
  // SAFETY: Abort immediately if WiFi is being torn down for config mode.
  // configMode() calls WiFi.disconnect(true) on Core 0; if we start an HTTPS
  // request on Core 1 at the same time the SSL/WiFi stack is freed underneath
  // us, the result is a LoadProhibited crash (EXCVADDR ~0x130).
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    Serial.println("[BTC] Skipping fetch - CONFIG_MODE active");
    return;
  }

  Serial.println("[BTC] Fetching Bitcoin data...");
  
  // Update last fetch attempt time for backoff
  lastFetchAttempt = millis();
  
  HTTPClient http;
  // mempool.space /api/v1/prices supports: USD, EUR, GBP, CAD, CHF, AUD, JPY
  // Using uppercase currency codes as returned by the API.
  String currencyUpper = currency;
  currencyUpper.toUpperCase();

  // Fetch BTC price from mempool.space — same server as block height fetch
  bool priceOk = false;
  http.begin("https://mempool.space/api/v1/prices");
  http.setTimeout(8000);

  if (http.GET() == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      long price = doc[currencyUpper] | 0L;
      if (price > 0) {
        bitcoinData.price = String(price);
        priceOk = true;
      } else {
        Serial.println("[BTC] Currency '" + currencyUpper + "' not in mempool.space prices — keeping last value");
      }
    }
  }
  http.end();

  delay(200); // SSL cleanup delay

  // Second guard: config mode might have been triggered while the first
  // HTTP call was running. Don't start a new SSL connection if WiFi is gone.
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
    Serial.println("[BTC] Aborting mid-fetch - CONFIG_MODE active");
    return;
  }

  // Fetch block height from mempool.space — keep last known value on failure
  bool blockOk = false;
  http.begin("https://mempool.space/api/blocks/tip/height");
  http.setTimeout(8000);

  if (http.GET() == 200) {
    String val = http.getString();
    val.trim();
    if (val.length() > 0) {
      bitcoinData.blockHigh = val;
      blockOk = true;
    }
  }
  http.end();

  // If price failed but block succeeded: retry price once. The block fetch just established
  // a connection to mempool.space so the router's connection-tracking entry is now warm —
  // the retry usually succeeds immediately where the first cold attempt was dropped.
  if (!priceOk && blockOk && !deviceState.isInState(DeviceState::CONFIG_MODE)) {
    delay(300);
    Serial.println("[BTC] Price failed, block OK — retrying price (connection now warm)...");
    http.begin("https://mempool.space/api/v1/prices");
    http.setTimeout(8000);
    if (http.GET() == 200) {
      JsonDocument doc2;
      if (!deserializeJson(doc2, http.getString())) {
        long price = doc2[currencyUpper] | 0L;
        if (price > 0) { bitcoinData.price = String(price); priceOk = true; }
      }
    }
    http.end();
    if (priceOk) Serial.println("[BTC] Price retry succeeded: " + bitcoinData.price);
    else         Serial.println("[BTC] Price retry also failed — keeping last value: " + bitcoinData.price);
  }

  Serial.println("[BTC] Source: mempool.space");
  if (!priceOk)  Serial.println("[BTC] Price fetch failed — keeping last value: " + bitcoinData.price);
  if (!blockOk)  Serial.println("[BTC] Block fetch failed — keeping last value: " + bitcoinData.blockHigh);
  Serial.println("[BTC] Price: " + bitcoinData.price + " " + currency);
  Serial.println("[BTC] Block height: " + bitcoinData.blockHigh);

  // Retry sooner if either request failed; stale display values are preserved
  btcDataHasError = (!priceOk || !blockOk);
  if (btcDataHasError) {
    Serial.println("[BTC] ERROR detected - will retry in 1 minute instead of 5 minutes");
  }
  
  bitcoinData.lastUpdate = millis();
}

/**
 * Periodically update Bitcoin ticker display.
 */
void updateBitcoinTicker()
{
  // Only update if ticker is active and not in error/config/help modes
  if (!multiChannelConfig.btcTickerActive || deviceState.isInState(DeviceState::ERROR_RECOVERABLE) || deviceState.isInState(DeviceState::CONFIG_MODE) || deviceState.isInState(DeviceState::HELP_SCREEN)) {
    return;
  }

  unsigned long currentTime = millis();

  // Use shorter interval if last fetch had errors, otherwise use normal interval
  unsigned long updateInterval = btcDataHasError ? BTC_ERROR_RETRY_INTERVAL : BTC_UPDATE_INTERVAL;

  // Check if it's time for an update and enforce backoff for failed attempts
  if (currentTime - bitcoinData.lastUpdate >= updateInterval) {
    // Enforce 30-second backoff between failed BTC fetch attempts
    if ((currentTime - lastFetchAttempt) < RETRY_BACKOFF) {
      return; // Too soon - skip this attempt
    }
    
    Serial.println("[BTC] Update interval reached, fetching new data...");
    fetchBitcoinData();

    // Refresh the display ONLY if we're STILL on the ticker screen
    // Use partial update to reduce flicker (only updates values, not full redraw)
    if (multiChannelConfig.btcTickerActive && !deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && !deviceState.isInState(DeviceState::PRODUCT_SELECTION)) {
      updateBtctickerValues(); // Partial update instead of btctickerScreen()
      Serial.println("[BTC] Values updated (partial refresh - reduced flicker)");
    }
  }
}
#endif // ENABLE_BITCOIN_DATA

/**
 * Periodically update switch labels from server.
 */
void updateSwitchLabels()
{
  // Skip if in error/config/help modes
  if (deviceState.isInState(DeviceState::ERROR_RECOVERABLE) || deviceState.isInState(DeviceState::CONFIG_MODE) || deviceState.isInState(DeviceState::HELP_SCREEN)) {
    return;
  }

  unsigned long currentTime = millis();

  // Check if labels failed to load initially or if it's time for periodic update
  if (!labelsLoadedSuccessfully || (currentTime - productLabels.lastUpdate >= LABEL_UPDATE_INTERVAL)) {
    // Enforce backoff delay between retry attempts to prevent SSL memory exhaustion
    if ((currentTime - lastFetchAttempt) < RETRY_BACKOFF) {
      return; // Too soon - skip this attempt
    }
    
    if (!labelsLoadedSuccessfully) {
      Serial.println("[LABELS] Labels not loaded successfully, retrying...");
    } else {
      Serial.println("[LABELS] Periodic update interval reached, fetching labels...");
    }
    fetchSwitchLabels();
  }
}

// ============================================================================
// MINI-POS API (Touch 3.5 — amount entry → invoice via zapbox_extension)
// ============================================================================

extern void updateLightningQR(const String& lnurlStr);

/**
 * POST /<apiPath>/api/v1/pos/invoice
 * Header: X-Api-Key: <wallet invoice key>
 * Body:   {"amount": 5.00, "currency": "EUR", "device_id": "<22-char-id>"}
 * Response: {"payment_hash": "...", "payment_request": "lnbc..."}
 */
bool requestMiniPosInvoice(const String &amountStr)
{
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    miniPosState.infoMsg = "No server configured";
    return false;
  }
  if (miniPosConfig.invoiceKey.length() == 0) {
    miniPosState.infoMsg = "No invoice key";
    return false;
  }

  HTTPClient http;
  String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
               + "/api/v1/pos/invoice";
  LOG_INFO("MiniPoS", "Requesting invoice: " + amountStr + " " + miniPosConfig.currency);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", miniPosConfig.invoiceKey);
  http.setConnectTimeout(5000);
  http.setTimeout(10000);

  String body = String("{\"amount\":") + amountStr
              + ",\"currency\":\"" + miniPosConfig.currency + "\""
              + ",\"device_id\":\"" + deviceId + "\"}";
  int httpCode = http.POST(body);
  if (httpCode != 200) {
    String resp = http.getString();
    http.end();
    LOG_ERROR("MiniPoS", String("Invoice HTTP ") + String(httpCode) + " - " + resp);
    miniPosState.infoMsg = (httpCode < 0) ? String("Connection failed")
                                          : "Server error " + String(httpCode);
    return false;
  }

  String resp = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    LOG_ERROR("MiniPoS", "Invoice response JSON parse error");
    miniPosState.infoMsg = "Bad response";
    return false;
  }
  const char *hash = doc["payment_hash"];
  const char *bolt11 = doc["payment_request"];
  if (!hash || !bolt11 || strlen(bolt11) == 0) {
    LOG_ERROR("MiniPoS", "Invoice response missing fields");
    miniPosState.infoMsg = "Bad response";
    return false;
  }

  LOG_INFO("MiniPoS", String("BOLT11 length: ") + String(strlen(bolt11)) + " chars");
  miniPosState.paymentHash = String(hash);
  miniPosState.amountLine = amountStr + " " + miniPosConfig.currency;
  miniPosState.invoicePending = true;
  miniPosState.invoiceCreatedAt = millis();
  // BOLT11 into the QR/NFC buffer ("lightning:" prefix added automatically)
  updateLightningQR(String(bolt11));
  LOG_INFO("MiniPoS", "Invoice created: " + miniPosState.paymentHash);
  return true;
}

/**
 * GET /<apiPath>/api/v1/pos/invoice/last?device_id=<id>
 * Header: X-Api-Key: <wallet invoice key>
 * Response: {"amount": 23.5, "currency": "EUR"} or {"amount": null}
 */
bool fetchMiniPosLastPay(String &amountOut)
{
  amountOut = "";
  if (lnbitsServer.length() == 0 || deviceId.length() == 0 ||
      miniPosConfig.invoiceKey.length() == 0) {
    return false;
  }

  HTTPClient http;
  String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath
               + "/api/v1/pos/invoice/last?device_id=" + deviceId;
  http.begin(url);
  http.addHeader("X-Api-Key", miniPosConfig.invoiceKey);
  http.setConnectTimeout(5000);
  http.setTimeout(7000);

  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    LOG_WARN("MiniPoS", String("Last-pay HTTP ") + String(httpCode));
    return false;
  }
  String resp = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  if (doc["amount"].isNull()) return false;

  float amount = doc["amount"];
  if (miniPosConfig.decimal) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", amount);
    amountOut = String(buf);
  } else {
    amountOut = String((long)amount);
  }
  // Keep within the 7-char entry limit
  if (amountOut.length() > 7) amountOut = amountOut.substring(0, 7);
  LOG_INFO("MiniPoS", "Last paid amount: " + amountOut);
  return true;
}
