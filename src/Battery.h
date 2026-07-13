#pragma once

#include <Arduino.h>

// ============================================================================
// BATTERY GAUGE — JC3248W535C Touch 3.5" only
//
// The module wires the LiPo rail to GPIO 5 through a 33K/100K divider. GPIO 5 is
// also channel CH03, so the gauge only runs while CH03 is unconfigured; every
// function below is a no-op / returns false otherwise.
//
// On boards without a battery connector all of this compiles to nothing.
// ============================================================================

#ifdef BOARD_JC3248W535C

void initBattery();        // configure the ADC; no-op when CH03 is in use as a channel
void batteryLoop();        // call from loop(); samples internally every 10 s
bool batteryAvailable();   // false when CH03 is in use, or when no cell is connected
bool batteryNoCell();      // true when the pin rails: no cell connected / battery switch off
int  batteryPercent();     // 0..100
int  batteryMilliVolts();  // calibrated cell voltage — diagnostics / logging
bool batteryChanged();     // true once after the percentage changed (clears the flag)

#else

inline void initBattery() {}
inline void batteryLoop() {}
inline bool batteryAvailable() { return false; }
inline bool batteryNoCell()    { return false; }
inline int  batteryPercent()   { return 0; }
inline int  batteryMilliVolts(){ return 0; }
inline bool batteryChanged()   { return false; }

#endif  // BOARD_JC3248W535C
