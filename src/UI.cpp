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
extern int maxProducts;
#ifdef BOARD_JC3248W535C
extern T35AmbientConfig t35AmbientConfig;
#endif

// External function declarations from main.cpp
extern void showQRScreen();
extern void authyShowStart();   // Authy: IDENTITY TRIGGER idle screen
extern void showProductQRScreen(String label, int displayPin);
extern void showThresholdQRScreen();
extern void btctickerScreen();
extern void productSelectionScreen();
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
 * Writes a brightness level to the LED button pin.
 * Uses analogWrite (LEDC under the hood) so the screensaver pulse can fade the
 * LED. Boards without an LED pin (PIN_LED_BUTTON_LED < 0) are a no-op.
 */
void ledSetLevel(uint8_t level) {
#if PIN_LED_BUTTON_LED >= 0
  analogWrite(PIN_LED_BUTTON_LED, level);
#else
  (void)level;
#endif
}

#if ENABLE_DISPLAY
/**
 * Screensaver breathing pulse for the LED button.
 * While the screen is blanked the LED slowly fades in and out to draw attention
 * to the device and hint that pressing the button wakes it up.
 * Only runs when the device is ready — a dark LED still means "not ready".
 */
static const unsigned long LED_PULSE_PERIOD_MS = 6000; // one full breath
static const uint8_t LED_PULSE_MIN = 4;                // 0..255 = 0..100%
static const unsigned long LED_PULSE_STEP_MS = 20;     // 50 Hz update, flicker-free

static void updateScreensaverPulse() {
  static unsigned long lastPulseUpdate = 0;
  unsigned long now = millis();
  if (now - lastPulseUpdate < LED_PULSE_STEP_MS) return;
  lastPulseUpdate = now;

  // Phase derived from millis() rather than accumulated, so entering/leaving the
  // screensaver needs no state and the pulse never drifts.
  float phase = (now % LED_PULSE_PERIOD_MS) / (float)LED_PULSE_PERIOD_MS;
  float wave = (1.0f - cosf(phase * 2.0f * PI)) * 0.5f; // 0…1, smooth at both ends
  // Gamma correction: the eye is far more sensitive at low levels, a linear ramp
  // would look like it snaps on and then crawls.
  float corrected = powf(wave, 2.2f);
  ledSetLevel(LED_PULSE_MIN + (uint8_t)(corrected * (255 - LED_PULSE_MIN)));
}
#endif

/**
 * Updates the ready LED based on device state.
 * Only updates LED if state has changed to avoid redundant writes.
 * For headless version: Fast blink during initialization, slow blink in config mode, solid when ready.
 * Error states (headless): 1-4 blinks indicating specific network errors.
 */
