#include "I2CBus.h"
#include "Log.h"
#include <Arduino.h>
#include <string.h>

SemaphoreHandle_t i2cBusMutex = nullptr;

void i2cBusInit()
{
    i2cBusMutex = xSemaphoreCreateMutex();
    if (i2cBusMutex == nullptr) {
        LOG_ERROR("I2CBus", "Failed to create I2C mutex");
    }
}

bool i2cTake()
{
    if (i2cBusMutex == nullptr) return true; // mutex not yet initialised – allow access
    BaseType_t ok = xSemaphoreTake(i2cBusMutex, pdMS_TO_TICKS(500));
    if (ok != pdTRUE) {
        LOG_WARN("I2CBus", "Timeout waiting for I2C mutex");
    }
    return (ok == pdTRUE);
}

void i2cGive()
{
    if (i2cBusMutex == nullptr) return;
    xSemaphoreGive(i2cBusMutex);
}

// ---- Module status summary -------------------------------------------------

enum I2CModuleState : uint8_t { I2C_MODULE_UNCONFIGURED = 0, I2C_MODULE_FOUND, I2C_MODULE_MISSING };

struct I2CModuleStatus {
    const char    *name;
    uint8_t        addr;
    I2CModuleState state;
};

// Fixed order so the banner always lists the same modules, whether or not a
// driver ran. Addresses are filled in by i2cReportModule() — the PCF8575 is
// jumper-selectable, so its address is only known at runtime.
static I2CModuleStatus i2cModules[] = {
    { "PN532",    0x24, I2C_MODULE_UNCONFIGURED },
    { "NT3H2111", 0x55, I2C_MODULE_UNCONFIGURED },
    { "PCF8574",  0x20, I2C_MODULE_UNCONFIGURED },
    { "PCF8575",  0x21, I2C_MODULE_UNCONFIGURED },
};

void i2cReportModule(const char *name, uint8_t addr, bool present)
{
    for (auto &m : i2cModules) {
        if (strcmp(m.name, name) != 0) continue;
        m.addr  = addr;
        m.state = present ? I2C_MODULE_FOUND : I2C_MODULE_MISSING;
        return;
    }
}

void i2cPrintModuleSummary()
{
    for (const auto &m : i2cModules) {
        switch (m.state) {
            case I2C_MODULE_FOUND:   // ✅
                Serial.printf("   %-9s 0x%02X \xE2\x9C\x85\n", m.name, m.addr);
                break;
            case I2C_MODULE_MISSING: // ❌
                Serial.printf("   %-9s 0x%02X \xE2\x9D\x8C\n", m.name, m.addr);
                break;
            default:
                Serial.printf("   %-9s      \xE2\x80\x93 not configured\n", m.name);
                break;
        }
    }
}
