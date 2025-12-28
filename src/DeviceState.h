#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <Arduino.h>

// ============================================================================
// Device Operating States
// ============================================================================
enum class DeviceState {
    INITIALIZING,           // During boot, hardware setup
    CONNECTING_WIFI,        // WiFi connection in progress
    READY,                  // Ready for payments (normal operation)
    RECEIVING_PAYMENT,      // QR code displayed, awaiting payment
    SCREENSAVER,            // Screensaver active
    HELP_SCREEN,            // Help pages displayed
    REPORT_SCREEN,          // Report/Log screen
    CONFIG_MODE,            // Configuration editor
    ERROR_CRITICAL,         // Critical error (WiFi down, API unavailable)
    ERROR_RECOVERABLE,      // Recoverable error (temporary issue)
    DEEP_SLEEP,             // Device in deep sleep
    PRODUCT_SELECTION,      // Multi-product selection screen
    BTC_TICKER              // Bitcoin ticker display
};

// ============================================================================
// WiFi Connectivity State (Orthogonal to Device State)
// ============================================================================
enum class WiFiState {
    DISCONNECTED,           // Not connected
    CONNECTING,             // Attempting connection
    CONNECTED,              // Connected and ready
    ERROR                   // Connection error
};

// ============================================================================
// State Manager - Central State Machine Handler
// ============================================================================
class StateManager {
private:
    DeviceState currentState;
    DeviceState previousState;
    unsigned long stateEnteredTime;
    
    WiFiState wifiState;
    unsigned long wifiStateChangedTime;

public:
    StateManager() 
        : currentState(DeviceState::INITIALIZING),
          previousState(DeviceState::INITIALIZING),
          stateEnteredTime(millis()),
          wifiState(WiFiState::DISCONNECTED),
          wifiStateChangedTime(millis()) {}

    // ========================================================================
    // Main State Management
    // ========================================================================

    /**
     * Transition to new device state with validation and callbacks
     * @param newState Target device state
     * @return true if transition successful, false if invalid
     */
    bool transition(DeviceState newState) {
        // No-op if already in this state
        if (currentState == newState) return true;

        // Validate transition
        if (!isValidTransition(currentState, newState)) {
            Serial.printf("[STATE_ERROR] Invalid transition: %s -> %s\n",
                         getDeviceStateName(currentState),
                         getDeviceStateName(newState));
            return false;
        }

        // Log transition
        Serial.printf("[STATE_TRANSITION] %s -> %s\n",
                     getDeviceStateName(currentState),
                     getDeviceStateName(newState));

        // Execute exit callback for previous state
        onStateExit(currentState);

        // Update state
        previousState = currentState;
        currentState = newState;
        stateEnteredTime = millis();

        // Execute entry callback for new state
        onStateEnter(currentState);

        return true;
    }

    /**
     * Get current device state
     */
    DeviceState getState() const { return currentState; }

    /**
     * Get previous device state
     */
    DeviceState getPreviousState() const { return previousState; }

    /**
     * Check if currently in specific state
     */
    bool isInState(DeviceState state) const { return currentState == state; }

    /**
     * Get time spent in current state (milliseconds)
     */
    unsigned long timeInState() const { return millis() - stateEnteredTime; }

    /**
     * Get human-readable state name for logging
     */
    const char* getDeviceStateName(DeviceState state) const {
        switch (state) {
            case DeviceState::INITIALIZING:        return "INITIALIZING";
            case DeviceState::CONNECTING_WIFI:     return "CONNECTING_WIFI";
            case DeviceState::READY:               return "READY";
            case DeviceState::RECEIVING_PAYMENT:   return "RECEIVING_PAYMENT";
            case DeviceState::SCREENSAVER:         return "SCREENSAVER";
            case DeviceState::HELP_SCREEN:         return "HELP_SCREEN";
            case DeviceState::REPORT_SCREEN:       return "REPORT_SCREEN";
            case DeviceState::CONFIG_MODE:         return "CONFIG_MODE";
            case DeviceState::ERROR_CRITICAL:      return "ERROR_CRITICAL";
            case DeviceState::ERROR_RECOVERABLE:   return "ERROR_RECOVERABLE";
            case DeviceState::DEEP_SLEEP:          return "DEEP_SLEEP";
            case DeviceState::PRODUCT_SELECTION:   return "PRODUCT_SELECTION";
            case DeviceState::BTC_TICKER:          return "BTC_TICKER";
            default:                               return "UNKNOWN";
        }
    }

    // ========================================================================
    // WiFi State Management (Orthogonal to Device State)
    // ========================================================================

    /**
     * Update WiFi connection state
     * Automatically triggers device state transitions on connection loss/gain
     */
    void updateWiFiState(WiFiState newWiFiState) {
        if (wifiState == newWiFiState) return;

        Serial.printf("[WiFi] %s -> %s\n",
                     getWiFiStateName(wifiState),
                     getWiFiStateName(newWiFiState));

        wifiState = newWiFiState;
        wifiStateChangedTime = millis();

        // WiFi lost during critical states
        if (newWiFiState == WiFiState::DISCONNECTED ||
            newWiFiState == WiFiState::ERROR) {

            if (currentState == DeviceState::READY ||
                currentState == DeviceState::RECEIVING_PAYMENT ||
                currentState == DeviceState::BTC_TICKER) {

                Serial.println("[STATE] WiFi lost - transitioning to CONNECTING_WIFI");
                transition(DeviceState::CONNECTING_WIFI);
            }
        }

        // WiFi restored
        if (newWiFiState == WiFiState::CONNECTED) {
            if (currentState == DeviceState::CONNECTING_WIFI) {
                Serial.println("[STATE] WiFi reconnected - returning to READY");
                transition(DeviceState::READY);
            }
        }
    }

