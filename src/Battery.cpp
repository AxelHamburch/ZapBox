#include "Battery.h"

#ifdef BOARD_JC3248W535C

#include "GlobalState.h"
#include "PinConfig.h"
#include "Log.h"

// ============================================================================
// CALIBRATION
//
// The schematic ratio (33K/100K -> 0.752) does NOT hold in practice. The divider
// has a ~25 kOhm source impedance and the module has no bypass cap at IO5, so the
// ADC's sample-and-hold drags the node down while measuring: the pin reads about
// 0.574 * V_BAT instead of 0.752 * V_BAT.
//
// That error is linear and stable, so it is calibrated out here rather than
// fixed in hardware. Values from a 2 h discharge run against a multimeter
// (2026-07-12, see temp/PlanungBatterie.md):
//
//     cell 4.07 V -> ADC 2388 mV      cell 3.93 V -> ADC 2325 mV
//     cell 4.01 V -> ADC 2367 mV      cell 3.89 V -> ADC 2312 mV
//     cell 3.96 V -> ADC 2341 mV
//
// Linear regression over those points:
//     V_BAT[mV] = 2.277 * ADC[mV] - 1371
//
// ⚠ Fitted between 3.89 V and 4.07 V only — everything below is extrapolated.
// The percentage table is deliberately conservative because of that. Refine it
// with the raw values this module logs (LOG_INFO "Battery") once a device has
// been run down to ~3.5 V.
// ============================================================================
static const float BATT_CAL_A = 2.277f;
static const float BATT_CAL_B = -1371.0f;

// A railed reading (~3107 mV, the ceiling of the 12 dB range) means NO BATTERY:
// with no cell to hold it down, the charger pushes the BAT node above the ADC's
// full scale. It does NOT mean "charging" — measured on the device, a charging
// cell reads perfectly normal values around 2300 mV, rising as it fills.
//
// Whether USB is attached cannot be detected at all: the divider hangs on the
// charger's BAT node, so with a cell connected the pin shows the cell voltage
// either way.
static const int BATT_ADC_NO_CELL_THRESHOLD = 2600;   // mV at the pin

static const uint32_t BATT_POLL_MS = 10000;
static const int      BATT_SAMPLES = 15;          // odd -> clean median

// LiPo discharge curve, conservative below 3.9 V (extrapolated region).
struct SocPoint { int mv; int pct; };
static const SocPoint kSocCurve[] = {
    {4150, 100}, {4050, 90}, {3950, 75}, {3850, 60}, {3800, 50},
    {3750,  40}, {3700, 30}, {3650, 20}, {3550, 10}, {3400,  5}, {3300, 0}
};
static const int kSocCount = sizeof(kSocCurve) / sizeof(kSocCurve[0]);

static bool     s_enabled   = false;
static bool     s_noCell    = false;
static bool     s_haveValue = false;
static int      s_percent   = 100;
static int      s_vbatMv    = 0;
static float    s_emaMv     = 0.0f;   // smoothed ADC reading
static bool     s_changed   = false;
static uint32_t s_nextPoll  = 0;

static int cmpInt(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

// Median of BATT_SAMPLES readings. The first read is discarded: it gives the
// sample-and-hold cap a chance to settle against the high-impedance divider.
//
// ⚠ The delays are part of the calibration, not padding. With a 25 kOhm source
// and no bypass cap, the node needs time to recover between samples — sampling
// faster reads systematically LOWER. These values (5 ms settle, 2 ms spacing)
// are the ones the calibration curve above was measured with; changing them
// invalidates it.
static int readAdcMilliVolts() {
    (void)analogReadMilliVolts(PIN_BAT_ADC);
    delay(5);

    int buf[BATT_SAMPLES];
    for (int i = 0; i < BATT_SAMPLES; i++) {
        buf[i] = (int)analogReadMilliVolts(PIN_BAT_ADC);
        delay(2);
    }
    qsort(buf, BATT_SAMPLES, sizeof(int), cmpInt);
    return buf[BATT_SAMPLES / 2];
}

static int socFromMilliVolts(int mv) {
    if (mv >= kSocCurve[0].mv)              return 100;
    if (mv <= kSocCurve[kSocCount - 1].mv)  return 0;
    for (int i = 0; i < kSocCount - 1; i++) {
        int hiMv = kSocCurve[i].mv,     hiPct = kSocCurve[i].pct;
        int loMv = kSocCurve[i + 1].mv, loPct = kSocCurve[i + 1].pct;
        if (mv <= hiMv && mv > loMv) {
            return loPct + (mv - loMv) * (hiPct - loPct) / (hiMv - loMv);
        }
    }
    return 0;
}

void initBattery() {
    s_enabled = t35AmbientConfig.batteryEnabled;
    if (!s_enabled) {
        LOG_INFO("Battery", String("Disabled — CH03 (GPIO ") + PIN_BAT_ADC
                          + ") is in use as a channel, so the divider cannot be read");
        return;
    }

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);   // 12 dB -> usable to ~3.1 V
    s_nextPoll = 0;                                   // sample immediately
    LOG_INFO("Battery", String("Enabled on GPIO ") + PIN_BAT_ADC + " (ADC1_CH4)");
}

void batteryLoop() {
    if (!s_enabled) return;
    if (millis() < s_nextPoll) return;
    s_nextPoll = millis() + BATT_POLL_MS;

    int adcMv = readAdcMilliVolts();

    if (adcMv > BATT_ADC_NO_CELL_THRESHOLD) {
        // Railed — no cell connected (or the battery switch is off). Nothing to
        // display; keep the gauge quiet rather than inventing a number.
        if (!s_noCell) LOG_INFO("Battery", String("No battery detected (adc=") + adcMv + " mV, railed)");
        s_noCell    = true;
        s_haveValue = false;
        s_emaMv     = 0.0f;   // restart smoothing when a cell shows up again
        return;
    }
    s_noCell = false;

    // Exponential moving average — WiFi transmit bursts briefly sag the rail.
    s_emaMv = (s_emaMv <= 0.0f) ? (float)adcMv : (0.2f * adcMv + 0.8f * s_emaMv);

    int vbat = (int)(BATT_CAL_A * s_emaMv + BATT_CAL_B);
    int pct  = socFromMilliVolts(vbat);

    // Hysteresis: never move more than one point per poll, so the display cannot
    // flicker between two values when the load changes.
    if (!s_haveValue) {
        s_percent   = pct;
        s_haveValue = true;
        s_changed   = true;
    } else if (pct != s_percent) {
        s_percent += (pct > s_percent) ? 1 : -1;
        s_changed  = true;
    }
    s_vbatMv = vbat;

    // Raw value stays in the log: this is the data the calibration gets refined
    // from once a device has been run down into the extrapolated region.
    LOG_INFO("Battery", String("adc=") + (int)s_emaMv + " mV  vbat=" + vbat
                      + " mV  soc=" + s_percent + "%");
}

bool batteryAvailable()  { return s_enabled && s_haveValue; }
bool batteryNoCell()     { return s_noCell; }
int  batteryPercent()    { return s_percent; }
int  batteryMilliVolts() { return s_vbatMv; }

bool batteryChanged() {
    bool c = s_changed;
    s_changed = false;
    return c;
}

#endif  // BOARD_JC3248W535C
