#include "API.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// External references to main.cpp
extern StateManager deviceState;
extern MultiChannelConfig multiChannelConfig;
extern ProductLabels productLabels;
extern BitcoinData bitcoinData;
extern String lnbitsServer;
extern String deviceId;
extern String currency;
extern bool labelsLoadedSuccessfully;

// External constants from main.cpp
const unsigned long BTC_UPDATE_INTERVAL = 300000; // 5 minutes
const unsigned long BTC_ERROR_RETRY_INTERVAL = 60000; // 1 minute retry for errors
const unsigned long LABEL_UPDATE_INTERVAL = 300000; // 5 minutes

// Retry backoff for failed label & BTC fetches
static unsigned long lastFetchAttempt = 0;
static const unsigned long RETRY_BACKOFF = 30000; // 30 seconds between retries
static bool btcDataHasError = false; // Track if last BTC fetch had errors

// External function declarations from main.cpp
extern void btctickerScreen();
extern void updateBtctickerValues(); // Partial update function

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
      // Extract currency from response
      const char* currencyChar = doc["currency"];
      Serial.print("[LABELS] DEBUG - currency field from API: ");
      if (currencyChar != nullptr) {
        Serial.println(String(currencyChar));
        String oldCurrency = currency;
        currency = String(currencyChar);
        currency.toUpperCase(); // Ensure uppercase for display and API calls
        Serial.println("[LABELS] Currency changed from '" + oldCurrency + "' to '" + currency + "'");
      } else {
        Serial.println("NULL - field not found in response");
        // Keep currency from config (don't override with USD)
        Serial.println("[LABELS] No currency in API response, keeping config value: " + currency);
      }
      
      // Clear existing labels
      for (int i = 0; i < 4; i++) {
        productLabels.labels[i] = "";
      }
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        
        // Store label based on pin number using array index
        int pinIndex = getPinIndex(pin);
        if (pinIndex >= 0 && pinIndex < 4) {
          productLabels.labels[pinIndex] = labelStr;
          Serial.println("[LABELS] Pin " + String(pin) + " label: " + labelStr);
        }
      }
      
      Serial.println("[LABELS] Successfully fetched and cached all labels");
      labelsLoadedSuccessfully = true; // Mark labels as successfully loaded
      productLabels.lastUpdate = millis(); // Update timestamp
      
      // Always fetch Bitcoin data with the correct currency (not just when ticker is active)
      // This ensures data is ready when ticker is activated
      Serial.println("[LABELS] Currency received - fetching Bitcoin data with correct currency");
      fetchBitcoinData();
      
      // If ticker is currently active, redraw screen to show new currency
      if (multiChannelConfig.btcTickerActive) {
        Serial.println("[LABELS] Ticker active - refreshing display");
        btctickerScreen();
      }
    } else {
      Serial.println("[LABELS] JSON parsing failed: " + String(error.c_str()));
    }
  } else {
    Serial.printf("[LABELS] HTTP request failed with code: %d\n", httpCode);
  }
  
  http.end();
}

/**
 * Fetch Bitcoin price and block height from external APIs (sequential).
 */
void fetchBitcoinData()
{
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
