#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

/**
 * Extract a value from a delimited string by index.
 * @param data The input string
 * @param separator The delimiter character
 * @param index The index of the value to extract (0-based)
 * @return The extracted substring, or empty string if index out of bounds
 */
String getValue(String data, char separator, int index);

/**
 * Execute special mode with PWM-like control.
 * Controls specified pin with configurable frequency and duty cycle ratio.
 * @param pin GPIO pin to control
 * @param duration_ms Total duration in milliseconds
 * @param freq Frequency in Hz
 * @param ratio ON/OFF time ratio
 */
void executeSpecialMode(int pin, unsigned long duration_ms, float freq, float ratio);

#endif // UTILS_H
