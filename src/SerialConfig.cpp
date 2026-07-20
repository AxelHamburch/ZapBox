#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "FFat.h"
#include "SerialConfig.h"
#ifdef BOARD_JC3248W535C
  #include "TouchAXS15231B.h"
#else
  #include "TouchCST816S.h"
#endif
#include "DeviceState.h"
#include "GlobalState.h"
#include "PinConfig.h"
#include "UI.h"   // ledSetOn() — all LED pin writes go through the PWM wrapper

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
        #if !defined(BOARD_ESP32C3_21_1) && !defined(BOARD_JC3248W535C)
        // PIN_BUTTON_2 = GPIO14 = SPI Flash IO1 on ESP32-C3 — never read as GPIO.
        // On JC3248W535C, GPIO14 = channel CH01 (no physical HELP button;
        // config is exited via touch) — reading it as a button causes spurious
        // floating-pin exits and a config-mode boot loop.
        prevHelpState = digitalRead(PIN_BUTTON_2);
        #endif
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
    // On ESP32-C3-21-1, PIN_BUTTON_2 = GPIO14 = SPI Flash IO1 — skip to avoid spurious exits.
    // On JC3248W535C, PIN_BUTTON_2 defaults to GPIO14 = channel CH01 (no HELP button) —
    // the floating pin produces phantom HIGH→LOW edges and a config-mode boot loop. Exit via touch.
    #if !defined(BOARD_ESP32C3_21_1) && !defined(BOARD_JC3248W535C)
    int helpState = digitalRead(PIN_BUTTON_2);
    if (prevHelpState == HIGH && helpState == LOW) { // Negative edge (button pressed)
        Serial.println("[CONFIG] HELP button pressed - exiting config mode");
        prevHelpState = helpState;
        initialized = false; // Reset for next config mode session
        return true;
    }
    prevHelpState = helpState;
    #endif
    
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

// Helper: Send a line over serial with USB CDC pacing.
// ESP32-S3 native USB CDC sends at USB speed, not limited by baud rate.
// Without pacing, rapid Serial.println() calls overflow the browser's
// Web Serial API buffer and bytes get silently dropped.
static void serialPrint(const String &msg) {
    Serial.print(msg);
    Serial.flush();
    delay(15);
}

static void serialPrintln(const String &msg) {
    Serial.println(msg);
    Serial.flush();
    delay(15);
}

static void serialPrintln() {
    Serial.println();
    Serial.flush();
    delay(15);
}

// Helper: Send long string over serial with USB CDC pacing (chunked).
// Used for any output longer than ~64 bytes to prevent buffer overflow.
static void serialWriteChunked(const String &msg) {
    const int chunkSize = 64;
    for (unsigned int i = 0; i < msg.length(); i += chunkSize) {
        unsigned int endPos = min(i + (unsigned int)chunkSize, (unsigned int)msg.length());
        Serial.write(msg.c_str() + i, endPos - i);
        Serial.flush();
        delay(15);
    }
}

