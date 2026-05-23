#include "IOExpander.h"
#include "GlobalState.h"
#include "I2CBus.h"
#include "Log.h"

#if ENABLE_DISPLAY
#include <PCF8574.h>
#include <Wire.h>

static PCF8574 pcf(0x20);
static bool pcfInitialized = false;

// Output state bitmask: 1=HIGH (relay off), 0=LOW (relay on, active-LOW)
static uint8_t outputState = 0x00;

void initIOExpander() {
    if (!ioExpanderConfig.enabled) return;

    // PCF8574 is open-drain: LOW = relay on (chip sinks current), HIGH = relay off (weak pull-up).
    // Initialise all pins HIGH so all relays start off.
    outputState = 0xFF;

    i2cTake();
    bool found = pcf.begin();
    if (found) pcf.write8(outputState);
    i2cGive();

    if (!found) {
        LOG_ERROR("IOExpander", "PCF8574 not found at 0x20 – check wiring and address jumpers");
        ioExpanderConfig.enabled = false;
        return;
    }
    pcfInitialized = true;
    LOG_INFO("IOExpander", "PCF8574 initialized at 0x20 — 8 relay channels ready (virtual pins 200-207)");
}

void activateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    // No mode check: relay activation is triggered by a paid LNbits invoice
    // on virtual pin 200-207. If the PCF8574 is present we execute it.
    outputState &= ~(1 << ch); // LOW = active (PCF8574 sinks current)
    i2cTake();
    pcf.write8(outputState);
    i2cGive();
    LOG_INFO("IOExpander", String("CH") + String(ch + 5) + " (P" + String(ch) + ") activated");
}

void deactivateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    outputState |= (1 << ch); // HIGH = inactive (open-drain, weak pull-up)
    i2cTake();
    pcf.write8(outputState);
    i2cGive();
    LOG_INFO("IOExpander", String("CH") + String(ch + 5) + " (P" + String(ch) + ") deactivated");
}


#else
// Headless variant: PCF8574 support not available — stub out all functions
void initIOExpander() {}
void activateExpanderChannel(int) {}
void deactivateExpanderChannel(int) {}
#endif
