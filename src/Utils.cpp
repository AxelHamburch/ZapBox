#include "Utils.h"
#include "GlobalState.h"
#include "DeviceState.h"

// External references to main.cpp
extern StateManager deviceState;
extern MultiChannelConfig multiChannelConfig;

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
  
  pinMode(pin, OUTPUT);
  
  // If Single mode (multiChannelConfig.mode == "off") and pin is 12, also prepare pin 13
  bool parallelPin13 = (multiChannelConfig.mode == "off" && pin == 12);
  if (parallelPin13) {
    pinMode(13, OUTPUT);
    Serial.println("[SPECIAL] Pin 13 will be controlled in parallel to Pin 12 (Single mode)");
  }
  
  // Execute cycles until duration is reached
  while (elapsed < duration_ms) {
    // Check for config mode interrupt
    if (deviceState.isInState(DeviceState::CONFIG_MODE)) {
      Serial.println("[SPECIAL] Interrupted by config mode");
      digitalWrite(pin, LOW);
      if (parallelPin13) {
        digitalWrite(13, LOW);
      }
      break;
    }
    
    cycleCount++;
    
    // PIN HIGH
    digitalWrite(pin, HIGH);
    if (parallelPin13) {
      digitalWrite(13, HIGH);
    }
    Serial.printf("[SPECIAL] Cycle %d: Pin HIGH\n", cycleCount);
    delay(onTime_ms);
    
    // PIN LOW
    digitalWrite(pin, LOW);
    if (parallelPin13) {
      digitalWrite(13, LOW);
    }
    Serial.printf("[SPECIAL] Cycle %d: Pin LOW\n", cycleCount);
    delay(offTime_ms);
    
    elapsed = millis() - startTime;
  }
  
  // Ensure pin is LOW at the end
  digitalWrite(pin, LOW);
  if (parallelPin13) {
    digitalWrite(13, LOW);
  }
  Serial.printf("[SPECIAL] Completed %d cycles in %lu ms\n", cycleCount, elapsed);
}
