#include <Arduino.h>
#include "PinConfig.h"
#include "DeviceState.h"
#include "GlobalState.h"
#include "Input.h"

// Externals from main.cpp
extern bool wakeFromPowerSavingMode();
extern void navigateToNextProduct();
extern void reportMode();
extern void configMode();
extern void showHelp();
extern unsigned long configModeStartTime;
extern StateManager deviceState;

void handleExternalSingleClick() {
  // Same behavior as NEXT/touch: wake powerConfig.screensaver/backlight and navigate/toggle ticker
  if (wakeFromPowerSavingMode()) {
    return;
  }
  navigateToNextProduct();
}

void handleExternalButton() {
  static int lastStableState = HIGH;
  static int lastRawState = HIGH;
  unsigned long now = millis();
  int rawState = digitalRead(PIN_LED_BUTTON_SW); // Pull-up, pressed = LOW

  // Detect raw state change and start debounce timer
  if (rawState != lastRawState) {
    externalButtonState.lastChange = now;
    lastRawState = rawState;
  }

  // Still debouncing - wait for stable signal
  if ((now - externalButtonState.lastChange) < ExternalButtonConfig::DEBOUNCE_MS) {
    return;
  }

  // Debounce complete - check if stable state changed
  if (rawState == lastStableState) {
    return; // No actual state change after debounce
  }

  // State has changed and is stable - this is a real edge!
  int state = rawState;

  // Falling edge: button pressed
  if (state == LOW && lastStableState == HIGH) {
    Serial.println("[EXT_BTN] Button pressed (falling edge detected)");
    externalButtonState.pressed = true;
    externalButtonState.holdActionFired = false;
    externalButtonState.pressStartTime = now;

    // Start/refresh multi-click window
    if (externalButtonState.clickCount == 0 || (now - externalButtonState.sequenceStart) > ExternalButtonConfig::TRIPLE_WINDOW_MS) {
      externalButtonState.clickCount = 0;
      externalButtonState.sequenceStart = now;
    }
  }

  // Rising edge: button released
  if (state == HIGH && lastStableState == LOW) {
    Serial.println("[EXT_BTN] Button released (rising edge detected)");
    externalButtonState.pressed = false;
    unsigned long pressDuration = now - externalButtonState.pressStartTime;
    Serial.printf("[EXT_BTN] Press duration: %lu ms\n", pressDuration);

    // If a hold action already fired, reset state
    if (externalButtonState.holdActionFired) {
      externalButtonState.holdActionFired = false;
      externalButtonState.clickCount = 0;
      externalButtonState.sequenceStart = 0;
    } else {
      // Treat as short click
      externalButtonState.clickCount++;
      Serial.printf("[EXT_BTN] Click count: %d\n", externalButtonState.clickCount);

      // Triple-click within window -> Report
      if (externalButtonState.clickCount >= 3 && (now - externalButtonState.sequenceStart) <= ExternalButtonConfig::TRIPLE_WINDOW_MS) {
        Serial.println("[EXT_BTN] Triple click -> Report Mode");
        reportMode();
        externalButtonState.clickCount = 0;
        externalButtonState.sequenceStart = 0;
      } else {
        // Immediate single-click action (wake/navigate)
        handleExternalSingleClick();

        // Reset window if expired
        if ((now - externalButtonState.sequenceStart) > ExternalButtonConfig::TRIPLE_WINDOW_MS) {
          externalButtonState.clickCount = 0;
          externalButtonState.sequenceStart = 0;
        }
      }
    }
  }

  // CRITICAL: Update lastStableState at the end so next call can detect changes
  lastStableState = state;
}

// Separate function to check hold actions - called continuously
void checkExternalButtonHolds() {
  if (!externalButtonState.pressed || externalButtonState.holdActionFired) {
    return;
  }

  unsigned long pressDuration = millis() - externalButtonState.pressStartTime;

  // Second-press hold >=3s → Config (double-click, hold on second)
  if (externalButtonState.clickCount == 1 && pressDuration >= ExternalButtonConfig::CONFIG_HOLD_MS) {
    Serial.println("[EXT_BTN] Second press held >=3s -> Config Mode");
    externalButtonState.holdActionFired = true;
    externalButtonState.clickCount = 0;
    externalButtonState.sequenceStart = 0;
    configMode();
    return;
  }

  // Single long hold (first press) >=2s → Help
  if (externalButtonState.clickCount == 0 && pressDuration >= ExternalButtonConfig::HELP_HOLD_MS) {
    Serial.println("[EXT_BTN] Long hold >=2s -> Help");
    externalButtonState.holdActionFired = true;
    externalButtonState.clickCount = 0;
    externalButtonState.sequenceStart = 0;
    showHelp();
    return;
  }
}

void handleConfigExitButtons() {
  if (!deviceState.isInState(DeviceState::CONFIG_MODE) || configModeStartTime == 0 || (millis() - configModeStartTime) < ExternalButtonConfig::CONFIG_EXIT_GUARD_MS) {
    return;
  }

  static int prevNextState = HIGH;
  static int prevExtState = HIGH;

  int nextState = digitalRead(PIN_BUTTON_1);
  int extState = digitalRead(PIN_LED_BUTTON_SW);

  // Positive flank (release) exits config
  if (prevNextState == LOW && nextState == HIGH) {
    Serial.println("[CONFIG] NEXT button release -> exit config mode");
    ESP.restart();
  }

  if (prevExtState == LOW && extState == HIGH) {
    Serial.println("[CONFIG] External button release -> exit config mode");
    ESP.restart();
  }

  prevNextState = nextState;
  prevExtState = extState;
}