void executeConfig(String wifiSSID, String wifiPass, bool hasExistingData)
{
    // WiFi is already OFF (disabled in configMode() before we get here).
    
    // Increase serial timeout for long config JSON lines (24 fields with URLs = 1300+ bytes).
    // Browser sends in 128-byte chunks with 15ms gaps; default 1000ms can expire
    // before the terminating \n arrives, causing truncated reads.
    Serial.setTimeout(5000);
    
    // CRITICAL: Ensure serial is fully ready (especially after wake from deep sleep)
    // Also wait for setup() on Core 1 to notice CONFIG_MODE and stop writing to Serial.
    // Additionally, WiFi event callbacks (e.g. ASSOC_LEAVE from WiFi.disconnect())
    // fire asynchronously on the WiFi task and write to Serial via esp_log — we must
    // wait long enough for those to complete before we start our own output.
    Serial.flush();
    // T-Display-S3 uses native USB CDC → needs longer stabilisation to prevent
    // the browser's Web Serial buffer from dropping bytes.
    // Headless ESP32 uses hardware UART (USB-UART bridge) → no CDC buffering,
    // and WiFi teardown already waited 500 ms before we get here.
#if ENABLE_DISPLAY
    delay(1500); // USB CDC stabilisation for T-Display-S3
#else
    delay(100);  // UART: just let WiFi event callbacks drain
#endif

    // Drain any stale bytes that other cores may have written while we waited
    while (Serial.available()) Serial.read();
    
    // Extra flush + drain: catch any late WiFi event log output
    Serial.flush();
    delay(100);
    while (Serial.available()) Serial.read();
    
    serialPrintln("");
    serialPrintln("===================================");
    serialPrintln("   Serial Config Mode Active");
    serialPrintln("===================================");
    serialPrintln("[CONFIG_MODE_ENTER]");
    serialPrintln("Waiting for commands...");
    serialPrintln("Touch screen or press button to exit.");
    
    // Send additional CONFIG_MODE_ENTER markers after pauses for reliable
    // detection by the web installer (USB CDC may chunk or interleave the first one).
    delay(200);
    serialPrintln("[CONFIG_MODE_ENTER]");
    Serial.flush();
    
    // Third marker after a longer pause — catches edge cases where late
    // WiFi/ESP-IDF log callbacks corrupted the earlier markers.
    delay(500);
    serialPrintln("[CONFIG_MODE_ENTER]");
    Serial.flush();

    // Drain any stale RX bytes (from other cores) before entering the command
    // loop.  This must happen AFTER all CONFIG_MODE_ENTER sends are done so
    // that no user command sent in response to the first ENTER is accidentally
    // consumed.
    delay(50);
    while (Serial.available()) Serial.read();

    // Capture the current touch state so the guard loop uses rising-edge
    // detection.  The AXS15231B keeps reporting isPressed()==true until the
    // data is consumed; without this snapshot the stale 5th-tap event fires
    // immediately after the 2 s guard expires and exits config mode at once.
    bool touchLastPressed = false;
    if (touchControllerPtr != nullptr) {
#ifdef BOARD_JC3248W535C
        touchLastPressed = ((TouchAXS15231B*)touchControllerPtr)->isPressed();
#else
        touchLastPressed = ((TouchCST816S*)touchControllerPtr)->isPressed();
#endif
    }

    unsigned long lastActivity = millis(); // Track last serial activity
    const unsigned long inactivityTimeout = 180000; // 180 seconds

#if !ENABLE_DISPLAY
    // LED blink state for config mode (headless version only)
    // Start with LED ON so the first blink is visible immediately
    ledSetOn(true);
    #ifdef PIN_ONBOARD_LED
    digitalWrite(PIN_ONBOARD_LED, HIGH);
    #endif
    unsigned long lastConfigBlinkTime = millis();
    bool configBlinkState = true; // Already ON
#endif

    while (true)
    {
        yield(); // Feed the watchdog timer
        
#if !ENABLE_DISPLAY
        // Blink LEDs for config mode indication (headless version only)
        if (millis() - lastConfigBlinkTime > 1000) { // Blink every 1 second (1Hz)
            configBlinkState = !configBlinkState;
            ledSetOn(configBlinkState);
            #ifdef PIN_ONBOARD_LED
            digitalWrite(PIN_ONBOARD_LED, configBlinkState ? HIGH : LOW);
            #endif
            lastConfigBlinkTime = millis();
        }
#endif
        
        // Check for button exit (NEXT or HELP pressed)
        if (checkButtonExit()) {
            serialPrintln("[CONFIG_MODE_EXIT]");
            delay(500);
            ESP.restart();
        }
        
        // Check for touch exit only after the normal guard period and only on
        // a rising edge (new press). The AXS15231B keeps reporting the last
        // touch until data is consumed, so we require a transition from
        // not-pressed → pressed to avoid the stale 5th-tap event triggering
        // an immediate exit right after the guard expires.
        if (touchControllerPtr != nullptr &&
            configModeStartTime > 0 &&
            (millis() - configModeStartTime) >= ExternalButtonConfig::CONFIG_EXIT_GUARD_MS)
        {
#ifdef BOARD_JC3248W535C
            TouchAXS15231B* touch = (TouchAXS15231B*)touchControllerPtr;
#else
            TouchCST816S* touch = (TouchCST816S*)touchControllerPtr;
#endif
            bool touchNow = touch->isPressed();
            if (touchNow && !touchLastPressed)
            {
                serialPrintln("[CONFIG] Touch detected - exiting config mode");
                serialPrintln("[CONFIG_MODE_EXIT]");
                delay(500);
                ESP.restart();
            }
            touchLastPressed = touchNow;
        }
        
        // Check for inactivity timeout - only if existing data is present
        if (hasExistingData && (millis() - lastActivity > inactivityTimeout))
        {
            serialPrintln("\n--- Inactivity timeout (180s) - returning to QR screen ---");
            serialPrintln("[CONFIG_MODE_EXIT]");
            delay(500);
            ESP.restart();
        }
        
        // WiFi is OFF during config mode - no need to check.
        // WiFi will be re-established on ESP.restart() after config is done.

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
            
        // Echo received command - skip echo for /file-read to keep output clean
        // (the response already shows what was requested)
        KeyValue kv = extractKeyValue(data);
        String commandName = kv.key;
        
        if (commandName != "/file-read") {
            if (data.length() > 100) {
                int spaceIdx = data.indexOf(' ');
                String cmdPart = (spaceIdx > 0) ? data.substring(0, spaceIdx) : data;
                serialPrintln("received: " + cmdPart + " [" + String(data.length()) + " bytes]");
            } else {
                serialPrintln("received: " + data);
            }
        }

        // Debounce /file-read: ignore if last read was less than 1s ago.
        // Prevents USB CDC buffer corruption from rapid-fire read requests.
        static unsigned long lastFileReadTime = 0;
        if (commandName == "/file-read") {
            if (millis() - lastFileReadTime < 1000) {
                continue; // Silently skip — browser will use last successful response
            }
            lastFileReadTime = millis();
        }
        
        if (commandName == "/config-done")
        {
            serialPrintln("/config-done");
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
        delay(50);
        // Resend CONFIG_MODE_ENTER so the installer detects config mode even if
        // it connected AFTER the initial signals were sent at startup.
        // executeCommand() is only called from executeConfig(), so this is always correct.
        serialPrintln("[CONFIG_MODE_ENTER]");
        return;
    }
    if (commandName == "/config-restart")
    {
        serialPrintln("- Restarting ESP32...");
        serialPrintln("[CONFIG_MODE_EXIT]");
        delay(500);
        ESP.restart();
        return;
    }
    if (commandName == "/config-soft-reset")
    {
        serialPrintln("- Soft reset: Restarting ESP32 (connection stays open)...");
        serialPrintln("[CONFIG_MODE_EXIT]");
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
    if (commandName == "/file-write")
    {
        return writeFile(path, data);
    }

    if (commandName == "/file-read")
    {
        return readFile(path);
    }

    Serial.println("- Unknown command");
    Serial.flush();
    delay(15);
}

void removeFile(String path)
{
    serialPrintln("- Remove file: " + path);
    FFat.remove("/" + path);
}

void appendToFile(String path, String data)
{
    serialPrintln("- Append to file: " + path);
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
    serialPrintln("- Append done");
}

void writeFile(String path, String data)
{
    serialPrintln("- Write file: " + path);
    File file = FFat.open("/" + path, FILE_WRITE);
    if (file)
    {
        file.print(data);
        file.close();
        serialPrintln("- Write done");
    }
    else
    {
        serialPrintln("- Write failed");
    }
}

void readFile(String path)
{
    // Warm up USB CDC output to prevent byte-loss on first transmission
    // after a period of inactivity (e.g., first /file-read after entering
    // config mode).  The first 64-byte chunk can be silently dropped when
    // the USB endpoint resumes after idle; these sacrificial bytes ensure
    // the actual payload starts on a "warm" endpoint.
    Serial.print("\n");
    Serial.flush();
    delay(50);

    File file = FFat.open("/" + path, "r");
    if (file)
    {
        String content = file.readString();
        file.close();
        
        // Send response: /file-read <JSON>
        // No extra status lines — keeps serial output clean and predictable
        String response = "/file-read " + content;
        serialWriteChunked(response);
        Serial.println(); // Terminating newline
        Serial.flush();
        delay(50); // Gap after data payload
    }
    else
    {
        serialPrintln("- Failed to open file for reading");
    }
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