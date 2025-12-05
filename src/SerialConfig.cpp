#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "FFat.h"
#include "SerialConfig.h"

void configOverSerialPort(String wifiSSID, String wifiPass, bool hasExistingData)
{
    executeConfig(wifiSSID, wifiPass, hasExistingData);
}

void executeConfig(String wifiSSID, String wifiPass, bool hasExistingData)
{
    Serial.println("\n--- Serial Config Mode Active ---");
    Serial.println("Waiting for commands...");
    Serial.flush();

    // Remember initial WiFi state - only restart if WiFi comes BACK (was disconnected)
    bool wifiWasDisconnected = (WiFi.status() != WL_CONNECTED);
    Serial.printf("WiFi initial state: %s\n", wifiWasDisconnected ? "DISCONNECTED" : "CONNECTED");

    unsigned long lastWiFiCheck = millis();
    unsigned long lastActivity = millis(); // Track last serial activity
    const unsigned long inactivityTimeout = 120000; // 120 seconds

    while (true)
    {
        yield(); // Feed the watchdog timer
        
        // Check for inactivity timeout - only if existing data is present
        if (hasExistingData && (millis() - lastActivity > inactivityTimeout))
        {
            Serial.println("\n--- Inactivity timeout (20s) - returning to QR screen ---");
            Serial.flush();
            delay(500);
            ESP.restart();
        }
        
        // Check WiFi every 5 seconds to see if it's back
        if (millis() - lastWiFiCheck > 5000)
        {
            // Only restart if WiFi was disconnected and now came back
            if (wifiWasDisconnected && WiFi.status() == WL_CONNECTED)
            {
                Serial.println("\n--- WiFi reconnected! Restarting... ---");
                Serial.flush();
                delay(500);
                ESP.restart();
            }
            // Try to reconnect if we have credentials and WiFi is down
            else if (wifiWasDisconnected && wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED)
            {
                Serial.println("Attempting WiFi reconnect...");
                WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN); // Force scanning for all APs, not just the first one
                WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
            }
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
    Serial.println("executeCommand: " + commandName + " > " + commandData);
    KeyValue kv = extractKeyValue(commandData);
    String path = kv.key;
    String data = kv.value;

    if (commandName == "/hello")
    {
        // https://patorjk.com/software/taag/#p=display&f=Small+Slant&t=ZAPBOX
        Serial.println("  ____  ___   ___  ___  ____  _  __");
        Serial.println(" /_  / / _ | / _ \\\\/ _ )/ __ \\\\| |/_/");
        Serial.println("  / /_/ __ |/ ___/ _  / /_/ />  <");
        Serial.println(" /___/_/ |_/_/  /____/\\\\____/_/|_|");
        return;
    }
    if (commandName == "/config-restart")
    {
        Serial.println("- Restarting ESP32...");
        Serial.flush();
        delay(500);
        ESP.restart();
        return;
    }
    if (commandName == "/config-soft-reset")
    {
        Serial.println("- Soft reset: Restarting ESP32 (connection stays open)...");
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
        while (file.available())
        {
            String line = file.readStringUntil('\n');
            Serial.println("/file-read " + line);
        }
        file.close();
    }
    else
    {
        Serial.println("- Failed to open file for reading");
    }
    Serial.println("- Read file done");
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