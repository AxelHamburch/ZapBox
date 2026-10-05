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

// ── Relay protection circuit (see RelayProtectionConfig in GlobalState.h) ──────

/**
 * Hard-limit a relay activation time. Returns durationMs unchanged when the
 * protection circuit is off; otherwise at most RelayProtectionConfig::MAX_DURATION_MS.
 * @param context short label for the log line printed when the value is cut
 */
int capRelayDuration(int durationMs, const char *context);

/**
 * True if the GPIO currently carries the ambient-light (backlight sync) signal
 * instead of a relay. Such a pin is not a switched load: the protection circuit
 * must neither cap nor guard it.
 */
bool isAmbientLightPin(int pin);

/**
 * Arm an independent failsafe for a GPIO relay pin that was just switched HIGH:
 * a timer (not the main loop) drives the pin LOW after durationMs + margin.
 * No-op while the protection circuit is off. Disarm once the pin was switched
 * LOW the regular way.
 */
void relayGuardArm(int pin, unsigned long durationMs);
void relayGuardDisarm(int pin);

#endif // UTILS_H
