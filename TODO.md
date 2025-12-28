# ZapBox TODO


zapbox 
mit yellow & black, aber qr-code seite invertiert

## Bug:

- Restart geht nicht bei:

[BUTTON] Config mode button pressed
Config mo[LOOP] Config mode detected, exiting payment loop
Has existing data: YES
--- Se[CG_MODE_ENTER]
Waiting for commands...
WiFi initial state: CONNECTED

-> Fehlermeldung beim klicken: Restart only works in Config Mode

Er ist laut Display und Logs im Config Mode, hat es anscheinden nicht mitbekommen. Auch die Button READ und WRITE CONFIG sind ausgegraut. 

---

## Screensaver & Deep Sleep Implementation Plan

### 1. Display Management (Display.cpp/h)
- [ ] Add `displayScreensaverMode()` function to handle screensaver display changes
- [ ] Add `displayDeepSleepMode()` function to prepare display before deep sleep
- [ ] Implement display power control (turn off backlight for screensaver)
- [ ] Create timeout timer tracking variables:
  - `unsigned long screensaverActivationTime = 0;` - when screensaver/deep sleep was activated
  - `unsigned long activationTimeoutMinutes = 5;` - configurable timeout
  - `bool screensaverActive = false;` - current screensaver state
  - `bool deepSleepActive = false;` - current deep sleep state
  - `unsigned long lastActivityTime = 0;` - track last user interaction (button press)

### 2. Button Integration (main.cpp)
- [ ] Update button press handlers to reset activity timer:
  - BOOT button press (INT_PIN): Reset `lastActivityTime = millis()`
  - IO14 button press (PIN_BUTTON_2): Reset `lastActivityTime = millis()`
- [ ] Add logic to wake from screensaver/deep sleep on button press
  - Deactivate screensaver: Set `screensaverActive = false`, restore display, re-enable WebSocket
  - Wake from deep sleep: Call `esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO)` and resume normal operation

### 3. Main Loop Logic (main.cpp)
- [ ] Add timeout check in main loop:
  ```
  if ((screensaver == "on" || deepSleep == "on") && !screensaverActive && !deepSleepActive) {
    unsigned long elapsedTime = (millis() - lastActivityTime) / (1000 * 60); // Convert to minutes
    
    if (elapsedTime >= activationTimeoutMinutes) {
      if (screensaver == "on") {
        screensaverActive = true;
        displayScreensaverMode();
      } else if (deepSleep == "on") {
        deepSleepActive = true;
        prepareDeepSleep();
        esp_deep_sleep(UINT64_MAX); // Sleep indefinitely until GPIO wake
      }
    }
  }
  ```

### 4. GPIO Wake Interrupt (main.cpp)
- [ ] Configure GPIO pins for wake-up from deep sleep:
  - BOOT button (PIN_BUTTON_1): Enable as wake source
  - IO14 button (PIN_BUTTON_2): Enable as wake source
- [ ] Setup RTC memory to track if waking from deep sleep:
  - Check on startup: `if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)`
  - Resume normal operation and show "Resumed from Deep Sleep" message

### 5. Screensaver Display (Display.cpp)
- [ ] Create minimal "screensaver" display state:
  - Option 1: Show only time/status indicators on dark screen
  - Option 2: Animated pattern (moving dots, bouncing logo, etc.)
  - Option 3: Complete black screen (maximum power saving while still on)
- [ ] Implement display refresh at low frequency (e.g., every 5 seconds) to save power
- [ ] Keep WebSocket connection alive (don't disconnect)

### 6. Deep Sleep Configuration (main.cpp)
- [ ] Convert `activationTime` from string to integer (already in minutes)
- [ ] Set up GPIO pins as wake sources:
  ```
  esp_sleep_enable_ext0_wakeup(PIN_BUTTON_1, 0); // BOOT button
  esp_sleep_enable_ext0_wakeup(PIN_BUTTON_2, 0); // IO14 button
  ```
- [ ] Configure sleep timer if needed (optional):
  ```
  esp_sleep_enable_timer_wakeup(activationTimeoutMinutes * 60 * 1000000); // microseconds
  ```
- [ ] Save current configuration to RTC memory before sleep (survives deep sleep)

### 7. Mutual Exclusion Logic
- [ ] Ensure only one mode is active:
  - On startup: Check config, validate `screensaver` and `deepSleep` values
  - If both are "on", log warning and disable `deepSleep` (screensaver takes precedence)
- [ ] Validate `activationTime` is within range (1-120 minutes)

### 8. Testing & Debugging
- [ ] Test screensaver activation after timeout
- [ ] Test wake-up from screensaver with BOOT button
- [ ] Test wake-up from screensaver with IO14 button
- [ ] Test deep sleep entry and GPIO wake
- [ ] Verify WebSocket stays connected during screensaver
- [ ] Verify relay operations work normally when screensaver/deep sleep active
- [ ] Check power consumption improvements
- [ ] Test timeout calculation (ensure 5 minutes actually waits 5 minutes)
- [ ] Verify mutual exclusion works correctly

---

## Long Term:
- Oberer button lange drücken -> display bitcoin ticker
- Op Return Eintrag