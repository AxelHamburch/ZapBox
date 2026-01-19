#pragma once

#include <Arduino.h>  // For String, uint8_t types

#ifdef ENABLE_DISPLAY

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
void thankYouScreen();
void drawQRCode();
void showQRScreen();
void showThresholdQRScreen();
void showSpecialModeQRScreen();
void showProductQRScreen(String label, int pin);
void productSelectionScreen();
void activateScreensaver(String mode);
void deactivateScreensaver();
bool isScreensaverActive();
void prepareDeepSleep();
void setupDeepSleepWakeup(String mode);
bool isDeepSleepActive();

#else

// Headless mode - stub implementations (no display)
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
inline void thankYouScreen() {}
inline void drawQRCode() {}
inline void showQRScreen() {}
inline void showThresholdQRScreen() {}
inline void showSpecialModeQRScreen() {}
inline void showProductQRScreen(String, int) {}
inline void productSelectionScreen() {}
inline void activateScreensaver(String) {}
inline void deactivateScreensaver() {}
inline bool isScreensaverActive() { return false; }
inline void prepareDeepSleep() {}
inline void setupDeepSleepWakeup(String) {}
inline bool isDeepSleepActive() { return false; }

#endif // ENABLE_DISPLAY
