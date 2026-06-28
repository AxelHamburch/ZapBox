#pragma once

#include <Arduino.h>  // For String, uint8_t types
#include "GlobalState.h"

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
void identityTriggerScreen();
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
void showProductQRScreen(String label, int pin);
void showBoltCardScreen(String label, int pin);
void showMobilePhoneScreen(String label, int pin);
void productSelectionScreen();
void activateScreensaver(String mode);
void deactivateScreensaver();
bool isScreensaverActive();
void prepareDeepSleep();
void setupDeepSleepWakeup(String mode);
bool isDeepSleepActive();

// NFC Hardware Test (ENABLE_NFC=1 + ENABLE_NFC_TEST=1)
void nfcTestScreen(String lnurlw);

// PIN pad for BoltCard PIN entry (LUD pinLimit)
void showPinPadScreen(const PinPadState &state);
// Returns: 0-9=digit, 10=backspace, 11=clear, 12=cancel, -1=no hit
int  pinPadHitTest(uint16_t x, uint16_t y);

// Mini-PoS (Touch 3.5 only): amount entry screen, invoice QR with cancel
// button, paid confirmation. Stubs on all other boards.
#ifdef BOARD_JC3248W535C
void showMiniPosInputScreen();
// Returns: 0-9=digit, 10=backspace, 13=decimal point, 14=Invoice,
//          15=Last Pay, -1=no hit
int  miniPosHitTest(uint16_t x, uint16_t y);
void showMiniPosQRScreen();
// True when the touch hits the small Cancel button on the Mini-PoS QR screen
bool miniPosQrCancelHit(uint16_t x, uint16_t y);
void miniPosPaidScreen();
// Numerical product selection (Touch 3.5 multi-channel only):
// keypad panel, product QR with cancel button
void showProductSelectScreen();
void updateProductSelectBlockHeight();
// Returns: 0-9=digit, 10=backspace, 11=OK (confirm), 12=CANCEL, -1=no hit
int  productSelectHitTest(uint16_t x, uint16_t y);
void showProductSelectQRScreen(String label, int pin);
// True when the touch hits the small Cancel button on the product QR screen
bool productSelectQrCancelHit(uint16_t x, uint16_t y);
// Mode selection screen shown on boot when mode == "modeselect"
void showModeSelectionScreen();
// Returns: 1=Single, 2=Multi-channel, 3=Mini-PoS, 4=Authy, -1=no hit
int  modeSelectHitTest(uint16_t x, uint16_t y);
// Authy teach screen: registration QR + "Learning Identities" + CANCEL button
void showAuthTeachScreen(String label, int pin);
// Centered multi-line error overlay after PIN failure (size 3, over the QR area)
void showAuthPinError(const String &l1, const String &l2, const String &l3);
// Overlay a transient status toast on any auth screen (green=ok, red=error)
void showAuthToast(const String &msg, bool isError);
bool authTeachCancelHit(uint16_t x, uint16_t y);
// Authy dual-page: identity QR + payment page, bottom-left tab to switch pages
void showAuthIdentityScreen(String label, int pin);
void showAuthPayScreen(String label, int pin);
bool authTabHit(uint16_t x, uint16_t y);
// Red hint shown when the server reports Identities disabled (HTTP 403)
void authIdentityDisabledScreen();
#else
inline void showModeSelectionScreen() {}
inline int  modeSelectHitTest(uint16_t, uint16_t) { return -1; }
inline void showMiniPosInputScreen() {}
inline int  miniPosHitTest(uint16_t, uint16_t) { return -1; }
inline void showMiniPosQRScreen() {}
inline bool miniPosQrCancelHit(uint16_t, uint16_t) { return false; }
inline void miniPosPaidScreen() {}
inline void showProductSelectScreen() {}
inline void updateProductSelectBlockHeight() {}
inline int  productSelectHitTest(uint16_t, uint16_t) { return -1; }
inline void showProductSelectQRScreen(String, int) {}
inline bool productSelectQrCancelHit(uint16_t, uint16_t) { return false; }
inline void showAuthTeachScreen(String, int) {}
inline void showAuthPinError(const String &, const String &, const String &) {}
inline void showAuthToast(const String &, bool) {}
inline bool authTeachCancelHit(uint16_t, uint16_t) { return false; }
inline void showAuthIdentityScreen(String, int) {}
inline void showAuthPayScreen(String, int) {}
inline bool authTabHit(uint16_t, uint16_t) { return false; }
inline void authIdentityDisabledScreen() {}
#endif

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
inline void identityTriggerScreen() {}
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
inline void showProductQRScreen(String, int) {}
inline void showBoltCardScreen(String, int) {}
inline void showMobilePhoneScreen(String, int) {}
inline void productSelectionScreen() {}
inline void activateScreensaver(String) {}
inline void deactivateScreensaver() {}
inline bool isScreensaverActive() { return false; }
inline void prepareDeepSleep() {}
inline void setupDeepSleepWakeup(String) {}
inline bool isDeepSleepActive() { return false; }
inline void nfcTestScreen(String) {}
inline void showPinPadScreen(const PinPadState &) {}
inline int  pinPadHitTest(uint16_t, uint16_t) { return -1; }
inline void showModeSelectionScreen() {}
inline int  modeSelectHitTest(uint16_t, uint16_t) { return -1; }
inline void showMiniPosInputScreen() {}
inline int  miniPosHitTest(uint16_t, uint16_t) { return -1; }
inline void showMiniPosQRScreen() {}
inline bool miniPosQrCancelHit(uint16_t, uint16_t) { return false; }
inline void miniPosPaidScreen() {}
inline void showProductSelectScreen() {}
inline void updateProductSelectBlockHeight() {}
inline int  productSelectHitTest(uint16_t, uint16_t) { return -1; }
inline void showProductSelectQRScreen(String, int) {}
inline bool productSelectQrCancelHit(uint16_t, uint16_t) { return false; }
inline void showAuthTeachScreen(String, int) {}
inline void showAuthPinError(const String &, const String &, const String &) {}
inline void showAuthToast(const String &, bool) {}
inline bool authTeachCancelHit(uint16_t, uint16_t) { return false; }
inline void showAuthIdentityScreen(String, int) {}
inline void showAuthPayScreen(String, int) {}
inline bool authTabHit(uint16_t, uint16_t) { return false; }
inline void authIdentityDisabledScreen() {}

#endif // ENABLE_DISPLAY
