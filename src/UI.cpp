#include "UI.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Display.h"
#include "Payment.h"
#include <Arduino.h>
#include <WiFi.h>

// External references to main.cpp
extern StateManager deviceState;
extern PowerConfig powerConfig;
extern ActivityTracking activityTracking;
extern bool initializationActive;
extern bool readyLedState;
extern LightningConfig lightningConfig;
extern MultiChannelConfig multiChannelConfig;
extern SpecialModeConfig specialModeConfig;
extern ProductLabels productLabels;
extern bool needsQRRedraw;

// External function declarations from main.cpp
extern void showQRScreen();
extern void showProductQRScreen(String label, int displayPin);
extern void showThresholdQRScreen();
extern void btctickerScreen();
extern void productSelectionScreen();
extern void showSpecialModeQRScreen();
extern void deactivateScreensaver();
extern void activateScreensaver(String mode);
extern void prepareDeepSleep();
extern void setupDeepSleepWakeup(String mode);
extern void bootUpScreen();

// Configuration constant (1 second grace period after wake-up)
const unsigned long GRACE_PERIOD_MS = 1000;

/**
 * Wakes the device from power saving mode (screensaver or deep sleep).
 * Implements grace period to prevent multiple rapid wake-ups.
 * @return true if device was in grace period or just woke up, false if in normal operation
 */
bool wakeFromPowerSavingMode() {
  // Check if we're in grace period after wake-up
  if (powerConfig.lastWakeUpTime > 0 && (millis() - powerConfig.lastWakeUpTime) < GRACE_PERIOD_MS) {
    Serial.println("[WAKE] Ignored - in grace period after wake-up");
    return true; // Indicate we're in grace period
  }
  
  // Clear wake-up timestamp once grace period has passed - allows subsequent touches to navigate normally
  if (powerConfig.lastWakeUpTime > 0) {
    Serial.println("[WAKE] Grace period expired, resuming normal operation");
    powerConfig.lastWakeUpTime = 0;
  }
  
  // Reset activity timer
  activityTracking.lastActivityTime = millis();
  
  // If screensaver or deep sleep was active, deactivate and return true
  if (deviceState.isInState(DeviceState::SCREENSAVER) || deviceState.isInState(DeviceState::DEEP_SLEEP)) {
    Serial.println("[WAKE] Waking from power saving mode");
    deviceState.transition(DeviceState::READY);
    deactivateScreensaver();
    powerConfig.lastWakeUpTime = millis();
    activityTracking.lastActivityTime = millis();
    return true; // Indicate we just woke up
  }
  
  return false; // Normal operation, no wake-up needed
}

/**
 * Determines if the device is ready to receive payments.
 * @return true if device is in a state where it can receive payments
 */
bool isReadyForReceive() {
  // LED ON when device is past init, not in error/config/help/report, and not in deep sleep
  return deviceState.getState() != DeviceState::INITIALIZING && 
         !initializationActive && 
         !deviceState.isInState(DeviceState::ERROR_RECOVERABLE) && 
         !deviceState.isInState(DeviceState::CONFIG_MODE) && 
         !deviceState.isInState(DeviceState::HELP_SCREEN) && 
         !deviceState.isInState(DeviceState::REPORT_SCREEN) && 
         !deviceState.isInState(DeviceState::DEEP_SLEEP);
}

/**
 * Updates the ready LED based on device state.
 * Only updates LED if state has changed to avoid redundant writes.
 */
void updateReadyLed() {
  bool shouldBeOn = isReadyForReceive();
  if (shouldBeOn != readyLedState) {
    digitalWrite(PIN_LED_BUTTON_LED, shouldBeOn ? HIGH : LOW); // Source 3.3V when ready
    readyLedState = shouldBeOn;
    Serial.printf("[LED] Ready LED %s\n", shouldBeOn ? "ON" : "OFF");
  }
}

/**
 * Redraws the QR screen based on current mode and configuration.
 * Handles threshold mode, multi-channel mode with ticker, and product selection.
 */
