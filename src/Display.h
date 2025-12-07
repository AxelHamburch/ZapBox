#pragma once

void initDisplay();
void startupScreen();
void configModeScreen();
void errorReportScreen(byte wifiCount, byte internetCount, byte websocketCount);
void wifiReconnectScreen();
void internetReconnectScreen();
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