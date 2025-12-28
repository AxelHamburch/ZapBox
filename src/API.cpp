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

// Retry backoff for failed label fetches
static unsigned long lastLabelFetchAttempt = 0;
static const unsigned long LABEL_RETRY_BACKOFF = 30000; // 30 seconds between retries

// Retry backoff for failed BTC fetches
static unsigned long lastBtcFetchAttempt = 0;
static const unsigned long BTC_RETRY_BACKOFF = 30000; // 30 seconds between retries

// External function declarations from main.cpp
extern void btctickerScreen();

// Task handles for parallel requests
static TaskHandle_t coinGeckoTaskHandle = NULL;
static TaskHandle_t mempoolTaskHandle = NULL;

// Temporary storage for parallel BTC data fetch
static struct {
  String price;
  String blockHeight;
  bool priceReady = false;
  bool blockHeightReady = false;
} parallelBtcData;

/**
 * Task: Fetch BTC price from CoinGecko (runs in parallel)
 */
void fetchCoinGeckoTask(void* parameter) {
  HTTPClient http;
  String currencyLower = currency;
  currencyLower.toLowerCase();
  String apiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=" + currencyLower;
  
  http.begin(apiUrl);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc["bitcoin"].is<JsonObject>()) {
      float price = doc["bitcoin"][currencyLower];
      parallelBtcData.price = String((int)price);
    } else {
      parallelBtcData.price = "Error";
    }
  } else {
    parallelBtcData.price = "Error";
    Serial.println("[BTC] CoinGecko error: " + String(httpCode));
  }
  http.end();
  
  // Aggressive cleanup for SSL context
  delay(100);
  
  parallelBtcData.priceReady = true;
  vTaskDelete(NULL);
}

/**
 * Task: Fetch block height from Mempool.space (runs in parallel)
 */
void fetchMempoolTask(void* parameter) {
  HTTPClient http;
  http.begin("https://mempool.space/api/blocks/tip/height");
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    parallelBtcData.blockHeight = http.getString();
    parallelBtcData.blockHeight.trim();
  } else {
    parallelBtcData.blockHeight = "Error";
    Serial.println("[BTC] Mempool error: " + String(httpCode));
  }
  http.end();
  
  // Aggressive cleanup for SSL context
  delay(100);
  
  parallelBtcData.blockHeightReady = true;
  vTaskDelete(NULL);
}

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
  lastLabelFetchAttempt = millis();

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
 * Fetch Bitcoin price and block height from external APIs (sequential to avoid SSL memory conflicts).
 */
void fetchBitcoinData()
{
  Serial.println("[BTC] Fetching Bitcoin data (sequential)...");
  
  // Reset parallel data flags
  parallelBtcData.priceReady = false;
  parallelBtcData.blockHeightReady = false;
  parallelBtcData.price = "Error";
  parallelBtcData.blockHeight = "Error";
  
  // Track fetch attempt for backoff
  lastBtcFetchAttempt = millis();
  
  // Start CoinGecko task on Core 0 (lower priority)
  // Sequential execution: Start first task, wait for completion before starting second
  xTaskCreatePinnedToCore(
    fetchCoinGeckoTask,
    "CoinGecko",
    12288,
    NULL,
    1,
    &coinGeckoTaskHandle,
    0
  );
  
  // Wait for CoinGecko task to complete with timeout (max 4 seconds)
  unsigned long startTime = millis();
  while ((millis() - startTime) < 4000) {
    if (parallelBtcData.priceReady) {
      break; // CoinGecko done
    }
    delay(100);
  }
  
  // Small delay to allow SSL context cleanup
  delay(200);
  
  // Start Mempool task on Core 0 (lower priority) - AFTER CoinGecko is done
  xTaskCreatePinnedToCore(
    fetchMempoolTask,
    "Mempool",
    12288,
    NULL,
    1,
    &mempoolTaskHandle,
    0
  );
  
  // Wait for Mempool task with timeout (max 4 seconds)
  startTime = millis();
  while ((millis() - startTime) < 4000) {
    if (parallelBtcData.blockHeightReady) {
      break; // Mempool done
    }
    delay(100);
  }
  
  // Get results (use values from tasks, or "Error" if timeout)
  bitcoinData.price = parallelBtcData.price;
  bitcoinData.blockHigh = parallelBtcData.blockHeight;
  
  Serial.println("[BTC] Price: " + bitcoinData.price + " " + currency);
  Serial.println("[BTC] Block height: " + bitcoinData.blockHigh);
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

  // Check if it's time for an update and enforce backoff for failed attempts
  if (currentTime - bitcoinData.lastUpdate >= BTC_UPDATE_INTERVAL) {
    // Enforce 30-second backoff between failed BTC fetch attempts
    if ((currentTime - lastBtcFetchAttempt) < BTC_RETRY_BACKOFF) {
      return; // Too soon - skip this attempt
    }
    
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
    // Enforce backoff delay between retry attempts to prevent SSL memory exhaustion
    if ((currentTime - lastLabelFetchAttempt) < LABEL_RETRY_BACKOFF) {
      // Too soon - skip this attempt
      return;
    }
    
    if (!labelsLoadedSuccessfully) {
      Serial.println("[LABELS] Labels not loaded successfully, retrying...");
    } else {
      Serial.println("[LABELS] Periodic update interval reached, fetching labels...");
    }
    fetchSwitchLabels();
  }
}
