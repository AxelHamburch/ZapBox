#include "Utils.h"
#include "GlobalState.h"
#include "DeviceState.h"
#include "Display.h"
#include "PinConfig.h"
#include <WebSocketsClient.h>

// External references to main.cpp
extern StateManager deviceState;
extern MultiChannelConfig multiChannelConfig;
extern WebSocketsClient webSocket;

// Helper function: Check if light barrier should stop the action
// Returns true if light barrier is active AND minimum action time has passed
inline bool shouldStopForLightBarrier(unsigned long actionStartTime) {
  #ifdef PIN_LIGHT_BARRIER
  if (!lightBarrierConfig.enabled) {
    return false; // Light barrier disabled
  }
  
  // Check if minimum action time has passed (2 seconds)
  unsigned long elapsed = millis() - actionStartTime;
  if (elapsed < lightBarrierConfig.minActionTime) {
    return false; // Too early to stop
  }
  
  // Check if light barrier is triggered (NPN active = LOW)
  bool lightBarrierActive = (digitalRead(PIN_LIGHT_BARRIER) == LOW);
  if (lightBarrierActive) {
    Serial.printf("[LIGHT BARRIER] Triggered in special mode after %lu ms - stopping action!\n", elapsed);
    return true;
  }
  #endif
  
  return false;
}

/**
 * Extract a value from a delimited string by index.
 */
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/**
 * Execute special mode with PWM-like control.
 * Controls specified pin with configurable frequency and duty cycle ratio.
 */
