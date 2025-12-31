#include "Navigation.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "PinConfig.h"
#include "Display.h"
#include "Payment.h"
#include "UI.h"
#include "TouchCST816S.h"
#include <Arduino.h>
#include "Log.h"

// External references to main.cpp
extern StateManager deviceState;
extern MultiChannelConfig multiChannelConfig;
extern SpecialModeConfig specialModeConfig;
extern ProductLabels productLabels;
extern ProductSelectionState productSelectionState;
extern PowerConfig powerConfig;
extern ActivityTracking activityTracking;
extern TouchState touchState;
extern TouchCST816S touch;
extern DisplayConfig displayConfig;
extern unsigned long configModeStartTime;

// External constants
extern unsigned long TOUCH_DOUBLE_CLICK_MS;

// External function declarations from main.cpp
extern void showHelp();
extern void configMode();
extern void reportMode();
extern void showQRScreen();
extern void showProductQRScreen(String label, int displayPin);
extern void showSpecialModeQRScreen();
extern void btctickerScreen();
extern void deactivateScreensaver();
extern void configMode();
extern void reportMode();
extern void showHelp();

/**
 * Navigate to next product in multi-channel mode.
 */
void navigateToNextProduct() {
  // Wake from power saving mode if active
  if (wakeFromPowerSavingMode()) {
    LOG_DEBUG("Navigation", "Device woke up, not navigating");
    return; // Don't navigate, just wake up
  }
  
  LOG_INFO("Navigation", "Navigate button pressed");
  
  if (multiChannelConfig.mode == "off") {
    // Single mode behavior depends on multiChannelConfig.btcTickerMode
    if (multiChannelConfig.btcTickerMode == "always") {
      // Toggle between ticker and product QR when always mode is on
      if (multiChannelConfig.btcTickerActive) {
        LOG_INFO("Navigation", "Single mode ALWAYS - Switching from ticker to QR");
        multiChannelConfig.btcTickerActive = false;
        ensureQrForPin(12);
        if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
          showSpecialModeQRScreen();
        } else {
          showQRScreen();
        }
        productSelectionState.showTime = millis(); // Start timer for auto-return to ticker
      } else {
        LOG_INFO("Navigation", "Single mode ALWAYS - Switching from QR to ticker");
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        productSelectionState.showTime = 0; // Reset timer (ticker has no timeout)
      }
    } else if (multiChannelConfig.btcTickerMode == "selecting") {
      if (multiChannelConfig.btcTickerActive) {
        // Already showing ticker - skip back to QR immediately
        LOG_INFO("Navigation", "Single mode SELECTING - Skipping from ticker to QR");
        multiChannelConfig.btcTickerActive = false;
        ensureQrForPin(12);
        if (specialModeConfig.mode != "standard" && specialModeConfig.mode != "") {
          showSpecialModeQRScreen();
        } else {
          showQRScreen();
        }
        productSelectionState.showTime = 0; // Reset timer
      } else {
        // Show Bitcoin ticker for 10 seconds
        btctickerScreen();
        multiChannelConfig.btcTickerActive = true;
        productSelectionState.showTime = millis(); // Start 10-second timer
        LOG_INFO("Navigation", "Single mode with SELECTING - Showing Bitcoin ticker for 10 seconds");
      }
    } else {
      LOG_INFO("Navigation", "Single mode - no navigation available");
    }
    return; // Single mode, no multi-product navigation
  }
  
  // Check if we're on product selection screen (multiChannelConfig.currentProduct == -1)
  if (multiChannelConfig.currentProduct == -1) {
    // Start from first product
    multiChannelConfig.currentProduct = 1;
    productSelectionState.showTime = millis(); // Reset timer immediately to prevent race condition
  } else if (multiChannelConfig.btcTickerActive) {
    // If ticker is active, go back to first product
    multiChannelConfig.btcTickerActive = false;
    multiChannelConfig.currentProduct = 1;
    deviceState.transition(DeviceState::READY);
    LOG_INFO("Navigation", "Ticker active - returning to first product");
  } else {
    multiChannelConfig.currentProduct++;
  }
  
  // Determine navigation behavior based on multiChannelConfig.btcTickerMode
  if (multiChannelConfig.btcTickerMode == "selecting") {
    // SELECTING mode: After last product, show BTC ticker (which will auto-return to product selection)
    if (multiChannelConfig.mode == "duo" && multiChannelConfig.currentProduct > 2) {
      multiChannelConfig.currentProduct = 0; // Reset for next navigation
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis(); // Start timer for auto-return
      LOG_INFO("Navigation", "SELECTING mode - Showing Bitcoin ticker after last product");
      return;
    } else if (multiChannelConfig.mode == "quattro" && multiChannelConfig.currentProduct > 4) {
      multiChannelConfig.currentProduct = 0; // Reset for next navigation
      btctickerScreen();
      multiChannelConfig.btcTickerActive = true;
      productSelectionState.showTime = millis(); // Start timer for auto-return
      LOG_INFO("Navigation", "SELECTING mode - Showing Bitcoin ticker after last product");
      return;
    }
  } else {
    // ALWAYS or OFF mode: Loop back to first product
    if (multiChannelConfig.mode == "duo" && multiChannelConfig.currentProduct > 2) {
      multiChannelConfig.currentProduct = 1; // Loop back to first product
    } else if (multiChannelConfig.mode == "quattro" && multiChannelConfig.currentProduct > 4) {
      multiChannelConfig.currentProduct = 1; // Loop back to first product
    }
  }
  
  LOG_INFO("Navigation", String("Navigate to product: ") + String(multiChannelConfig.currentProduct));
  
  // IMPORTANT: Disable product selection screen FIRST to prevent concurrent screen updates
  deviceState.transition(DeviceState::READY);
  
  // Small delay to ensure any ongoing display operation completes
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Show product QR screen (multiChannelConfig.currentProduct should be 1-4 for duo/quattro)
  if (multiChannelConfig.currentProduct >= 1) {
    // Show product QR screen
    multiChannelConfig.btcTickerActive = false; // Exit Bitcoin ticker when navigating to products
    
    // Capture multiChannelConfig.currentProduct value to prevent race conditions
    int productNum = multiChannelConfig.currentProduct;
    String label = "";
    int pin = 0;
    
    // Map product number (1-4) to pin (12, 13, 10, 11)
    switch(productNum) {
      case 1: pin = 12; break;
      case 2: pin = 13; break;
      case 3: pin = 10; break;
      case 4: pin = 11; break;
      default:
        LOG_WARN("Navigation", String("Invalid product number ") + String(productNum) + String(", defaulting to Pin 12"));
        pin = 12;
        break;
    }
    
    // Get label from array, or use fallback
    int pinIndex = getPinIndex(pin);
    if (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0) {
      label = productLabels.labels[pinIndex];
    } else {
      label = "Pin " + String(pin);
    }
    
    // Generate LNURL dynamically and update QR for this pin
    ensureQrForPin(pin);
    
    // Show product screen
    showProductQRScreen(label, pin);
    LOG_INFO("Navigation", String("Showing product: ") + label + String(" (Pin ") + String(pin) + String(")"));
    
    // Reset product selection timer after every navigation
    productSelectionState.showTime = millis();
    LOG_DEBUG("Navigation", "Product selection timer reset");
  }
}

