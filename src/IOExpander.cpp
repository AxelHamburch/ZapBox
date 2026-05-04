#include "IOExpander.h"
#include "GlobalState.h"
#include "Log.h"

#if ENABLE_DISPLAY
#include <PCF8574.h>
#include <Wire.h>

static PCF8574 pcf(0x20);
static bool pcfInitialized = false;

// Current output state bitmask (1 = HIGH for outputs / input-mode for sensors)
static uint8_t outputState = 0x00;

void initIOExpander() {
    if (!ioExpanderConfig.enabled) return;

    // Relay channels: PCF8574 is open-drain.
    // Active (relay ON)  = pin LOW  ÔåÆ chip sinks up to 25 mA
    // Inactive (relay OFF) = pin HIGH ÔåÆ open-drain, weak pull-up (~100 ┬ÁA only)
    // Sensor channels must be HIGH (input mode on PCF8574 open-drain bus).
    // ÔåÆ initialise ALL pins HIGH; relay channels pulled LOW when activated.
    outputState = 0xFF; // all HIGH on startup

    if (!pcf.begin()) {
        LOG_ERROR("IOExpander", "PCF8574 not found at 0x20 ÔÇô check wiring and address jumpers");
        ioExpanderConfig.enabled = false;
        return;
    }
    pcf.write8(outputState);
    pcfInitialized = true;
    LOG_INFO("IOExpander", "PCF8574 initialized at 0x20 (all pins HIGH, relay=active-LOW)");

    for (int i = 0; i < 8; i++) {
        if (ioExpanderConfig.channels[i].mode != "off") {
            LOG_INFO("IOExpander", String("CH") + String(i + 5, DEC) + ": " + ioExpanderConfig.channels[i].mode);
        }
    }
}

void activateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    if (ioExpanderConfig.channels[ch].mode != "relay") return;
    outputState &= ~(1 << ch); // LOW = active (PCF8574 sinks current)
    pcf.write8(outputState);
    LOG_INFO("IOExpander", String("CH") + String(ch + 5) + " (P" + String(ch) + ") activated");
}

void deactivateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    if (ioExpanderConfig.channels[ch].mode != "relay") return;
    outputState |= (1 << ch); // HIGH = inactive (open-drain, weak pull-up)
    pcf.write8(outputState);
    LOG_INFO("IOExpander", String("CH") + String(ch + 5) + " (P" + String(ch) + ") deactivated");
}

bool readExpanderSensor(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return false;
    // PCF8574 open-drain: pin reads LOW when NPN sensor pulls it to GND
    return (pcf.read8() & (1 << ch)) == 0;
}

#else
// Headless variant: PCF8574 support is T-Display-S3 only ÔÇö stub out all functions
void initIOExpander() {}
void activateExpanderChannel(int) {}
void deactivateExpanderChannel(int) {}
bool readExpanderSensor(int) { return false; }
#endif
