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
const unsigned long LABEL_UPDATE_INTERVAL = 300000; // 5 minutes

// External function declarations from main.cpp
extern void btctickerScreen();

/**
 * Fetch switch labels and configuration from LNbits server.
 */
void fetchSwitchLabels()
{
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    Serial.println("[LABELS] Cannot fetch labels - server or deviceId not configured");
    return;
  }

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
      productLabels.label12 = "";
      productLabels.label13 = "";
      productLabels.label10 = "";
      productLabels.label11 = "";
      
      // Extract labels from switches array
      JsonArray switches = doc["switches"];
      for (JsonObject switchObj : switches) {
        int pin = switchObj["pin"];
        const char* labelChar = switchObj["label"];
        String labelStr = (labelChar != nullptr) ? String(labelChar) : "";
        
        // Store label based on pin number
        if (pin == 12) {
          productLabels.label12 = labelStr;
          Serial.println("[LABELS] Pin 12 label: " + productLabels.label12);
        } else if (pin == 13) {
          productLabels.label13 = labelStr;
          Serial.println("[LABELS] Pin 13 label: " + productLabels.label13);
        } else if (pin == 10) {
          productLabels.label10 = labelStr;
          Serial.println("[LABELS] Pin 10 label: " + productLabels.label10);
        } else if (pin == 11) {
          productLabels.label11 = labelStr;
          Serial.println("[LABELS] Pin 11 label: " + productLabels.label11);
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
 * Fetch Bitcoin price and block height from external APIs.
 */
void fetchBitcoinData()
{
  Serial.println("[BTC] Fetching Bitcoin data...");
  HTTPClient http;

  // Fetch BTC price from CoinGecko using configured currency
  String currencyLower = currency;
  currencyLower.toLowerCase(); // CoinGecko expects lowercase currency code
  String apiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + currencyLower;
  
  Serial.println("[BTC] Current currency variable: '" + currency + "'");
  Serial.println("[BTC] Requesting price from CoinGecko with URL: " + apiUrl);
  http.begin(apiUrl);
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("[BTC] CoinGecko response: " + payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["bitcoin"].is<JsonObject>()) {
      Serial.println("[BTC] Looking for price key: '" + currencyLower + "'");
      float price = doc["bitcoin"][currencyLower];
      bitcoinData.price = String((int)price); // Convert to integer string
      Serial.println("[BTC] Price updated: " + bitcoinData.price + " " + currency);
    } else {
      Serial.println("[BTC] Failed to parse CoinGecko JSON");
      bitcoinData.price = "Error";
    }
  } else {
    Serial.printf("[BTC] CoinGecko request failed: %d\n", httpCode);
    bitcoinData.price = "Error";
  }
  http.end();

  delay(100); // Small delay between requests

  // Fetch block height from mempool.space
  Serial.println("[BTC] Requesting block height from mempool.space...");
  http.begin("https://mempool.space/api/blocks/tip/height");
  http.setTimeout(5000);

  httpCode = http.GET();
  if (httpCode == 200) {
    bitcoinData.blockHigh = http.getString();
    bitcoinData.blockHigh.trim();
    Serial.println("[BTC] Block height updated: " + bitcoinData.blockHigh);
  } else {
    Serial.printf("[BTC] Mempool.space request failed: %d\n", httpCode);
    bitcoinData.blockHigh = "Error";
  }
  http.end();

  Serial.println("[BTC] Bitcoin data fetch completed");
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

  // Check if it's time for an update
  if (currentTime - bitcoinData.lastUpdate >= BTC_UPDATE_INTERVAL) {
    Serial.println("[BTC] Update interval reached, fetching new data...");
    fetchBitcoinData();

    // Refresh the display ONLY if we're STILL on the ticker screen
    // Double-check multiChannelConfig.btcTickerActive immediately before drawing to prevent race conditions
    if (multiChannelConfig.btcTickerActive && !deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && !deviceState.isInState(DeviceState::PRODUCT_SELECTION)) {
      // Final check right before screen update to prevent concurrent drawing
      if (multiChannelConfig.btcTickerActive) {
        btctickerScreen();
        Serial.println("[BTC] Screen refreshed with new data");
      }
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
    if (!labelsLoadedSuccessfully) {
      Serial.println("[LABELS] Labels not loaded successfully, retrying...");
    } else {
      Serial.println("[LABELS] Periodic update interval reached, fetching labels...");
    }
    fetchSwitchLabels();
  }
}
