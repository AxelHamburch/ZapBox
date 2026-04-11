#pragma once

#include <Arduino.h>  // For String, uint8_t types

#ifdef ENABLE_DISPLAY

// Must be called once in setup() BEFORE any display function.
// Creates the recursive FreeRTOS mutex that serializes all SPI/TFT access
// across Core 0 (Task1 button callbacks) and Core 1 (main loop).
void initDisplayMutex();

void initDisplay();
void startupScreen();
void btctickerScreen();
void updateBtctickerValues(); // Partial update - only values, no full redraw
void initializationScreen();
void bootUpScreen();
void configModeScreen();
void errorReportScreen(uint8_t wifiCount, uint8_t internetCount, uint8_t serverCount, uint8_t websocketCount);
void wifiReconnectScreen();
void internetReconnectScreen();
void serverReconnectScreen();
void websocketReconnectScreen();
void stepOneScreen();
void stepTwoScreen();
void stepThreeScreen();
void actionTimeScreen();
void updateActionTimeCountdown(int remainingSecs);
void nfcPendingScreen();
void nfcNoLuckScreen();
void nfcNotSupportedScreen();
void nfcErrorDetailScreen(const char* detail);
void thankYouScreen();
void productBlockedScreen();
void supplyBinEmptyScreen();
void drawQRCode();
void showQRScreen();
void showThresholdQRScreen();
void showSpecialModeQRScreen();
void showProductQRScreen(String label, int pin);
void showBoltCardScreen(String label, int pin);
void productSelectionScreen();
void activateScreensaver(String mode);
void deactivateScreensaver();
bool isScreensaverActive();
void prepareDeepSleep();
void setupDeepSleepWakeup(String mode);
bool isDeepSleepActive();

// NFC Hardware Test (ENABLE_NFC=1 + ENABLE_NFC_TEST=1)
void nfcTestScreen(String lnurlw);

#else

// Headless mode - stub implementations (no display)
inline void initDisplayMutex() {}
inline void initDisplay() {}
inline void startupScreen() {}
inline void btctickerScreen() {}
inline void updateBtctickerValues() {}
inline void initializationScreen() {}
inline void bootUpScreen() {}
inline void configModeScreen() {}
inline void errorReportScreen(uint8_t, uint8_t, uint8_t, uint8_t) {}
inline void wifiReconnectScreen() {}
inline void internetReconnectScreen() {}
inline void serverReconnectScreen() {}
inline void websocketReconnectScreen() {}
inline void stepOneScreen() {}
inline void stepTwoScreen() {}
inline void stepThreeScreen() {}
inline void actionTimeScreen() {}
inline void updateActionTimeCountdown(int) {}
inline void nfcPendingScreen() {}
inline void nfcNoLuckScreen() {}
inline void nfcNotSupportedScreen() {}
inline void nfcErrorDetailScreen(const char*) {}
inline void thankYouScreen() {}
inline void productBlockedScreen() {}
inline void supplyBinEmptyScreen() {}
inline void drawQRCode() {}
inline void showQRScreen() {}
inline void showThresholdQRScreen() {}
inline void showSpecialModeQRScreen() {}
inline void showProductQRScreen(String, int) {}
inline void showBoltCardScreen(String, int) {}
inline void productSelectionScreen() {}
inline void activateScreensaver(String) {}
inline void deactivateScreensaver() {}
inline bool isScreensaverActive() { return false; }
inline void prepareDeepSleep() {}
inline void setupDeepSleepWakeup(String) {}
inline bool isDeepSleepActive() { return false; }
inline void nfcTestScreen(String) {}

#endif // ENABLE_DISPLAY