void updateReadyLed() {
#if !ENABLE_DISPLAY
  // No LED pin on this board (PIN_LED_BUTTON_LED == -1) — skip all LED logic
  #if PIN_LED_BUTTON_LED < 0
  return;
  #endif
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

  // Identity mode: NFC card not recognised or rejected (tagid 404) → reuse 3× fast blink.
  if (authyConfig.enabled && authyState.nfcRejectedFlash) {
    authyState.nfcRejectedFlash = false;
    nfcNoLuckActive = true;
    nfcNoLuckStartTime = millis();
    ledSetOn(true);
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, HIGH);
    #endif
  }

  // NFC "NO LUCK" blink: 3× fast blink (100ms ON / 100ms OFF) after timeout/error,
  // then return to steady LED.
  if (nfcNoLuckActive) {
    unsigned long elapsed = millis() - nfcNoLuckStartTime;
    if (elapsed < 600) { // 3 blinks × 200ms (100 ON + 100 OFF) = 600ms
      bool on = ((elapsed / 100) % 2 == 0);
      static bool lastNoLuckState = true;
      if (on != lastNoLuckState) {
        lastNoLuckState = on;
        ledSetOn(on);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
        #endif
      }
      return;
    }
    // Blink sequence done → return to steady ON
    nfcNoLuckActive = false;
    ledSetOn(true);
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
        ledSetOn(true);
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
          ledSetOn(on);
          #ifdef PIN_ONBOARD_LED
          digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
          #endif
        }
      } else {
        // Blink sequence done – LED ON for rest of relay duration (it's the READY LED)
        ledSetOn(true);
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
        ledSetOn(nfcBlinkState);
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
      ledSetOn(true);
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

  // Teach mode LED (headless identity): double-pulse pattern.
  // When a card is enrolled: 6× rapid flash (50 ms ON/OFF) first, then resume.
  // Idle teach: 150ms ON / 100ms OFF / 150ms ON / 1500ms pause (total 1900ms).
  // This pattern is unique — distinguishable from all error/status blink patterns.
  if (authyConfig.enabled && authyState.teachActive) {
    static bool teachEnrollFlashActive = false;
    static unsigned long teachEnrollFlashStart = 0;

    if (authyState.teachEnrolledFlash) {
      authyState.teachEnrolledFlash = false;
      teachEnrollFlashActive = true;
      teachEnrollFlashStart = millis();
      ledSetOn(true);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, HIGH);
      #endif
    }
    if (teachEnrollFlashActive) {
      unsigned long el = millis() - teachEnrollFlashStart;
      if (el < 600) { // 6 × 100ms (50 ON + 50 OFF)
        bool on = ((el / 50) % 2 == 0);
        static bool lastTeachFlashState = true;
        if (on != lastTeachFlashState) {
          lastTeachFlashState = on;
          ledSetOn(on);
          #ifdef PIN_ONBOARD_LED
          digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
          #endif
        }
        return;
      }
      teachEnrollFlashActive = false;
    }
    // Double-pulse: ON(150) / OFF(100) / ON(150) / PAUSE(1500) = 1900ms cycle
    unsigned long t = millis() % 1900;
    bool on = (t < 150) || (t >= 250 && t < 400);
    static bool lastTeachState = false;
    if (on != lastTeachState) {
      lastTeachState = on;
      ledSetOn(on);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
      #endif
    }
    return;
  }

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
      ledSetOn(initBlinkState);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, initBlinkState ? HIGH : LOW);
      #endif
      lastInitBlinkTime = millis();
    }
    return; // Exit early during initialization blink
  }
  
  // Sensor blocking: very fast blink (10Hz = 50ms ON / 50ms OFF)
  // Indicates vending sensor is blocking payments (higher priority than network errors)
  if (lightBarrierConfig.isAnyBlocking()) {
    static unsigned long lastSensorBlinkTime = 0;
    static bool sensorBlinkState = false;
    if (millis() - lastSensorBlinkTime > 50) { // 50ms = 10Hz very fast blink
      sensorBlinkState = !sensorBlinkState;
      ledSetOn(sensorBlinkState);
      #ifdef PIN_ONBOARD_LED
      digitalWrite(PIN_ONBOARD_LED, sensorBlinkState ? HIGH : LOW);
      #endif
      lastSensorBlinkTime = millis();
    }
    return; // Exit early during sensor blocking
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
        ledSetOn(shouldBeOn);
        #ifdef PIN_ONBOARD_LED
        digitalWrite(PIN_ONBOARD_LED, shouldBeOn ? HIGH : LOW);
        #endif
      }
    } else {
      // We're in the pause phase - LED should be OFF
      if (errorBlinkState) {
        errorBlinkState = false;
        errorBlinkPhase = -1;
        ledSetOn(false);
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

#if ENABLE_DISPLAY
  // Screensaver: pulse instead of steady ON, but only while ready. A dark LED
  // keeps its meaning ("problem / not ready") in the screensaver too.
  if (shouldBeOn && isScreensaverActive()) {
    updateScreensaverPulse();
    // Invalidate the cached state so waking up always rewrites full brightness
    // instead of being skipped as "already ON".
    readyLedState = false;
    return;
  }
#endif

  if (shouldBeOn != readyLedState) {
    ledSetOn(shouldBeOn); // Source 3.3V when ready
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
  // Note: "both-boltcard" single-product mode no longer intercepts here.
  // bothModeScreen==0 maps to QR+BoltCard (QR shown, reader runs in background).
  // The initial BoltCard screen is handled by showInitialScreenAfterConnections().
  // After a payment, help, report, or WiFi recovery, always return to the QR screen.

  LOG_DEBUG("Display", "Redrawing QR screen...");

  // Mode selection pending: keep the selection screen visible
  if (multiChannelConfig.mode == "modeselect") {
    showModeSelectionScreen();
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Mini-PoS mode: amount entry screen, or invoice QR while one is pending
  if (miniPosConfig.enabled) {
    if (miniPosState.invoicePending) {
      showMiniPosQRScreen();
      LOG_DEBUG("Display", "Mini-PoS invoice QR screen displayed");
    } else {
      multiChannelConfig.btcTickerActive = false;
      miniPosState.inputActive = true;
      miniPosState.lastInputActivity = millis();
      showMiniPosInputScreen();
      LOG_DEBUG("Display", "Mini-PoS amount entry screen displayed");
    }
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Threshold mode
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    LOG_DEBUG("Display", "Threshold QR screen displayed");
    return;
  }

  // Multi-Channel-Control mode
  if (multiChannelConfig.mode != "off") {
    // Numerical product selection: restore keypad panel / product QR / main screen
    #ifdef BOARD_JC3248W535C
    if (t35AmbientConfig.numericSelect) {
      if (productSelectState.qrActive && productSelectState.qrPin > 0) {
        int pin = productSelectState.qrPin;
        int idx = getPinIndex(pin);
        String label = (idx >= 0 && productLabels.labels[idx].length() > 0)
                       ? productLabels.labels[idx] : "Pin " + String(pin);
        ensureQrForPin(pin);
        showProductSelectQRScreen(label, pin);
        LOG_DEBUG("Display", "Numeric selection product QR redrawn");
      } else if (productSelectState.panelActive) {
        productSelectState.lastActivity = millis();
        showProductSelectScreen();
        LOG_DEBUG("Display", "Numeric selection keypad panel redrawn");
      } else if (multiChannelConfig.btcTickerMode == "always") {
        multiChannelConfig.currentProduct = 0;
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        LOG_DEBUG("Display", "Numeric selection main screen (ticker) redrawn");
      } else {
        multiChannelConfig.currentProduct = -1;
        multiChannelConfig.btcTickerActive = false;
        productSelectionScreen();
        LOG_DEBUG("Display", "Numeric selection main screen (select product) redrawn");
      }
      deviceState.transition(DeviceState::READY);
      return;
    }
    #endif
    // Servo mode with only 1 active product: skip product selection, use product 1 directly
    if (multiChannelConfig.mode == "servo" && servoConfig.activeChannelCount() <= 1) {
      if (multiChannelConfig.currentProduct == -1 ||
          (multiChannelConfig.currentProduct == 0 && multiChannelConfig.btcTickerMode == "off")) {
        multiChannelConfig.currentProduct = 1;
      }
    }
    // T35 OFA / single-channel: if no product selected yet, auto-set to CH01
    #ifdef BOARD_JC3248W535C
    if (t35AmbientConfig.oneForAll && multiChannelConfig.currentProduct == -1) {
      multiChannelConfig.currentProduct = 1;
    }
    #endif
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

      // Map product number (1-4) to pin
      // Servo mode: dynamic mapping based on active channels
      if (multiChannelConfig.mode == "servo") {
        displayPin = servoConfig.productToPin(multiChannelConfig.currentProduct);
      } else {
        int idx0 = multiChannelConfig.currentProduct - 1;
        if (idx0 >= 0 && idx0 < RELAY_CHANNEL_MAX) {
          displayPin = RELAY_CHANNEL_PINS[idx0];
        } else {
          displayPin = RELAY_CHANNEL_PINS[0];
        }
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

  // Authy mode: redraws (after a payment, help, recovery) return to the
  // IDENTITY TRIGGER start screen — never the generic QR.
  if (authyConfig.enabled) {
    authyShowStart();
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Single mode (1-channel)
  if (specialModeConfig.mode != "standard") {
    // SPECIAL MODE: ensure LNURL for primary channel is up-to-date, then show QR
    ensureQrForPin(RELAY_CHANNEL_PINS[0]);
    showQRScreen();
    multiChannelConfig.btcTickerActive = false;
    LOG_DEBUG("Display", "Special mode QR screen displayed (single mode)");
    deviceState.transition(DeviceState::READY);
    return;
  }

  if (multiChannelConfig.btcTickerMode == "always") {
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
    ensureQrForPin(RELAY_CHANNEL_PINS[0]);
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
  // Mode selection pending: WiFi is ready but user hasn't picked a mode yet —
  // keep the selection screen visible; the loop() touch handler will handle the rest.
  if (multiChannelConfig.mode == "modeselect") {
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Mini-PoS mode: start on the amount entry screen — or on the BTC ticker
  // when ticker mode "always" is configured (ticker acts as screensaver).
  if (miniPosConfig.enabled) {
    miniPosState.resetInput();
    miniPosState.resetInvoice();
    strncpy(lightningConfig.lightning, MINIPOS_IDLE_TAG_URL,
            sizeof(lightningConfig.lightning) - 1);
    lightningConfig.lightning[sizeof(lightningConfig.lightning) - 1] = '\0';
    if (multiChannelConfig.btcTickerMode == "always") {
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
    } else {
      miniPosState.inputActive = true;
      miniPosState.lastInputActivity = millis();
      showMiniPosInputScreen();
      multiChannelConfig.btcTickerActive = false;
    }
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Threshold mode has priority
  if (lightningConfig.thresholdKey.length() > 0) {
    showThresholdQRScreen();
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Authy mode: idle on the IDENTITY TRIGGER start screen; the auth QR is only
  // fetched/shown when the user opens it by touch.
  if (authyConfig.enabled) {
    authyShowStart();
    productSelectionState.showTime = 0;
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Single mode
  if (multiChannelConfig.mode == "off") {
    if (multiChannelConfig.btcTickerMode == "always") {
      ensureQrForPin(RELAY_CHANNEL_PINS[0]); // pre-generate LNURL so NT3H writes immediately
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis();
    } else {
      // SELECTING or OFF: show QR screen (BoltCard reader runs in background for both-boltcard)
      ensureQrForPin(RELAY_CHANNEL_PINS[0]);
      showQRScreen();
      multiChannelConfig.btcTickerActive = false;
      productSelectionState.showTime = 0;
    }
    deviceState.transition(DeviceState::READY);
    return;
  }

  // Multi-Channel-Control (duo/quattro/servo)

#ifdef BOARD_JC3248W535C
  Serial.printf("[T35-INIT-DEBUG] oneForAll=%d maxProducts=%d mode=%s ticker=%s\n",
      (int)t35AmbientConfig.oneForAll, maxProducts,
      multiChannelConfig.mode.c_str(), multiChannelConfig.btcTickerMode.c_str());
  // Numerical product selection: no QR is shown on the main screen — the NT3H
  // tag carries the project URL until a product QR is opened via the keypad.
  if (t35AmbientConfig.numericSelect) {
    strncpy(lightningConfig.lightning, MINIPOS_IDLE_TAG_URL,
            sizeof(lightningConfig.lightning) - 1);
    lightningConfig.lightning[sizeof(lightningConfig.lightning) - 1] = '\0';
    productSelectState.resetAll();
    if (multiChannelConfig.btcTickerMode == "always") {
      multiChannelConfig.currentProduct = 0;
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      deviceState.transition(DeviceState::BTC_TICKER);
    } else {
      multiChannelConfig.currentProduct = -1;
      multiChannelConfig.btcTickerActive = false;
      productSelectionScreen();
      deviceState.transition(DeviceState::PRODUCT_SELECTION);
    }
    productSelectionState.showTime = millis();
    return;
  }
  // OFA active: always show CH01 QR directly, skip product selection entirely
  if (t35AmbientConfig.oneForAll && multiChannelConfig.mode != "off") {
    Serial.println("[T35-INIT-DEBUG] OFA path taken — showing CH01 QR");
    multiChannelConfig.currentProduct = 1;
    int firstPin = PIN_RELAY_CH01;
    int pinIndex = getPinIndex(firstPin);
    String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                   ? productLabels.labels[pinIndex] : String("Pin ") + String(firstPin);
    ensureQrForPin(firstPin);
    showProductQRScreen(label, firstPin);
    multiChannelConfig.btcTickerActive = false;
    deviceState.transition(DeviceState::READY);
    productSelectionState.showTime = millis();
    return;
  }
#endif

  // In servo mode with only 1 active product: skip product selection, show product 1 directly
  bool servoSingleProduct = (multiChannelConfig.mode == "servo" && servoConfig.activeChannelCount() <= 1);
  if (servoSingleProduct && multiChannelConfig.btcTickerMode != "always") {
    multiChannelConfig.currentProduct = 1;
    int firstPin = servoConfig.productToPin(1);
    int pinIndex = getPinIndex(firstPin);
    String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0) ? productLabels.labels[pinIndex] : String("Pin ") + String(firstPin);
    ensureQrForPin(firstPin);
    showProductQRScreen(label, firstPin);
    multiChannelConfig.btcTickerActive = false;
    deviceState.transition(DeviceState::READY);
    productSelectionState.showTime = millis();
  } else if (multiChannelConfig.btcTickerMode == "off") {
    multiChannelConfig.currentProduct = -1; // product selection
    productSelectionScreen();
    deviceState.transition(DeviceState::PRODUCT_SELECTION);
    multiChannelConfig.btcTickerActive = false;
    productSelectionState.showTime = millis();
  } else if (multiChannelConfig.btcTickerMode == "always") {
    multiChannelConfig.currentProduct = 0; // ticker
    btctickerScreen();
    multiChannelConfig.btcTickerActive = true;
    deviceState.transition(DeviceState::BTC_TICKER);
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
  // Screensaver mode activation (can run standalone or as first stage before deep sleep)
  if (!deviceState.isInState(DeviceState::SCREENSAVER) && !deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.screensaver != "off") {
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

  // Deep sleep activation (can follow screensaver or run standalone)
  if (!deviceState.isInState(DeviceState::DEEP_SLEEP) && powerConfig.deepSleep != "off") {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - activityTracking.lastActivityTime;

    // Debug output every 10 seconds
    static unsigned long lastDebugOutputDeep = 0;
    if (currentTime - lastDebugOutputDeep > 10000) {
      LOG_DEBUG("DeepSleep", String("Elapsed: ") + String(elapsedTime) + " ms / Timeout: " + String(powerConfig.deepSleepTimeoutMs) + " ms (" + String(elapsedTime * 100.0 / powerConfig.deepSleepTimeoutMs, 1) + "%)");
      lastDebugOutputDeep = currentTime;
    }

    if (elapsedTime >= powerConfig.deepSleepTimeoutMs) {
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
