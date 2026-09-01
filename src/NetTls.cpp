#include "NetTls.h"
#include "Log.h"
#include <Arduino.h>

static SemaphoreHandle_t s_mutex       = nullptr;
static volatile uint32_t s_lastRelease = 0; // millis() of the last outermost release
static volatile uint8_t  s_depth       = 0; // recursion depth of the current holder

void netTlsInit()
{
#ifdef NET_TLS_DISABLE
    // Escape hatch for A/B testing: with a null mutex every gate call below is a
    // pass-through, so the firmware behaves exactly as it did before the gate
    // existed. Add -DNET_TLS_DISABLE to build_flags to rule the gate in or out.
    s_mutex = nullptr;
    LOG_WARN("NetTLS", "TLS gate DISABLED at build time (-DNET_TLS_DISABLE)");
#else
    s_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_mutex == nullptr) {
        LOG_ERROR("NetTLS", "Failed to create TLS gate mutex");
    }
#endif
}

// Remaining quiet time before a new handshake may start (0 = go ahead).
static uint32_t settleRemaining()
{
    if (s_lastRelease == 0) return 0; // nothing closed yet
    uint32_t since = millis() - s_lastRelease;
    return (since >= NET_TLS_SETTLE_MS) ? 0 : (NET_TLS_SETTLE_MS - since);
}

bool netTlsTake(const char *who, uint32_t timeoutMs)
{
    if (s_mutex == nullptr) return true; // gate not initialised – never block traffic

    uint32_t start = millis();
    if (xSemaphoreTakeRecursive(s_mutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        LOG_WARN("NetTLS", String(who) + ": timeout waiting for TLS slot");
        return false;
    }

    // Only the outermost take waits out the settle window — a nested take (e.g.
    // fetchSwitchLabels() from inside webSocketEvent()) reuses the open slot.
    if (s_depth == 0) {
        uint32_t remaining = settleRemaining();
        if (remaining > 0) {
            // Don't blow the caller's overall budget waiting for the settle gap.
            uint32_t spent = millis() - start;
            uint32_t budget = (spent < timeoutMs) ? (timeoutMs - spent) : 0;
            if (remaining > budget) remaining = budget;
            if (remaining > 0) vTaskDelay(pdMS_TO_TICKS(remaining));
        }
    }
    s_depth++;
    return true;
}

bool netTlsTryTake(const char *who)
{
    if (s_mutex == nullptr) return true;

    if (xSemaphoreTakeRecursive(s_mutex, 0) != pdTRUE) return false;

    if (s_depth == 0 && settleRemaining() > 0) {
        // Router still needs to release the previous conntrack entry.
        // Backing off here is what throttles the auto-reconnect storm.
        xSemaphoreGiveRecursive(s_mutex);
        return false;
    }
    s_depth++;
    return true;
}

void netTlsGive()
{
    if (s_mutex == nullptr) return;

    if (s_depth > 0 && --s_depth == 0) {
        s_lastRelease = millis();
        if (s_lastRelease == 0) s_lastRelease = 1; // 0 means "nothing closed yet"
    }
    xSemaphoreGiveRecursive(s_mutex);
}
