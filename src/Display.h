#pragma once

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