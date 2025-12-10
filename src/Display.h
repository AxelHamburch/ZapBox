#pragma once

void initDisplay();
void startupScreen();
void configModeScreen();
void errorReportScreen(uint8_t wifiCount, uint8_t internetCount, uint8_t serverCount, uint8_t websocketCount);
void wifiReconnectScreen();
void internetReconnectScreen();
void serverReconnectScreen();
void websocketReconnectScreen();
void stepOneScreen();
void stepTwoScreen();
void stepThreeScreen();
void switchedOnScreen();
void specialModeScreen();
void thankYouScreen();
void drawQRCode();
void showQRScreen();
void showThresholdQRScreen();
void showSpecialModeQRScreen();