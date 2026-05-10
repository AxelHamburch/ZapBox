#ifdef BOARD_JC3248W535C

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "Display.h"
#include "Log.h"

// ============================================================================
// GLOBALS & TYPES
// ============================================================================

#define PIN_LCD_BL         LCD_BL_PIN
#define SCR_W              320
#define SCR_H              480

static Arduino_ESP32QSPI *_bus = nullptr;
static Arduino_AXS15231B *_panel = nullptr;
static Arduino_GFX *_gfx = nullptr;
static SemaphoreHandle_t displayMutex = nullptr;

// Theme colors
uint32_t themeBackground = 0x000000;
uint32_t themeForeground = 0xFFFFFF;
uint32_t themeAccent = 0xFF9500;

// ============================================================================
// RAII DISPLAY LOCK
// ============================================================================

class DisplayLock {
public:
  DisplayLock() : _locked(false) {
    if (displayMutex) {
      xSemaphoreTakeRecursive(displayMutex, portMAX_DELAY);
      _locked = true;
    }
  }
  
  ~DisplayLock() {
    if (_locked && displayMutex)
      xSemaphoreGiveRecursive(displayMutex);
  }
private:
  bool _locked;
};

// ============================================================================
// INITIALIZATION
// ============================================================================

void initDisplayMutex() {
  if (!displayMutex) {
    displayMutex = xSemaphoreCreateRecursiveMutex();
  }
  Serial.println("[INFO][DISPLAY] Touch display mutex ready");
}

void initDisplay() {
  DisplayLock lock;

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);

  _bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS,   // 45 = cs
    LCD_QSPI_CLK,  // 47 = sck
    LCD_QSPI_D0,   // 21 = mosi (D0)
    LCD_QSPI_D1,   // 48 = miso (D1)
    LCD_QSPI_D2,   // 40 = quadwp (D2)
    LCD_QSPI_D3    // 39 = quadhd (D3)
  );

  _panel = new Arduino_AXS15231B(_bus, GFX_NOT_DEFINED, 0, false, 320, 480);
  _gfx = _panel;

  if (!_gfx->begin()) {
    Serial.println("[DISPLAY] ERROR: begin() failed!");
    LOG_ERROR("DISPLAY", "Arduino_GFX begin() failed");
    return;
  }
  Serial.println("[DISPLAY] begin() OK");

  _gfx->fillScreen(0x0000);
  digitalWrite(PIN_LCD_BL, HIGH);
  LOG_INFO("DISPLAY", String("JC3248W535C ready ") + SCR_W + "x" + SCR_H);
}

// ============================================================================
// BASIC SCREEN FUNCTIONS (minimal implementations)
// ============================================================================

void startupScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);  // Black
  }
}

void btctickerScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void updateBtctickerValues() {
  DisplayLock lock;
  if (_gfx) {
    // Placeholder
  }
}

void initializationScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void bootUpScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void configModeScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void errorReportScreen(uint8_t w, uint8_t i, uint8_t s, uint8_t ws) {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0xF800);  // Red
  }
}

void wifiReconnectScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void internetReconnectScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void serverReconnectScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void websocketReconnectScreen() {
  DisplayLock lock;
  if (_gfx) {
    _gfx->fillScreen(0x0000);
  }
}

void stepOneScreen() {}
void stepTwoScreen() {}
void stepThreeScreen() {}
void actionTimeScreen() {}
void updateActionTimeCountdown(int r) {}
void nfcPendingScreen() {}
void nfcNoLuckScreen() {}
void nfcNotSupportedScreen() {}
void nfcErrorDetailScreen(const char* d) {}
void thankYouScreen() {}
void productBlockedScreen() {}
void supplyBinEmptyScreen() {}
void drawQRCode() {}
void showQRScreen() {}
void showThresholdQRScreen() {}
void showSpecialModeQRScreen() {}
void showProductQRScreen(String l, int p) {}
void showBoltCardScreen(String l, int p) {}
void showMobilePhoneScreen(String l, int p) {}
void productSelectionScreen() {}
void activateScreensaver(String m) {}
void deactivateScreensaver() {}
bool isScreensaverActive() { return false; }
void prepareDeepSleep() {}
void setupDeepSleepWakeup(String m) {}
bool isDeepSleepActive() { return false; }
void nfcTestScreen(String l) {}

#endif  // BOARD_JC3248W535C
