#include "IOExpander.h"
#include "GlobalState.h"
#include "I2CBus.h"
#include "Log.h"

#if ENABLE_DISPLAY
#include <PCF8574.h>
#include <PCF8575.h>
#include <MCP23017.h>
#include <Wire.h>

// ---- PCF8574 (8 channels, virtual pins 200-207) ----------------------------

static PCF8574 pcf(0x20);
static bool pcfInitialized = false;

// Output state bitmask. Meaning of a set bit depends on ioExpanderConfig.activeHigh:
//   active-LOW  (default): 1=HIGH=relay off, 0=LOW=relay on
//   active-HIGH:           1=HIGH=relay on,  0=LOW=relay off
static uint8_t outputState = 0x00;

// Port level that means "relay off" for the configured polarity.
static inline uint8_t idleState8() {
    return ioExpanderConfig.activeHigh ? 0x00 : 0xFF;
}

void initIOExpander() {
    if (!ioExpanderConfig.enabled) return;

    // Start with all relays off for the configured polarity.
    // Active-LOW: all ports HIGH (PCF8574 is open-drain, weak pull-up).
    // Active-HIGH: all ports LOW.
    outputState = idleState8();

    // Pass the idle state into begin(): the library would otherwise write its own
    // default of 0xFF first, which on an active-HIGH relay board would pulse every
    // relay on before we could correct it.
    i2cTake();
    bool found = pcf.begin(outputState);
    i2cReportModule("PCF8574", 0x20, found);
    i2cGive();

    if (!found) {
        LOG_ERROR("IOExpander", "PCF8574 not found at 0x20 – check wiring and address jumpers");
        ioExpanderConfig.enabled = false;
        return;
    }
    pcfInitialized = true;
    LOG_INFO("IOExpander", String("PCF8574 initialized at 0x20 — 8 relay channels ready "
                                  "(virtual pins 200-207), trigger=") +
                           (ioExpanderConfig.activeHigh ? "active-HIGH" : "active-LOW"));
}

void activateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    // No mode check: relay activation is triggered by a paid LNbits invoice
    // on virtual pin 200-207. If the PCF8574 is present we execute it.
    if (ioExpanderConfig.activeHigh) outputState |=  (1 << ch);
    else                             outputState &= ~(1 << ch);
    i2cTake();
    pcf.write8(outputState);
    i2cGive();
    LOG_INFO("IOExpander", String("CH") + String(ch + 200) + " (P" + String(ch) + ") activated");
}

void deactivateExpanderChannel(int ch) {
    if (!pcfInitialized || ch < 0 || ch > 7) return;
    if (ioExpanderConfig.activeHigh) outputState &= ~(1 << ch);
    else                             outputState |=  (1 << ch);
    i2cTake();
    pcf.write8(outputState);
    i2cGive();
    LOG_INFO("IOExpander", String("CH") + String(ch + 200) + " (P" + String(ch) + ") deactivated");
}

// ---- PCF8575 (16 channels, virtual pins 300-315) ---------------------------

static PCF8575 pcf16(0x21);
static bool pcf16Initialized = false;
static uint16_t outputState16 = 0x0000;

static inline uint16_t idleState16() {
    return ioExpander16Config.activeHigh ? 0x0000 : 0xFFFF;
}

// Human-readable PCF8575 port name: channels 0-7 → P00-P07, 8-15 → P10-P17.
static String portName16(int ch) {
    return String("P") + String(ch / 8) + String(ch % 8);
}

void initIOExpander16() {
    if (!ioExpander16Config.enabled) return;

    // Address collision guard: PCF8574 and PCF8575 share the 0x20-0x27 range.
    // If both were configured onto the same address neither could be addressed
    // reliably, so refuse to bring up the PCF8575 instead of producing random
    // relay behaviour.
    if (ioExpanderConfig.enabled && ioExpander16Config.address == 0x20) {
        LOG_ERROR("IOExpander16", "PCF8575 address 0x20 collides with the active PCF8574 – "
                                  "set A0 to VDD (0x21) or disable the PCF8574");
        ioExpander16Config.enabled = false;
        return;
    }

    outputState16 = idleState16();

    // Same reasoning as the PCF8574: hand begin() the idle state so the library's
    // 0xFFFF default never reaches an active-HIGH relay board.
    i2cTake();
    pcf16.setAddress(ioExpander16Config.address);
    bool found = pcf16.begin(outputState16);
    i2cReportModule("PCF8575", ioExpander16Config.address, found);
    i2cGive();

    if (!found) {
        LOG_ERROR("IOExpander16", String("PCF8575 not found at 0x") +
                                  String(ioExpander16Config.address, HEX) +
                                  " – check wiring and A0/A1/A2 jumpers");
        ioExpander16Config.enabled = false;
        return;
    }
    pcf16Initialized = true;
    LOG_INFO("IOExpander16", String("PCF8575 initialized at 0x") +
                             String(ioExpander16Config.address, HEX) +
                             " — 16 relay channels ready (virtual pins 300-315), trigger=" +
                             (ioExpander16Config.activeHigh ? "active-HIGH" : "active-LOW"));
}

