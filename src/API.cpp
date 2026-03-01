#include "API.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Log.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    Serial.println("[LABELS] Cannot fetch labels - server or deviceId not configured");
    return;
  }

  // Update last attempt time to prevent rapid retries
  lastFetchAttempt = millis();

  HTTPClient http;
  // Try primary extension path first (default: bitcoinswitch).
  // On 404, auto-detect by trying the other extension path (zapbox).
  String url = "https://" + lnbitsServer + "/" + extensionConfig.apiPath + "/api/v1/public/" + deviceId;
  Serial.println("[LABELS] Fetching switch configurations from: " + url);
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();

  // Auto-detect extension path: if primary returns 404, try the other extension
  if (httpCode == 404) {
    String fallbackPath = (extensionConfig.apiPath == "bitcoinswitch") ? "zapbox" : "bitcoinswitch";
    Serial.println("[LABELS] " + extensionConfig.apiPath + " returned 404 - trying fallback: " + fallbackPath);
    http.end();
    url = "https://" + lnbitsServer + "/" + fallbackPath + "/api/v1/public/" + deviceId;
    Serial.println("[LABELS] Fetching switch configurations from: " + url);
    http.begin(url);
    http.setTimeout(5000);
    httpCode = http.GET();
    if (httpCode == 200) {
      // Fallback succeeded - remember this path for all future requests (including Payment.cpp)
      extensionConfig.apiPath = fallbackPath;
      Serial.println("[LABELS] Auto-detected extension: /" + fallbackPath + "/api/v1/ (saved for payments)");
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
      
      // Clear existing labels (12 channels: 4 shared + 8 headless-only)
      for (int i = 0; i < 12; i++) {
        productLabels.labels[i] = "";
      }
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        
        // Store label based on pin number using array index (0-11)
        int pinIndex = getPinIndex(pin);
        if (pinIndex >= 0 && pinIndex < 12) {
          productLabels.labels[pinIndex] = labelStr;
          Serial.println("[LABELS] Pin " + String(pin) + " label: " + labelStr);
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
      // Always fetch Bitcoin data with the correct currency (not just when ticker is active)
      // This ensures data is ready when ticker is activated
      Serial.println("[LABELS] Currency received - fetching Bitcoin data with correct currency");
      fetchBitcoinData();
      
      // If ticker is currently active, redraw screen to show new currency
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
      Serial.println("[LABELS] Connection error - could not reach server");
    } else if (httpCode >= 500) {
      Serial.println("[LABELS] Server error - may be temporary, will retry");
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
  String currencyLower = currency;
  currencyLower.toLowerCase();
  
  // Fetch BTC price from CoinGecko
  bitcoinData.price = "Error";
  String apiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + currencyLower;
  http.begin(apiUrl);
  http.setTimeout(5000);
  
  if (http.GET() == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString()) && doc["bitcoin"].is<JsonObject>()) {
      float price = doc["bitcoin"][currencyLower];
      bitcoinData.price = String((int)price);
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
  
  // Fetch block height from Mempool
  bitcoinData.blockHigh = "Error";
  http.begin("https://mempool.space/api/blocks/tip/height");
  http.setTimeout(5000);
  
  if (http.GET() == 200) {
    bitcoinData.blockHigh = http.getString();
    bitcoinData.blockHigh.trim();
  }
  http.end();
  
  Serial.println("[BTC] Price: " + bitcoinData.price + " " + currency);
  Serial.println("[BTC] Block height: " + bitcoinData.blockHigh);
  
  // Check if any data failed to load
  btcDataHasError = (bitcoinData.price == "Error" || bitcoinData.blockHigh == "Error");
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