/**
 * Handle touch button interactions.
 */
void handleTouchButton()
{
  // If in Help mode: Allow second click to switch to Report
  if (deviceState.isInState(DeviceState::HELP_SCREEN)) {
    // Check for new button press
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
      touchState.pressed = true;
      touchState.pressStartTime = millis();
      
      // This is the second click - switch from Help to Report
      LOG_INFO("Touch", "Second click during Help -> Switching to Report Mode");
      deviceState.transition(DeviceState::REPORT_SCREEN); // Abort Help
      touchState.clickCount = 0; // Reset
      
      // Check for Config mode (display touch)
      if (touch.available()) {
        uint16_t mainTouchX = touch.getX();
        uint16_t mainTouchY = touch.getY();
        bool isMainAreaTouch = false;
        
        // Touch area detection based on displayConfig.orientation
        if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
          isMainAreaTouch = (mainTouchY <= 305);
        } else {
          isMainAreaTouch = (mainTouchX <= 145);
        }
        
        if (isMainAreaTouch) {
          LOG_INFO("Touch", "+ Display touch -> Config Mode");
          configMode();
          return;
        }
      }
      
      // No display touch -> Report Mode
      reportMode();
    }
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
      touchState.pressed = false;
    }
    return;
  }
  
  // If in Report mode: Button press aborts
  if (deviceState.isInState(DeviceState::REPORT_SCREEN)) {
    if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
      LOG_INFO("Touch", "Button press during Report - ABORTING");
      deviceState.transition(DeviceState::READY);
      touchState.pressed = true;
    }
    else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
      touchState.pressed = false;
    }
    touchState.clickCount = 0;
    return;
  }
  
  // FIRST: Check if click sequence timeout has expired (ALWAYS check, not just on touch events)
  if (touchState.clickCount > 0 && !touchState.pressed) {
    unsigned long timeSinceLastTouch = millis() - touchState.lastTime;
    
    // For 1 click: Wait 1 second for potential second click
    // If no second click after 1s → Start Help
    if (touchState.clickCount == 1 && timeSinceLastTouch > 1000 && !deviceState.isInState(DeviceState::HELP_SCREEN)) {
      LOG_INFO("Touch", "Timeout: 1 click, no second click -> Help");
      showHelp();
      touchState.clickCount = 0;
    }
    
    // For 2 clicks: Wait 1 second for potential third click
    // If no third click after 1s → Start Report
    else if (touchState.clickCount == 2 && timeSinceLastTouch > 1000 && !deviceState.isInState(DeviceState::REPORT_SCREEN)) {
      LOG_INFO("Touch", "Timeout: 2 clicks, no third click -> Report");
      reportMode();
      touchState.clickCount = 0;
    }
    
    // For 3 clicks: Wait 1 second for potential fourth click
    // If no fourth click after 1s → Reset (do nothing)
    else if (touchState.clickCount == 3 && timeSinceLastTouch > 1000) {
      LOG_DEBUG("Touch", "Timeout: 3 clicks, no fourth click -> Reset");
      touchState.clickCount = 0;
    }
    
    // If Help is running and more than 3s passed since last click: Reset
    if (deviceState.isInState(DeviceState::HELP_SCREEN) && timeSinceLastTouch > 3000) {
      touchState.clickCount = 0;
    }
  }
  
  // Config Mode Touch Exit: Any touch after 2s exits config mode
  if (deviceState.isInState(DeviceState::CONFIG_MODE) && configModeStartTime > 0 && (millis() - configModeStartTime) >= ExternalButtonConfig::CONFIG_EXIT_GUARD_MS) {
    if (digitalRead(PIN_TOUCH_INT) == LOW) {
      LOG_INFO("Touch", "Touch detected - exiting config mode");
      delay(100);
      ESP.restart();
    }
  }
  
  // Check if touch interrupt is triggered (GPIO 16 LOW when touched)
  if (digitalRead(PIN_TOUCH_INT) == LOW && !touchState.pressed) {
    // Touch detected - read coordinates to check if it's the button area
    uint16_t touchX = touch.getX();
    uint16_t touchY = touch.getY();
    
    // Define touch button area based on HARDWARE position
    // Touch coordinates are hardware-based (0-170 x 0-320), don't rotate with display!
    // Physical button is ALWAYS at the same hardware location: high Y values (Y > 305)
    // - Vertical (rotation=0): Button at BOTTOM of display → Y > 305
    // - Horizontal (rotation=1): Button at RIGHT of display → STILL Y > 305 (not X!)
    bool inButtonArea = (touchY > 305);
    
    // FIRST: Wake from powerConfig.screensaver if active (regardless of touch location)
    if (deviceState.isInState(DeviceState::SCREENSAVER)) {
      LOG_DEBUG("Touch", String("Display touched at X=") + String(touchX) + String(" Y=") + String(touchY) + String(" during screensaver - WAKING UP"));
      deviceState.transition(DeviceState::READY);
      deactivateScreensaver();
      powerConfig.lastWakeUpTime = millis();
      activityTracking.lastActivityTime = millis();
      // Don't process button click, just wake up
      return;
    }
    
    // If not in button area, update activity timer but don't process as button click
    if (!inButtonArea) {
      activityTracking.lastActivityTime = millis();
      return;
    }
    
    LOG_DEBUG("Touch", String("Button area touched at X=") + String(touchX) + String(" Y=") + String(touchY) + String(" (orientation=") + displayConfig.orientation + String(")"));
    
    // Touch button pressed
    touchState.pressed = true;
    touchState.pressStartTime = millis();
    
    // Increment click count if within double-click window
    unsigned long timeSinceLastTouch = millis() - touchState.lastTime;
    
    if (timeSinceLastTouch < TOUCH_DOUBLE_CLICK_MS && timeSinceLastTouch > 100) {
      // Within double-click window AND minimum 100ms since last touch (debounce)
      touchState.clickCount++;
      LOG_DEBUG("Touch", String("Click within window (") + String(timeSinceLastTouch) + String(" ms since last) - count now: ") + String(touchState.clickCount));
    } else if (timeSinceLastTouch <= 100) {
      // Too fast - likely bounce or accidental double-click, ignore
      LOG_DEBUG("Touch", String("Too fast (") + String(timeSinceLastTouch) + String(" ms), ignoring (debounce)"));
      return;
    } else {
      touchState.clickCount = 1; // Reset to 1 for new click sequence
      LOG_DEBUG("Touch", String("New click sequence (last click was ") + String(timeSinceLastTouch) + String(" ms ago)"));
    }
    touchState.lastTime = millis();
    
    LOG_DEBUG("Touch", String("Button click count: ") + String(touchState.clickCount));
    
    // Process clicks:
    // - Click 1: Wait for timeout (1s) → Help
    // - Click 2: Wait for timeout (1s) → Report (unless click 3 comes)
    // - Click 3: Wait for timeout (1s) → Nothing (waiting for click 4)
    // - Click 4: Immediate Config Mode
    if (touchState.clickCount == 4) {
      // Fourth click within timeout -> Config Mode (IMMEDIATE, no waiting)
      LOG_INFO("Touch", "Fourth click -> Config Mode");
      deviceState.transition(DeviceState::READY);  // Reset state before entering config
      configMode();
      touchState.clickCount = 0;
    }
    // For clicks 1, 2, and 3: Do nothing, let timeout handler decide
  }
  else if (digitalRead(PIN_TOUCH_INT) == HIGH && touchState.pressed) {
    // Touch released
    touchState.pressed = false;
    unsigned long pressDuration = millis() - touchState.pressStartTime;
    
    LOG_DEBUG("Touch", String("Button released after ") + String(pressDuration) + String(" ms"));
  }
}