void activateExpander16Channel(int ch) {
    if (!pcf16Initialized || ch < 0 || ch > 15) return;
    if (ioExpander16Config.activeHigh) outputState16 |=  (1u << ch);
    else                               outputState16 &= ~(1u << ch);
    i2cTake();
    pcf16.write16(outputState16);
    i2cGive();
    LOG_INFO("IOExpander16", String("CH") + String(ch + 300) + " (" + portName16(ch) + ") activated");
}

void deactivateExpander16Channel(int ch) {
    if (!pcf16Initialized || ch < 0 || ch > 15) return;
    if (ioExpander16Config.activeHigh) outputState16 &= ~(1u << ch);
    else                               outputState16 |=  (1u << ch);
    i2cTake();
    pcf16.write16(outputState16);
    i2cGive();
    LOG_INFO("IOExpander16", String("CH") + String(ch + 300) + " (" + portName16(ch) + ") deactivated");
}

// ---- MCP23017 (16 channels, virtual pins 400-415) --------------------------

static MCP23017 mcp(0x22);
static bool mcpInitialized = false;
static uint16_t outputStateMcp = 0x0000;

static inline uint16_t idleStateMcp() {
    return mcp23017Config.activeHigh ? 0x0000 : 0xFFFF;
}

// Human-readable MCP23017 port name: channels 0-7 → GPA0-GPA7, 8-15 → GPB0-GPB7.
static String portNameMcp(int ch) {
    return String("GP") + (ch < 8 ? "A" : "B") + String(ch % 8);
}

void initIOExpanderMCP() {
    if (!mcp23017Config.enabled) return;

    const uint8_t addr = mcp23017Config.address;

    // Address collision guard. The MCP23017 answers in the same 0x20-0x27 range as
    // both PCF chip families, and 0x24 belongs to the PN532 NFC reader. Refuse to
    // come up on a taken address instead of producing random relay behaviour.
    const char *clash = nullptr;
    if (ioExpanderConfig.enabled && addr == 0x20)                       clash = "the active PCF8574";
    else if (ioExpander16Config.enabled && addr == ioExpander16Config.address) clash = "the active PCF8575";
    else if (addr == 0x24)                                              clash = "the PN532 NFC reader";
    if (clash != nullptr) {
        LOG_ERROR("MCP23017", String("MCP23017 address 0x") + String(addr, HEX) +
                              " collides with " + clash +
                              " – set A1 to VDD and A0/A2 to GND (0x22)");
        mcp23017Config.enabled = false;
        return;
    }

    outputStateMcp = idleStateMcp();

    i2cTake();
    mcp.setAddress(addr);
    // begin(false) = no internal pull-ups. After power-on every MCP23017 port is a
    // high-impedance input, so nothing is driven yet: write the idle value into the
    // output latch FIRST, then switch the ports to output (IODIR = 0x0000). In that
    // order an active-HIGH relay board never sees a start-up pulse. The second
    // write16() re-asserts the latch after the direction change — two spare bytes at
    // boot, and it does not depend on how the library orders its register writes.
    bool found = mcp.begin(false);
    if (found) {
        mcp.write16(outputStateMcp);
        mcp.pinMode16(0x0000);
        mcp.write16(outputStateMcp);
    }
    i2cReportModule("MCP23017", addr, found);
    i2cGive();

    if (!found) {
        LOG_ERROR("MCP23017", String("MCP23017 not found at 0x") + String(addr, HEX) +
                              " – check wiring, the A0/A1/A2 pins and RESET (must be tied HIGH)");
        mcp23017Config.enabled = false;
        return;
    }
    mcpInitialized = true;
    LOG_INFO("MCP23017", String("MCP23017 initialized at 0x") + String(addr, HEX) +
                         " — 16 relay channels ready (virtual pins 400-415), trigger=" +
                         (mcp23017Config.activeHigh ? "active-HIGH" : "active-LOW"));
}

void activateExpanderMCPChannel(int ch) {
    if (!mcpInitialized || ch < 0 || ch > 15) return;
    if (mcp23017Config.activeHigh) outputStateMcp |=  (1u << ch);
    else                           outputStateMcp &= ~(1u << ch);
    i2cTake();
    mcp.write16(outputStateMcp);
    i2cGive();
    LOG_INFO("MCP23017", String("CH") + String(ch + 400) + " (" + portNameMcp(ch) + ") activated");
}

void deactivateExpanderMCPChannel(int ch) {
    if (!mcpInitialized || ch < 0 || ch > 15) return;
    if (mcp23017Config.activeHigh) outputStateMcp &= ~(1u << ch);
    else                           outputStateMcp |=  (1u << ch);
    i2cTake();
    mcp.write16(outputStateMcp);
    i2cGive();
    LOG_INFO("MCP23017", String("CH") + String(ch + 400) + " (" + portNameMcp(ch) + ") deactivated");
}

#else
// Headless variant: expander support not available — stub out all functions
void initIOExpander() {}
void activateExpanderChannel(int) {}
void deactivateExpanderChannel(int) {}
void initIOExpander16() {}
void activateExpander16Channel(int) {}
void deactivateExpander16Channel(int) {}
void initIOExpanderMCP() {}
void activateExpanderMCPChannel(int) {}
void deactivateExpanderMCPChannel(int) {}
#endif