void redrawQRScreen() {
  Serial.println("[DISPLAY] Redrawing QR screen...");

  // Threshold mode
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    Serial.println("[DISPLAY] Threshold QR screen displayed");
    return;
  }

  // Multi-Channel-Control mode
  if (multiChannelConfig.mode != "off") {
    // Behavior depends on btcTickerMode and currentProduct
    if (multiChannelConfig.currentProduct == -1) {
      // Special value: product selection screen
      productSelectionScreen();
      deviceState.transition(DeviceState::PRODUCT_SELECTION);
      multiChannelConfig.btcTickerActive = false;
      Serial.println("[DISPLAY] Product selection screen displayed");
      deviceState.transition(DeviceState::READY);
      return;
    } else if (multiChannelConfig.currentProduct == 0) {
      // Bitcoin ticker screen (only if ticker mode allows it)
      if (multiChannelConfig.btcTickerMode == "off") {
        // Should not show ticker if OFF, show product selection instead
        multiChannelConfig.currentProduct = -1;
        productSelectionScreen();
        deviceState.transition(DeviceState::PRODUCT_SELECTION);
        multiChannelConfig.btcTickerActive = false;
        Serial.println("[DISPLAY] BTC-Ticker OFF - Showing product selection screen");
        deviceState.transition(DeviceState::READY);
        return;
      } else {
        // Show ticker for "always" or "selecting" modes
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        Serial.println("[DISPLAY] Bitcoin ticker screen displayed");
        deviceState.transition(DeviceState::READY);
        return;
      }
    } else {
      // Show product QR for selected product (1..4)
      String label = "";
      int displayPin = 0;

      switch (multiChannelConfig.currentProduct) {
        case 1:
          label = (productLabels.label12.length() > 0) ? productLabels.label12 : "Pin 12";
          displayPin = 12;
          break;
        case 2:
          label = (productLabels.label13.length() > 0) ? productLabels.label13 : "Pin 13";
          displayPin = 13;
          break;
        case 3:
          label = (productLabels.label10.length() > 0) ? productLabels.label10 : "Pin 10";
          displayPin = 10;
          break;
        case 4:
          label = (productLabels.label11.length() > 0) ? productLabels.label11 : "Pin 11";
          displayPin = 11;
          break;
      }

      String lnurlStr = generateLNURL(displayPin);
      updateLightningQR(lnurlStr);
      showProductQRScreen(label, displayPin);
      multiChannelConfig.btcTickerActive = false;
      Serial.printf("[DISPLAY] Product %d QR screen displayed\n", multiChannelConfig.currentProduct);
      deviceState.transition(DeviceState::READY);
      return;
    }
  }

  // Single mode (1-channel)
  if (specialModeConfig.mode != "standard") {
    // SPECIAL MODE: ensure LNURL for pin 12 is up-to-date, then show special QR
    String lnurlStr = generateLNURL(12);
    updateLightningQR(lnurlStr);
    showSpecialModeQRScreen();
    multiChannelConfig.btcTickerActive = false;
    Serial.println("[DISPLAY] Special mode QR screen displayed (single mode)");
    deviceState.transition(DeviceState::READY);
    return;
  }

  if (multiChannelConfig.btcTickerMode == "always") {
    // ALWAYS: show BTC ticker
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
    Serial.println("[DISPLAY] Bitcoin ticker screen displayed (single mode, ALWAYS)");
    deviceState.transition(DeviceState::READY);
    return;
  }

  // OFF or SELECTING: show QR unless ticker is currently active
  if (multiChannelConfig.btcTickerActive) {
    btctickerScreen();
    Serial.println("[DISPLAY] Bitcoin ticker screen refreshed (single mode, SELECTING active)");
    deviceState.transition(DeviceState::READY);
    return;
  } else {
    String lnurlStr = generateLNURL(12);
    updateLightningQR(lnurlStr);
    showQRScreen();
    Serial.println("[DISPLAY] QR screen displayed (single mode)");
    deviceState.transition(DeviceState::READY);
    return;
  }
}

/**
 * Shows the appropriate initial screen once all connections are confirmed.
 * Handles threshold mode, multi-channel (duo/quattro) and single mode including special mode.
 */
void showInitialScreenAfterConnections() {
  // Threshold mode has priority
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Single mode
  if (multiChannelConfig.mode == "off") {
    if (multiChannelConfig.btcTickerMode == "always") {
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis();
    } else {
      // SELECTING or OFF: show normal/special QR
      String lnurlStr = generateLNURL(12);
      updateLightningQR(lnurlStr);
      if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
        showSpecialModeQRScreen();
      } else {
        showQRScreen();
      }
      multiChannelConfig.btcTickerActive = false;
      productSelectionState.showTime = 0;
    }
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Multi-Channel-Control (duo/quattro)
  if (multiChannelConfig.btcTickerMode == "off") {
    multiChannelConfig.currentProduct = -1; // product selection
    productSelectionScreen();
    deviceState.transition(DeviceState::PRODUCT_SELECTION);
    multiChannelConfig.btcTickerActive = false;
    productSelectionState.showTime = millis();
  } else if (multiChannelConfig.btcTickerMode == "always") {
    multiChannelConfig.currentProduct = 0; // ticker
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
    deviceState.transition(DeviceState::READY);
    productSelectionState.showTime = millis();
  } else if (multiChannelConfig.btcTickerMode == "selecting") {
    multiChannelConfig.currentProduct = -1; // product selection
    productSelectionScreen();
    deviceState.transition(DeviceState::PRODUCT_SELECTION);
    multiChannelConfig.btcTickerActive = false;
    productSelectionState.showTime = millis();
  }
}

