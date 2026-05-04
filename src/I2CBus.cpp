#include "I2CBus.h"
#include "Log.h"

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
