#include "UI.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Display.h"
#include "Payment.h"
#include "Log.h"
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
extern NetworkStatus networkStatus;

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
    LOG_DEBUG("Wake", "Ignored - in grace period after wake-up");
    return true; // Indicate we're in grace period
  }
  
  // Clear wake-up timestamp once grace period has passed - allows subsequent touches to navigate normally
  if (powerConfig.lastWakeUpTime > 0) {
    LOG_DEBUG("Wake", "Grace period expired, resuming normal operation");
    powerConfig.lastWakeUpTime = 0;
  }
  
  // Reset activity timer
  activityTracking.lastActivityTime = millis();
  
  // If screensaver or deep sleep was active, deactivate and return true
  if (deviceState.isInState(DeviceState::SCREENSAVER) || deviceState.isInState(DeviceState::DEEP_SLEEP)) {
    LOG_INFO("Wake", "Waking from power saving mode");
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
 * For headless version: Fast blink during initialization, slow blink in config mode, solid when ready.
 * Error states (headless): 1-4 blinks indicating specific network errors.
 */
void updateReadyLed() {
#if !ENABLE_DISPLAY
  // Skip LED updates when in CONFIG_MODE to prevent race condition with config blink on other core
  if (deviceState.isInState(DeviceState::CONFIG_MODE)) return;

  // NFC payment pending blink: 200ms ON / 800ms OFF while waiting for invoice settlement.
  // As soon as the relay fires (paymentQueue.processing), two quick confirmation blinks
  // (100ms ON / 100ms OFF × 2) indicate the payment was accepted, then LED goes OFF for
  // the relay duration. After nfcPaymentPending clears, LED returns to steady ON.
  // On timeout / HTTP error (no payment): 3× fast blink (100ms ON/OFF) = "NO LUCK".
  #if ENABLE_NFC
  static bool wasNfcPending = false;
  static bool nfcConfirmStarted = false;
  static unsigned long nfcConfirmStartTime = 0;
  static bool nfcNoLuckActive = false;
  static unsigned long nfcNoLuckStartTime = 0;

  // NFC "NO LUCK" blink: 3× fast blink (100ms ON / 100ms OFF) after timeout/error,
  // then return to steady LED.
  if (nfcNoLuckActive) {
    unsigned long elapsed = millis() - nfcNoLuckStartTime;
    if (elapsed < 600) { // 3 blinks × 200ms (100 ON + 100 OFF) = 600ms
      bool on = ((elapsed / 100) % 2 == 0);
      static bool lastNoLuckState = true;
      if (on != lastNoLuckState) {
        lastNoLuckState = on;
        digitalWrite(PIN_LED_BUTTON_LED, on ? HIGH : LOW);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
        #endif
      }
      return;
    }
    // Blink sequence done → return to steady ON
    nfcNoLuckActive = false;
    digitalWrite(PIN_LED_BUTTON_LED, HIGH);
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, HIGH);
    #endif
    readyLedState = true;
    return;
  }

  if (extensionConfig.nfcPaymentPending) {
    wasNfcPending = true;

    if (paymentQueue.processing) {
      // Relay just started or is running – fire 2 quick blinks first (400ms), then LED off
      if (!nfcConfirmStarted) {
        nfcConfirmStarted = true;
        nfcConfirmStartTime = millis();
        // First edge: ON
        digitalWrite(PIN_LED_BUTTON_LED, HIGH);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, HIGH);
        #endif
      }
      unsigned long elapsed = millis() - nfcConfirmStartTime;
      if (elapsed < 400) {
        // 0-100ms: ON, 100-200ms: OFF, 200-300ms: ON, 300-400ms: OFF
        bool on = ((elapsed / 100) % 2 == 0);
        static bool lastConfirmState = true;
        if (on != lastConfirmState) {
          lastConfirmState = on;
          digitalWrite(PIN_LED_BUTTON_LED, on ? HIGH : LOW);
          #ifdef PIN_ONBOARD_LED
          digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
          #endif
        }
      } else {
        // Blink sequence done – LED ON for rest of relay duration (it's the READY LED)
        digitalWrite(PIN_LED_BUTTON_LED, HIGH);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, HIGH);
        #endif
      }
    } else {
      // Waiting for WS paid event: slow asymmetric blink 200ms ON / 800ms OFF
      static unsigned long lastNfcBlinkTime = 0;
      static bool nfcBlinkState = false;
      unsigned long interval = nfcBlinkState ? 200 : 800; // ON=200ms, OFF=800ms
      if (millis() - lastNfcBlinkTime > interval) {
        nfcBlinkState = !nfcBlinkState;
        digitalWrite(PIN_LED_BUTTON_LED, nfcBlinkState ? HIGH : LOW);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, nfcBlinkState ? HIGH : LOW);
        #endif
        lastNfcBlinkTime = millis();
      }
    }
    return;
  }
  // NFC pending cleared – reset confirm-blink state and force LED re-apply
  if (wasNfcPending) {
    wasNfcPending = false;
    if (!nfcConfirmStarted) {
      // No payment received → timeout or HTTP error → "NO LUCK" 3× fast blink
      nfcNoLuckActive = true;
      nfcNoLuckStartTime = millis();
      // Start first blink: ON
      digitalWrite(PIN_LED_BUTTON_LED, HIGH);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, HIGH);
      #endif
      nfcConfirmStarted = false;
      return;
    }
    nfcConfirmStarted = false;
    readyLedState = !isReadyForReceive(); // Force mismatch so digitalWrite fires below
  }
  #endif

  // Headless version: Fast blink during initialization to show progress
  static unsigned long lastInitBlinkTime = 0;
  static bool initBlinkState = false;
  
  // Fast blink during INITIALIZING or CONNECTING_WIFI (5Hz = 200ms period)
  // Also keep fast blink when initializationActive is true, even if the state
  // temporarily jumped to HELP_SCREEN / READY (e.g. button pressed mid-init).
  if (deviceState.isInState(DeviceState::INITIALIZING) || 
      deviceState.isInState(DeviceState::CONNECTING_WIFI) ||
      initializationActive) {
    if (millis() - lastInitBlinkTime > 200) { // Blink every 200ms (5Hz)
      initBlinkState = !initBlinkState;
      digitalWrite(PIN_LED_BUTTON_LED, initBlinkState ? HIGH : LOW);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, initBlinkState ? HIGH : LOW);
      #endif
      lastInitBlinkTime = millis();
    }
    return; // Exit early during initialization blink
  }
  
  // Error state blink patterns (headless only)
  // Check for network errors and show specific blink patterns:
  // - NO WIFI: 1 blink (1000ms on, 2000ms pause)
  // - NO INTERNET: 2 blinks (500ms on/off, 2000ms pause)
  // - NO SERVER: 3 blinks (300ms on/off, 2000ms pause)
  // - NO WEBSOCKET: 4 blinks (250ms on/off, 2000ms pause)
  static unsigned long lastErrorBlinkTime = 0;
  static int errorBlinkPhase = 0;
  static bool errorBlinkState = false;
  static bool wasInErrorState = false; // Track if we were in error state
  
  // Determine which error state we're in (priority order)
  int errorBlinkCount = 0;
  int blinkDuration = 0;
  int pauseDuration = 2000; // 2 seconds pause between sequences
  
  if (!networkStatus.confirmed.wifi) {
    errorBlinkCount = 1;
    blinkDuration = 500; // 500ms per blink (on or off)
  } else if (!networkStatus.confirmed.internet) {
    errorBlinkCount = 2;
    blinkDuration = 300; // 300ms per blink (on or off)
  } else if (!networkStatus.confirmed.server) {
    errorBlinkCount = 3;
    blinkDuration = 250; // 250ms per blink (on or off)
  } else if (!networkStatus.confirmed.websocket) {
    errorBlinkCount = 4;
    blinkDuration = 200; // 200ms per blink (on or off)
  }
  
  // If we have an error state, show the blink pattern
  if (errorBlinkCount > 0) {
    // Debug log only on first detection of error
    static int lastErrorBlinkCount = 0;
    if (errorBlinkCount != lastErrorBlinkCount) {
      LOG_INFO("LED", String("Error detected - showing ") + String(errorBlinkCount) + String(" blink pattern (WiFi:") + 
               String(networkStatus.confirmed.wifi) + String(" Internet:") + String(networkStatus.confirmed.internet) + 
               String(" Server:") + String(networkStatus.confirmed.server) + String(" WebSocket:") + 
               String(networkStatus.confirmed.websocket) + String(")"));
      lastErrorBlinkCount = errorBlinkCount;
    }
    
    wasInErrorState = true; // Remember we had an error
    unsigned long currentTime = millis();
    
    // Calculate total sequence time: (blinks * 2 * duration) + pause
    // Each blink has an ON and OFF phase
    int totalBlinkTime = errorBlinkCount * 2 * blinkDuration;
    int totalSequenceTime = totalBlinkTime + pauseDuration;
    
    // Calculate position in sequence
    unsigned long sequencePosition = currentTime % totalSequenceTime;
    
    // Determine if we're in blink phase or pause phase
    if (sequencePosition < totalBlinkTime) {
      // We're in the blink phase
      int blinkPosition = sequencePosition / blinkDuration;
      bool shouldBeOn = (blinkPosition % 2 == 0); // Even positions = ON, odd = OFF
      
      if (shouldBeOn != errorBlinkState || errorBlinkPhase != blinkPosition) {
        errorBlinkState = shouldBeOn;
        errorBlinkPhase = blinkPosition;
        digitalWrite(PIN_LED_BUTTON_LED, shouldBeOn ? HIGH : LOW);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, shouldBeOn ? HIGH : LOW);
        #endif
      }
    } else {
      // We're in the pause phase - LED should be OFF
      if (errorBlinkState) {
        errorBlinkState = false;
        errorBlinkPhase = -1;
        digitalWrite(PIN_LED_BUTTON_LED, LOW);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, LOW);
        #endif
      }
    }
    return; // Exit early during error blink
  }
  
  // If we just recovered from error state, force LED update
  if (wasInErrorState) {
    wasInErrorState = false;
    readyLedState = !isReadyForReceive(); // Force state mismatch to trigger update
    LOG_INFO("LED", "Recovered from error state, forcing LED update");
  } else {
    // Clear error blink count when no longer in error state
    static int lastErrorBlinkCount = 0;
    if (lastErrorBlinkCount != 0) {
      LOG_INFO("LED", "All network connections restored - switching to READY LED");
      lastErrorBlinkCount = 0;
    }
  }
