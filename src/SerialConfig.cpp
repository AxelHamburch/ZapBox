#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "FFat.h"
#include "SerialConfig.h"
#include "TouchCST816S.h"
#include "DeviceState.h"
#include "GlobalState.h"
#include "PinConfig.h"

// Global reference to touch controller (set from main.cpp)
void* touchControllerPtr = nullptr;
extern unsigned long configModeStartTime;
extern StateManager deviceState;

// Check if NEXT, HELP, or EXTERNAL button is pressed to exit config mode
static bool checkButtonExit() {
    // Static variables to track previous button states
    static int prevNextState = HIGH;
    static int prevHelpState = HIGH;
    static int prevExtState = HIGH;
    static bool initialized = false;
    
    // Initialize button states on first call (during guard period)
    // This prevents triggering exit if buttons are already pressed when config mode starts
    if (!initialized) {
        prevNextState = digitalRead(PIN_BUTTON_1);
        prevHelpState = digitalRead(PIN_BUTTON_2);
        #ifdef PIN_LED_BUTTON_SW
        prevExtState = digitalRead(PIN_LED_BUTTON_SW);
        #else
        prevExtState = HIGH; // Not available on this board
        #endif
        initialized = true;
    }
    
    // Only check after guard period
    if (configModeStartTime == 0 || (millis() - configModeStartTime) < ExternalButtonConfig::CONFIG_EXIT_GUARD_MS) {
        return false;
    }
    
    // Check NEXT button (PIN_BUTTON_1)
    int nextState = digitalRead(PIN_BUTTON_1);
    if (prevNextState == HIGH && nextState == LOW) { // Negative edge (button pressed)
        Serial.println("[CONFIG] NEXT button pressed - exiting config mode");
        prevNextState = nextState;
        initialized = false; // Reset for next config mode session
        return true;
    }
    prevNextState = nextState;
    
    // Check HELP button (PIN_BUTTON_2)
    int helpState = digitalRead(PIN_BUTTON_2);
    if (prevHelpState == HIGH && helpState == LOW) { // Negative edge (button pressed)
        Serial.println("[CONFIG] HELP button pressed - exiting config mode");
        prevHelpState = helpState;
        initialized = false; // Reset for next config mode session
        return true;
    }
    prevHelpState = helpState;
    
    // Check EXTERNAL button (PIN_LED_BUTTON_SW) - only if available
    #ifdef PIN_LED_BUTTON_SW
    int extState = digitalRead(PIN_LED_BUTTON_SW);
    if (prevExtState == HIGH && extState == LOW) { // Negative edge (button pressed)
        Serial.println("[CONFIG] EXTERNAL button pressed - exiting config mode");
        prevExtState = extState;
        initialized = false; // Reset for next config mode session
        return true;
    }
    prevExtState = extState;
    #endif
    
    return false;
}

void configOverSerialPort(String wifiSSID, String wifiPass, bool hasExistingData)
{
    executeConfig(wifiSSID, wifiPass, hasExistingData);
}

