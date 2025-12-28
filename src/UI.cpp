#include "UI.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Display.h"
#include "Payment.h"
#include <Arduino.h>

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

// Configuration constant (1 second grace period after wake-up)
extern const unsigned long GRACE_PERIOD_MS;

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
    } else if (multiChannelConfig.currentProduct == 0) {
      // Bitcoin ticker screen (only if ticker mode allows it)
      if (multiChannelConfig.btcTickerMode == "off") {
        // Should not show ticker if OFF, show product selection instead
        multiChannelConfig.currentProduct = -1;
        productSelectionScreen();
        deviceState.transition(DeviceState::PRODUCT_SELECTION);
        multiChannelConfig.btcTickerActive = false;
        Serial.println("[DISPLAY] BTC-Ticker OFF - Showing product selection screen");
      } else {
        // Show ticker for "always" or "selecting" modes
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        Serial.println("[DISPLAY] Bitcoin ticker screen displayed");
      }
    } else {
      String label = "";
      int displayPin = 0;
      
      switch(multiChannelConfig.currentProduct) {
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
      
      // Generate LNURL dynamically for current product's pin
      String lnurlStr = generateLNURL(displayPin);
      updateLightningQR(lnurlStr);
      showProductQRScreen(label, displayPin);
      multiChannelConfig.btcTickerActive = false;
      Serial.printf("[DISPLAY] Product %d QR screen displayed\n", multiChannelConfig.currentProduct);
    }
    deviceState.transition(DeviceState::READY);
    return;
  }
  
  // Special mode
  if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
    // Generate LNURL for pin 12 before showing special mode QR
    String lnurlStr = generateLNURL(12);
    updateLightningQR(lnurlStr);
    showSpecialModeQRScreen();
    Serial.println("[DISPLAY] Special mode QR screen displayed");
    return;
  }
  
  // Standard mode
  String lnurlStr = generateLNURL(12);
  updateLightningQR(lnurlStr);
  showQRScreen();
  Serial.println("[DISPLAY] Normal QR screen displayed");
}
