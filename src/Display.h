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
void thankYouScreen();
void showQRScreen();
void showThresholdQRScreen();