void executeConfig(String wifiSSID, String wifiPass, bool hasExistingData)
{
    // CRITICAL: Ensure serial is fully ready (especially after wake from deep sleep)
    Serial.flush();
    delay(200); // Give serial port time to stabilize
    
    Serial.println("\n--- Serial Config Mode Active ---");
    Serial.println("[CONFIG_MODE_ENTER]");
    Serial.println("Waiting for commands...");
    Serial.flush();
    
    // Send multiple CONFIG_MODE_ENTER signals to ensure web installer detects it
    for (int i = 0; i < 3; i++) {
        delay(100);
        Serial.println("[CONFIG_MODE_ENTER]");
        Serial.flush();
    }

    // Remember initial WiFi state - only restart if WiFi comes BACK (was disconnected)
    bool wifiWasDisconnected = (WiFi.status() != WL_CONNECTED);
    Serial.printf("WiFi initial state: %s\n", wifiWasDisconnected ? "DISCONNECTED" : "CONNECTED");
    Serial.flush();

    // CRITICAL: Stop WiFi activity during config mode to prevent:
    // 1. Constant 4WAY_HANDSHAKE_TIMEOUT spam in serial console
    // 2. Unnecessary CPU load from WiFi reconnect attempts
    // 3. Interference with serial communication
    // WiFi will be properly re-established on ESP.restart() after config is done
    if (wifiWasDisconnected) {
        WiFi.disconnect(true); // true = turn off WiFi radio completely
        WiFi.mode(WIFI_OFF);
        Serial.println("WiFi radio disabled during config mode");
        Serial.flush();
    }

    unsigned long lastWiFiCheck = millis();
    unsigned long lastActivity = millis(); // Track last serial activity
    const unsigned long inactivityTimeout = 180000; // 180 seconds

#if !ENABLE_DISPLAY
    // LED blink state for config mode (headless version only)
    unsigned long lastConfigBlinkTime = millis();
    bool configBlinkState = false;
#endif

    while (true)
    {
        yield(); // Feed the watchdog timer
        
#if !ENABLE_DISPLAY
        // Blink LEDs for config mode indication (headless version only)
        if (millis() - lastConfigBlinkTime > 1000) { // Blink every 1 second (1Hz)
            configBlinkState = !configBlinkState;
            digitalWrite(PIN_LED_BUTTON_LED, configBlinkState ? HIGH : LOW);
            #ifdef PIN_ONBOARD_LED
            digitalWrite(PIN_ONBOARD_LED, configBlinkState ? HIGH : LOW);
            #endif
            lastConfigBlinkTime = millis();
        }
#endif
        
        // Check for button exit (NEXT or HELP pressed)
        if (checkButtonExit()) {
            Serial.println("[CONFIG_MODE_EXIT]");
            Serial.flush();
            delay(500);
            ESP.restart();
        }
        
        // Check for touch exit (any touch after 2s in config mode)
        if (touchControllerPtr != nullptr && configModeStartTime > 0 && (millis() - configModeStartTime) > 0)
        {
            TouchCST816S* touch = (TouchCST816S*)touchControllerPtr;
            if (touch->available())
            {
                Serial.println("[CONFIG] Touch detected - exiting config mode");
                Serial.println("[CONFIG_MODE_EXIT]");
                Serial.flush();
                delay(500);
                ESP.restart();
            }
        }
        
        // Check for inactivity timeout - only if existing data is present
        if (hasExistingData && (millis() - lastActivity > inactivityTimeout))
        {
            Serial.println("\n--- Inactivity timeout (60s) - returning to QR screen ---");
            Serial.println("[CONFIG_MODE_EXIT]");
            Serial.flush();
            delay(500);
            ESP.restart();
        }
        
        // Check WiFi every 5 seconds to see if it's back
        // Only relevant if WiFi WAS connected when entering config mode
        // (WiFi radio is OFF if it was disconnected - see above)
        if (!wifiWasDisconnected && millis() - lastWiFiCheck > 5000)
        {
            // WiFi was connected but may have dropped - if still connected, no action needed
            // If it disconnected during config, don't try to reconnect (user may be changing credentials)
            lastWiFiCheck = millis();
        }

        if (Serial.available() == 0)
        {
            delay(10);
            continue;
        }
        
        // Reset activity timer when data is received
        lastActivity = millis();
        
        // Read until newline (handles both \n and \r\n)
        String data = Serial.readStringUntil('\n');
        data.trim(); // Remove whitespace and line endings (\r, \n, spaces)
        
        if (data.length() == 0)
            continue;
            
        Serial.print("received: ");
        Serial.println(data);
        Serial.flush();
        
        KeyValue kv = extractKeyValue(data);
        String commandName = kv.key;
        
        if (commandName == "/config-done")
        {
            Serial.println("/config-done");
            Serial.flush();
            delay(500);
            ESP.restart();
        }
        executeCommand(commandName, kv.value);
    }
}

void executeCommand(String commandName, String commandData)
{
    KeyValue kv = extractKeyValue(commandData);
    String path = kv.key;
    String data = kv.value;

    if (commandName == "/hello")
    {
        // https://patorjk.com/software/taag/#p=display&f=Small+Slant&t=ZAPBOX
        // Send logo as single block to prevent chunking
        String logo = "";
        logo += "  ____  ___   ___  ___  ____  _  __\n";
        logo += " /_  / / _ | / _ \\/ _ )/ __ \\| |/_/\n";
        logo += "  / /_/ __ |/ ___/ _  / /_/ />  <\n";
        logo += " /___/_/ |_/_/  /____/\\____/_/|_|\n";
        Serial.print(logo);
        Serial.flush();
        return;
    }
    if (commandName == "/config-restart")
    {
        Serial.println("- Restarting ESP32...");
        Serial.println("[CONFIG_MODE_EXIT]");
        Serial.flush();
        delay(500);
        ESP.restart();
        return;
    }
    if (commandName == "/config-soft-reset")
    {
        Serial.println("- Soft reset: Restarting ESP32 (connection stays open)...");
        Serial.println("[CONFIG_MODE_EXIT]");
        Serial.flush();
        delay(500);
        ESP.restart();
        return;
    }
    if (commandName == "/file-remove")
    {
        return removeFile(path);
    }
    if (commandName == "/file-append")
    {
        return appendToFile(path, data);
    }

    if (commandName == "/file-read")
    {
        return readFile(path);
    }

    Serial.println("- Unknown command");
}

void removeFile(String path)
{
    Serial.println("- Remove file: " + path);
    FFat.remove("/" + path);
}

void appendToFile(String path, String data)
{
    Serial.println("- Append to file: " + path);
    File file = FFat.open("/" + path, FILE_APPEND);
    if (!file)
    {
        file = FFat.open("/" + path, FILE_WRITE);
    }
    if (file)
    {
        file.println(data);
        file.close();
    }
}

void readFile(String path)
{
    Serial.println("- Read file: " + path);
    File file = FFat.open("/" + path, "r");
    if (file)
    {
        // Read entire file into a String and send as one block
        // This avoids chunking issues where individual small chunks
        // can overflow the Web Serial API's default 255-byte buffer
        String content = file.readString();
        file.close();
        
        // Send prefix + content + newline as efficiently as possible
        Serial.print("/file-read ");
        Serial.print(content);
        Serial.println();
        Serial.flush();
        delay(50); // Allow USB transfer to complete
    }
    else
    {
        Serial.println("- Failed to open file for reading");
    }
    Serial.println("- Read file done");
    Serial.flush();
}

KeyValue extractKeyValue(String s)
{
    int spacePos = s.indexOf(" ");
    String key = s.substring(0, spacePos);
    if (spacePos == -1)
    {
        return {key, ""};
    }
    String value = s.substring(spacePos + 1, s.length());
    return {key, value};
}