    /**
     * Get current WiFi state
     */
    WiFiState getWiFiState() const { return wifiState; }

    /**
     * Check if WiFi is connected
     */
    bool isWiFiConnected() const { return wifiState == WiFiState::CONNECTED; }

    /**
     * Check if WiFi has issues
     */
    bool hasWiFiError() const { return wifiState == WiFiState::ERROR; }

    /**
     * Get human-readable WiFi state name
     */
    const char* getWiFiStateName(WiFiState state) const {
        switch (state) {
            case WiFiState::DISCONNECTED:  return "DISCONNECTED";
            case WiFiState::CONNECTING:    return "CONNECTING";
            case WiFiState::CONNECTED:     return "CONNECTED";
            case WiFiState::ERROR:         return "ERROR";
            default:                       return "UNKNOWN";
        }
    }

    /**
     * Get time spent in current WiFi state
     */
    unsigned long wifiStateAge() const { return millis() - wifiStateChangedTime; }

private:
    // ========================================================================
    // Transition Validation
    // ========================================================================

    /**
     * Define valid transitions between states
     * Prevents invalid state changes
     */
    bool isValidTransition(DeviceState from, DeviceState to) const {
        // Can't transition FROM these states at all
        if (from == DeviceState::DEEP_SLEEP) {
            // Can only wake from deep sleep via initialization
            return to == DeviceState::INITIALIZING;
        }

        // Can't transition TO DEEP_SLEEP except from READY or SCREENSAVER
        if (to == DeviceState::DEEP_SLEEP) {
            return from == DeviceState::READY || from == DeviceState::SCREENSAVER;
        }

        // WiFi failure can jump to CONNECTING_WIFI from most states
        if (to == DeviceState::CONNECTING_WIFI) {
            // But NOT from CONFIG_MODE, ERROR_CRITICAL, or DEEP_SLEEP
            return from != DeviceState::CONFIG_MODE &&
                   from != DeviceState::ERROR_CRITICAL &&
                   from != DeviceState::DEEP_SLEEP;
        }

        // Can't escape ERROR_CRITICAL except via transition to INITIALIZING
        if (from == DeviceState::ERROR_CRITICAL) {
            return to == DeviceState::INITIALIZING;
        }

        // Most other transitions are valid
        return true;
    }

    // ========================================================================
    // State Callbacks - Execute on State Entry/Exit
    // ========================================================================

    /**
     * Called when exiting a state
     * Used for cleanup and state-specific shutdown
     */
    void onStateExit(DeviceState state) {
        switch (state) {
            case DeviceState::CONFIG_MODE:
                // Config mode cleanup handled by handleConfigExitButtons()
                Serial.println("[STATE_EXIT] CONFIG_MODE");
                break;

            case DeviceState::SCREENSAVER:
                Serial.println("[STATE_EXIT] SCREENSAVER");
                break;

            case DeviceState::DEEP_SLEEP:
                Serial.println("[STATE_EXIT] DEEP_SLEEP");
                break;

            case DeviceState::HELP_SCREEN:
                Serial.println("[STATE_EXIT] HELP_SCREEN");
                break;

            case DeviceState::REPORT_SCREEN:
                Serial.println("[STATE_EXIT] REPORT_SCREEN");
                break;

            default:
                break;
        }
    }

    /**
     * Called when entering a new state
     * Used for state-specific initialization
     * Actual hardware operations (display update, etc.) must be called from main.cpp
     */
    void onStateEnter(DeviceState state) {
        switch (state) {
            case DeviceState::READY:
                Serial.println("[STATE_ENTRY] READY - display QR code");
                // Actual display update done in main loop
                break;

            case DeviceState::CONFIG_MODE:
                Serial.println("[STATE_ENTRY] CONFIG_MODE - showing config screen");
                break;

            case DeviceState::HELP_SCREEN:
                Serial.println("[STATE_ENTRY] HELP_SCREEN - showing help pages");
                break;

            case DeviceState::CONNECTING_WIFI:
                Serial.println("[STATE_ENTRY] CONNECTING_WIFI - showing connection screen");
                break;

            case DeviceState::SCREENSAVER:
                Serial.println("[STATE_ENTRY] SCREENSAVER - display off");
                break;

            case DeviceState::DEEP_SLEEP:
                Serial.println("[STATE_ENTRY] DEEP_SLEEP - entering deep sleep");
                break;

            case DeviceState::ERROR_CRITICAL:
            case DeviceState::ERROR_RECOVERABLE:
                Serial.println("[STATE_ENTRY] ERROR - showing error screen");
                break;

            case DeviceState::PRODUCT_SELECTION:
                Serial.println("[STATE_ENTRY] PRODUCT_SELECTION");
                break;

            case DeviceState::BTC_TICKER:
                Serial.println("[STATE_ENTRY] BTC_TICKER");
                break;

            case DeviceState::REPORT_SCREEN:
                Serial.println("[STATE_ENTRY] REPORT_SCREEN");
                break;

            case DeviceState::RECEIVING_PAYMENT:
                Serial.println("[STATE_ENTRY] RECEIVING_PAYMENT");
                break;

            default:
                Serial.printf("[STATE_ENTRY] %s\n", getDeviceStateName(state));
                break;
        }
    }
};

#endif // DEVICE_STATE_H
