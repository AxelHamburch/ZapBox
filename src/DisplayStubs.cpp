// DisplayStubs.cpp - Empty stub implementations for headless mode (no display)
// This file is ONLY compiled for esp32dev (controlled by src_filter in platformio.ini)

#include "Display.h"
#include <Arduino.h>

// All display functions are no-ops in headless mode
void initDisplay() {}
void startupScreen() {}
void btctickerScreen() {}
void updateBtctickerValues() {}
void initializationScreen() {}
void bootUpScreen() {}
void configModeScreen() {}
void errorReportScreen(uint8_t, uint8_t, uint8_t, uint8_t) {}
void wifiReconnectScreen() {}
void internetReconnectScreen() {}
void serverReconnectScreen() {}
void websocketReconnectScreen() {}
void stepOneScreen() {}
void stepTwoScreen() {}
void stepThreeScreen() {}
void actionTimeScreen() {}
void nfcPendingScreen() {}
void nfcNoLuckScreen() {}
void thankYouScreen() {}
void drawQRCode() {}
void showQRScreen() {}
void showThresholdQRScreen() {}
void showSpecialModeQRScreen() {}
void showProductQRScreen(String, int) {}
void productSelectionScreen() {}
void activateScreensaver(String) {}
void deactivateScreensaver() {}
bool isScreensaverActive() { return false; }
void prepareDeepSleep() {}
void setupDeepSleepWakeup(String) {}
bool isDeepSleepActive() { return false; }