#endif

  // Normal LED behavior for all versions
  bool shouldBeOn = isReadyForReceive();
  if (shouldBeOn != readyLedState) {
    digitalWrite(PIN_LED_BUTTON_LED, shouldBeOn ? HIGH : LOW); // Source 3.3V when ready
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, shouldBeOn ? HIGH : LOW); // Also control onboard LED (ESP32 Dev)
    #endif
    readyLedState = shouldBeOn;
    LOG_INFO("LED", String("Ready LED ") + (shouldBeOn ? "ON" : "OFF"));
  }
}

/**
 * Redraws the QR screen based on current mode and configuration.
 * Handles threshold mode, multi-channel mode with ticker, and product selection.
 */
void redrawQRScreen() {
  LOG_DEBUG("Display", "Redrawing QR screen...");

  // Threshold mode
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    LOG_DEBUG("Display", "Threshold QR screen displayed");
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
      LOG_DEBUG("Display", "Product selection screen displayed");
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
        LOG_DEBUG("Display", "BTC-Ticker OFF - Showing product selection screen");
        deviceState.transition(DeviceState::READY);
        return;
      } else {
        // Show ticker for "always" or "selecting" modes
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        LOG_DEBUG("Display", "Bitcoin ticker screen displayed");
        deviceState.transition(DeviceState::READY);
        return;
      }
    } else {
      // Show product QR for selected product (1..4)
      String label = "";
      int displayPin = 0;

      // Map product number (1-4) to pin (12, 13, 10, 11)
      switch (multiChannelConfig.currentProduct) {
        case 1: displayPin = 12; break;
        case 2: displayPin = 13; break;
        case 3: displayPin = 10; break;
        case 4: displayPin = 11; break;
        default: displayPin = 12; break;
      }

      // Get label from array, or use fallback
      int pinIndex = getPinIndex(displayPin);
      if (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0) {
        label = productLabels.labels[pinIndex];
      } else {
        label = "Pin " + String(displayPin);
      }

      ensureQrForPin(displayPin);
      showProductQRScreen(label, displayPin);
      multiChannelConfig.btcTickerActive = false;
      LOG_DEBUG("Display", String("Product ") + String(multiChannelConfig.currentProduct) + String(" QR screen displayed"));
      deviceState.transition(DeviceState::READY);
      return;
    }
  }

  // Single mode (1-channel)
  if (specialModeConfig.mode != "standard") {
    // SPECIAL MODE: ensure LNURL for pin 12 is up-to-date, then show special QR
    ensureQrForPin(12);
    showSpecialModeQRScreen();
    multiChannelConfig.btcTickerActive = false;
    LOG_DEBUG("Display", "Special mode QR screen displayed (single mode)");
    deviceState.transition(DeviceState::READY);
    return;
  }

  if (multiChannelConfig.btcTickerMode == "always") {
    // ALWAYS: show BTC ticker
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
    LOG_DEBUG("Display", "Bitcoin ticker screen displayed (single mode, ALWAYS)");
    deviceState.transition(DeviceState::READY);
    return;
  }

  // OFF or SELECTING: show QR unless ticker is currently active
  if (multiChannelConfig.btcTickerActive) {
    btctickerScreen();
    LOG_DEBUG("Display", "Bitcoin ticker screen refreshed (single mode, SELECTING active)");
    deviceState.transition(DeviceState::READY);
    return;
  } else {
    ensureQrForPin(12);
    showQRScreen();
    LOG_DEBUG("Display", "QR screen displayed (single mode)");
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
      ensureQrForPin(12);
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
      LOG_DEBUG("Screensaver", String("Elapsed: ") + String(elapsedTime) + " ms / Timeout: " + String(powerConfig.activationTimeoutMs) + " ms (" + String(elapsedTime * 100.0 / powerConfig.activationTimeoutMs, 1) + "%)");
      lastDebugOutput = currentTime;
    }

    if (elapsedTime >= powerConfig.activationTimeoutMs) {
      LOG_INFO("Screensaver", "Timeout reached, activating screensaver");
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
      LOG_DEBUG("DeepSleep", String("Elapsed: ") + String(elapsedTime) + " ms / Timeout: " + String(powerConfig.activationTimeoutMs) + " ms (" + String(elapsedTime * 100.0 / powerConfig.activationTimeoutMs, 1) + "%)");
      lastDebugOutputDeep = currentTime;
    }

    if (elapsedTime >= powerConfig.activationTimeoutMs) {
      LOG_INFO("DeepSleep", "Timeout reached, preparing for deep sleep");
      deviceState.transition(DeviceState::DEEP_SLEEP);

      // Flush serial output before sleep
      Serial.flush();

      // Prepare display for sleep
      prepareDeepSleep();

      // Give more time for display operations to complete
      vTaskDelay(pdMS_TO_TICKS(1000));

      // Final serial flush
      LOG_INFO("DeepSleep", "Entering sleep mode now...");
      Serial.flush();

      // Enter deep sleep (will not return - device will restart on wake-up)
      setupDeepSleepWakeup(powerConfig.deepSleep);

      // Note: This code is never reached because deep sleep/freeze mode restarts the device
    }
  }
}