void executeSpecialMode(int pin, unsigned long duration_ms, float freq, float ratio) {
  Serial.println("[SPECIAL] Executing special mode:");
  Serial.printf("[SPECIAL] Pin: %d, Duration: %lu ms, Frequency: %.2f Hz, Ratio: %.2f\n", 
                pin, duration_ms, freq, ratio);
  
  // Calculate timing
  unsigned long period_ms = (unsigned long)(1000.0 / freq);
  unsigned long onTime_ms = (unsigned long)(period_ms / (1.0 + 1.0/ratio));
  unsigned long offTime_ms = period_ms - onTime_ms;
  
  Serial.printf("[SPECIAL] Period: %lu ms, ON: %lu ms, OFF: %lu ms\n", 
                period_ms, onTime_ms, offTime_ms);
  
  unsigned long startTime = millis();
  unsigned long elapsed = 0;
  int cycleCount = 0;
  int lastDisplayedSec = -1;
  
  pinMode(pin, OUTPUT);
  
  // If Single mode (multiChannelConfig.mode == "off") and pin is 12, also prepare pin 13
  bool parallelPin13 = (multiChannelConfig.mode == "off" && pin == 12);
  if (parallelPin13) {
    pinMode(13, OUTPUT);
    Serial.println("[SPECIAL] Pin 13 will be controlled in parallel to Pin 12 (Single mode)");
  }

  // ESP32-C3-21-1: GPIO5 (SSR) always fires together with GPIO4 (Relay)
  #ifdef BOARD_ESP32C3_21_1
  int parallelSsrPin = (pin == PIN_RELAY) ? PIN_SSR : -1;
  if (parallelSsrPin >= 0) {
    pinMode(parallelSsrPin, OUTPUT);
    Serial.printf("[SPECIAL] GPIO%d (SSR) will be controlled in parallel to GPIO%d (Relay)\n", parallelSsrPin, pin);
  }
  #else
  int parallelSsrPin = -1;
  #endif
  
  // Relay output mode: GPIO 22/23 switch together with Pin 12 (headless only)
  #if !ENABLE_DISPLAY
  bool relayOut1 = (pin == 12 && lightBarrierConfig.relayOutput);
  bool relayOut2 = (pin == 12 && lightBarrierConfig.relayOutput2);
  if (relayOut1) Serial.println("[SPECIAL] GPIO 22 will be controlled in parallel to Pin 12 (relay output)");
  if (relayOut2) Serial.println("[SPECIAL] GPIO 23 will be controlled in parallel to Pin 12 (relay output)");
  #endif
  
  // Execute cycles until duration is reached
  while (elapsed < duration_ms) {
    // Check for config mode interrupt
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      Serial.println("[SPECIAL] Interrupted by config mode");
      digitalWrite(pin, LOW);
      if (parallelPin13) {
        digitalWrite(13, LOW);
      }
      if (parallelSsrPin >= 0) digitalWrite(parallelSsrPin, LOW);
      #if !ENABLE_DISPLAY
      if (relayOut1) digitalWrite(PIN_SENSOR_1, LOW);
      if (relayOut2) digitalWrite(PIN_SENSOR_2, LOW);
      #endif
      break;
    }
    
    cycleCount++;
    
    // PIN HIGH
    digitalWrite(pin, HIGH);
    if (parallelPin13) {
      digitalWrite(13, HIGH);
    }
    if (parallelSsrPin >= 0) digitalWrite(parallelSsrPin, HIGH);
    #if !ENABLE_DISPLAY
    if (relayOut1) digitalWrite(PIN_SENSOR_1, HIGH);
    if (relayOut2) digitalWrite(PIN_SENSOR_2, HIGH);
    #endif
    Serial.printf("[SPECIAL] Cycle %d: Pin HIGH\n", cycleCount);
    
    // CRITICAL: Non-blocking delay that keeps WebSocket alive
    unsigned long delayStart = millis();
    while (millis() - delayStart < onTime_ms) {
      // Check light barrier during special mode
      if (shouldStopForLightBarrier(startTime)) {
        Serial.println("[SPECIAL] Light barrier stopped action early");
        digitalWrite(pin, LOW);
        if (parallelPin13) {
          digitalWrite(13, LOW);
        }
        if (parallelSsrPin >= 0) digitalWrite(parallelSsrPin, LOW);
        #if !ENABLE_DISPLAY
        if (relayOut1) digitalWrite(PIN_SENSOR_1, LOW);
        if (relayOut2) digitalWrite(PIN_SENSOR_2, LOW);
        #endif
        return; // Exit special mode immediately
      }
      // Update countdown timer once per second
      unsigned long totalElapsed = millis() - startTime;
      int remaining = (int)((duration_ms - totalElapsed) / 1000);
      if (remaining < 0) remaining = 0;
      if (remaining != lastDisplayedSec) {
        lastDisplayedSec = remaining;
        updateActionTimeCountdown(remaining);
      }
      webSocket.loop(); // Keep WebSocket connection alive
      vTaskDelay(pdMS_TO_TICKS(1)); // Yield to other tasks
    }
    
    // PIN LOW
    digitalWrite(pin, LOW);
    if (parallelPin13) {
      digitalWrite(13, LOW);
    }
    if (parallelSsrPin >= 0) digitalWrite(parallelSsrPin, LOW);
    #if !ENABLE_DISPLAY
    if (relayOut1) digitalWrite(PIN_SENSOR_1, LOW);
    if (relayOut2) digitalWrite(PIN_SENSOR_2, LOW);
    #endif
    Serial.printf("[SPECIAL] Cycle %d: Pin LOW\n", cycleCount);
    
    // CRITICAL: Non-blocking delay that keeps WebSocket alive
    delayStart = millis();
    while (millis() - delayStart < offTime_ms) {
      // Check light barrier during OFF phase too
      if (shouldStopForLightBarrier(startTime)) {
        Serial.println("[SPECIAL] Light barrier stopped action early (during OFF)");
        return; // Exit special mode immediately
      }
      // Update countdown timer once per second
      unsigned long totalElapsed = millis() - startTime;
      int remaining = (int)((duration_ms - totalElapsed) / 1000);
      if (remaining < 0) remaining = 0;
      if (remaining != lastDisplayedSec) {
        lastDisplayedSec = remaining;
        updateActionTimeCountdown(remaining);
      }
      webSocket.loop(); // Keep WebSocket connection alive
      vTaskDelay(pdMS_TO_TICKS(1)); // Yield to other tasks
    }
    
    elapsed = millis() - startTime;
  }
  
  // Ensure pin is LOW at the end
  digitalWrite(pin, LOW);
  if (parallelPin13) {
    digitalWrite(13, LOW);
  }
  #if !ENABLE_DISPLAY
  if (relayOut1) digitalWrite(PIN_SENSOR_1, LOW);
  if (relayOut2) digitalWrite(PIN_SENSOR_2, LOW);
  #endif
  Serial.printf("[SPECIAL] Completed %d cycles in %lu ms\n", cycleCount, elapsed);
}