/**
 * Handles power saving activation (screensaver or deep sleep).
 * Contains the same logic previously embedded in the main loop.
 */
void handlePowerSavingChecks() {
  // Screensaver mode activation
  if (!deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.screensaver != "off" && powerConfig.deepSleep == "off") {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - activityTracking.lastActivityTime;

    // Debug output every 10 seconds
    static unsigned long lastDebugOutput = 0;
    if (currentTime - lastDebugOutput > 10000) {
      Serial.printf("[SCREENSAVER_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                    elapsedTime, powerConfig.activationTimeoutMs, (elapsedTime * 100.0 / powerConfig.activationTimeoutMs));
      lastDebugOutput = currentTime;
    }

    if (elapsedTime >= powerConfig.activationTimeoutMs) {
      Serial.println("[TIMEOUT] Screensaver timeout reached, activating powerConfig.screensaver");
      deviceState.transition(DeviceState::SCREENSAVER);
      activateScreensaver(powerConfig.screensaver);
      // Continue with payment loop - screensaver only turns off backlight
    }
  }

  // Deep sleep activation
  if (!deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.deepSleep != "off" && powerConfig.screensaver == "off") {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - activityTracking.lastActivityTime;

    // Debug output every 10 seconds
    static unsigned long lastDebugOutputDeep = 0;
    if (currentTime - lastDebugOutputDeep > 10000) {
      Serial.printf("[DEEP_SLEEP_CHECK] Elapsed: %lu ms / Timeout: %lu ms (%.1f%%)\n", 
                    elapsedTime, powerConfig.activationTimeoutMs, (elapsedTime * 100.0 / powerConfig.activationTimeoutMs));
      lastDebugOutputDeep = currentTime;
    }

    if (elapsedTime >= powerConfig.activationTimeoutMs) {
      Serial.println("[TIMEOUT] Deep sleep timeout reached, preparing for deep sleep");
      deviceState.transition(DeviceState::DEEP_SLEEP);

      // Flush serial output before sleep
      Serial.flush();

      // Prepare display for sleep
      prepareDeepSleep();

      // Give more time for display operations to complete
      vTaskDelay(pdMS_TO_TICKS(1000));

      // Final serial flush
      Serial.println("[DEEP_SLEEP] Entering sleep mode now...");
      Serial.flush();

      // Enter deep sleep (will not return in freeze mode)
      setupDeepSleepWakeup(powerConfig.deepSleep);

      // Execution continues here after wake-up from light sleep
      // (Deep sleep/freeze mode will restart the device instead)

      // CRITICAL: Light sleep disconnects USB-Serial hardware
      // We need to reinitialize USB-CDC to make Serial work again
      Serial.println("[WAKE_UP] Device woke from light sleep");

      // Reinitialize USB-CDC peripheral after light sleep
      Serial.end();
      delay(100);
      Serial.setRxBufferSize(2048); // Same as in setup()
      Serial.begin(115200);
      delay(200); // Give USB-CDC time to enumerate

      Serial.println("[WAKE_UP] USB-Serial reinitialized after light sleep");
      Serial.flush();

      // Show boot-up screen first
      bootUpScreen();
      Serial.println("[WAKE_UP] Boot screen displayed");

      // Turn backlight back on
      pinMode(PIN_LCD_BL, OUTPUT);
      digitalWrite(PIN_LCD_BL, HIGH);
      Serial.println("[WAKE_UP] Backlight restored");

      // Check WiFi connection status
      Serial.println("[WAKE_UP] Checking WiFi connection...");
      int wifiCheckCount = 0;
      while (WiFi.status() != WL_CONNECTED && wifiCheckCount < 30) {
        delay(100);
        wifiCheckCount++;
        if (wifiCheckCount % 10 == 0) {
          Serial.printf("[WAKE_UP] Waiting for WiFi... (%d/30)\n", wifiCheckCount);
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WAKE_UP] WiFi connected");
      } else {
        Serial.println("[WAKE_UP] WiFi not connected after wake-up, will retry in loop");
        // WiFi reconnection will be handled by checkAndReconnectWiFi() in main loop
      }

      // Reset activity time and clear sleep flag
      activityTracking.lastActivityTime = millis();
      powerConfig.lastWakeUpTime = millis();
      deviceState.transition(DeviceState::READY);

      // Small delay before redrawing screen
      delay(500);

      // Redraw the appropriate QR screen
      redrawQRScreen();
      Serial.println("[WAKE_UP] Ready for payments");
    }
  }
}
