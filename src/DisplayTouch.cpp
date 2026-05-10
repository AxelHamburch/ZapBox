#ifdef BOARD_JC3248W535C

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <esp_heap_caps.h>
#include "Display.h"
#include "GlobalState.h"
#include "Log.h"

// ============================================================================
// PANEL & GEOMETRY
// ============================================================================

#define PIN_LCD_BL  LCD_BL_PIN
#define SCR_W       320
#define SCR_H       480

// ============================================================================
// COLORS — TFT_eSPI compatibility macros (RGB565)
// ============================================================================
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_BLUE        0x001F
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_ORANGE      0xFD20

uint16_t themeForeground = TFT_BLACK;
uint16_t themeBackground = TFT_WHITE;

// ============================================================================
// GFX HANDLES, BACKBUFFER & MUTEX
// ============================================================================

static Arduino_ESP32QSPI* _bus    = nullptr;
static Arduino_AXS15231B* _panel  = nullptr;
static Arduino_GFX*       _gfx    = nullptr;
static uint16_t*          s_backbuf = nullptr;     // PSRAM 320×480×2 = 300 KB
static SemaphoreHandle_t  displayMutex = nullptr;

class DisplayLock {
public:
  DisplayLock() : _locked(false) {
    if (displayMutex) {
      xSemaphoreTakeRecursive(displayMutex, portMAX_DELAY);
      _locked = true;
    }
  }
  ~DisplayLock() {
    if (_locked && displayMutex) xSemaphoreGiveRecursive(displayMutex);
  }
private:
  bool _locked;
};

// ============================================================================
// 5×7 BITMAP FONT (Adafruit GFX default — ASCII 0x20–0x7E)
// ============================================================================

static const uint8_t font5x7[][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
  {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},
};

// ============================================================================
// BACKBUFFER DRAWING — all primitives write to PSRAM, single flush() pushes
// ============================================================================

static inline void putPixel(int x, int y, uint16_t color) {
  if ((unsigned)x < (unsigned)SCR_W && (unsigned)y < (unsigned)SCR_H) {
    s_backbuf[y * SCR_W + x] = color;
  }
}

static void fillRect(int x, int y, int w, int h, uint16_t color) {
  if (!s_backbuf) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  if (w <= 0 || h <= 0) return;
  for (int row = 0; row < h; row++) {
    uint16_t *p = &s_backbuf[(y + row) * SCR_W + x];
    for (int col = 0; col < w; col++) p[col] = color;
  }
}

static void fillScreen(uint16_t color) {
  if (!s_backbuf) return;
  if (color == 0x0000) {
    memset(s_backbuf, 0x00, (size_t)SCR_W * SCR_H * 2);
  } else if (color == 0xFFFF) {
    memset(s_backbuf, 0xFF, (size_t)SCR_W * SCR_H * 2);
  } else {
    int n = SCR_W * SCR_H;
    for (int i = 0; i < n; i++) s_backbuf[i] = color;
  }
}

static void drawChar(int x, int y, char c, uint16_t fg, uint16_t bg, uint8_t size, bool transparent_bg) {
  if (c < 0x20 || c > 0x7E) c = '?';
  const uint8_t *glyph = font5x7[c - 0x20];

  for (int col = 0; col < 5; col++) {
    uint8_t line = pgm_read_byte(&glyph[col]);
    for (int row = 0; row < 7; row++) {
      bool on = (line >> row) & 1;
      if (on) {
        // Foreground block
        for (int dy = 0; dy < size; dy++)
          for (int dx = 0; dx < size; dx++)
            putPixel(x + col * size + dx, y + row * size + dy, fg);
      } else if (!transparent_bg) {
        for (int dy = 0; dy < size; dy++)
          for (int dx = 0; dx < size; dx++)
            putPixel(x + col * size + dx, y + row * size + dy, bg);
      }
    }
  }
  // Inter-character spacing column
  if (!transparent_bg) {
    for (int row = 0; row < 7; row++)
      for (int dy = 0; dy < size; dy++)
        for (int dx = 0; dx < size; dx++)
          putPixel(x + 5 * size + dx, y + row * size + dy, bg);
  }
}

static int drawString(int x, int y, const char *s, uint16_t fg, uint16_t bg, uint8_t size, bool transparent_bg = false) {
  while (*s) {
    drawChar(x, y, *s, fg, bg, size, transparent_bg);
    x += 6 * size;
    s++;
  }
  return x;
}

static void drawCenter(int cx, int cy, const char *s, uint16_t fg, uint16_t bg, uint8_t size) {
  int len = 0;
  for (const char *p = s; *p; p++) len++;
  int w = len * 6 * size;
  int h = 8 * size;
  drawString(cx - w / 2, cy - h / 2, s, fg, bg, size);
}

// Push the entire backbuffer to the panel in ONE call. The Arduino_GFX library
// sets one address window then streams all pixels — no fragmented writes, no
// cache games. This is the operation we proved works (full-screen fillScreen
// rendered solid colors correctly in earlier tests).
static void flushDisplay() {
  if (!_gfx || !s_backbuf) return;
  _gfx->draw16bitRGBBitmap(0, 0, s_backbuf, SCR_W, SCR_H);
}

// ============================================================================
// INIT
// ============================================================================

void initDisplayMutex() {
  if (!displayMutex) displayMutex = xSemaphoreCreateRecursiveMutex();
  Serial.println("[INFO][DISPLAY] Touch display mutex ready");
}

void initDisplay() {
  DisplayLock lock;

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);

  // 1. PSRAM-backed framebuffer (320×480×2 = 300 KB)
  size_t bb_bytes = (size_t)SCR_W * SCR_H * sizeof(uint16_t);
  s_backbuf = (uint16_t *)heap_caps_malloc(bb_bytes, MALLOC_CAP_SPIRAM);
  if (!s_backbuf) {
    Serial.println("[DISPLAY] FATAL: backbuffer alloc failed (PSRAM)");
    LOG_ERROR("DISPLAY", "backbuffer alloc failed");
    return;
  }
  memset(s_backbuf, 0, bb_bytes);
  Serial.printf("[DISPLAY] backbuffer allocated: %u bytes in PSRAM\n", (unsigned)bb_bytes);

  // 2. QSPI bus + AXS15231B panel via Arduino_GFX (proven to do real Quad-SPI)
  _bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK,
    LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3
  );
  _panel = new Arduino_AXS15231B(_bus, GFX_NOT_DEFINED, 0 /*rotation*/, false,
                                  SCR_W, SCR_H);
  _gfx = _panel;

  if (!_gfx->begin()) {
    Serial.println("[DISPLAY] ERROR: begin() failed!");
    LOG_ERROR("DISPLAY", "Arduino_GFX begin() failed");
    return;
  }
  Serial.println("[DISPLAY] begin() OK");

  // 3. Initial clear + backlight on
  themeForeground = TFT_BLACK;
  themeBackground = TFT_WHITE;
  fillScreen(themeBackground);
  flushDisplay();
  digitalWrite(PIN_LCD_BL, HIGH);

  LOG_INFO("DISPLAY", String("JC3248W535C ready ") + SCR_W + "x" + SCR_H);
}

// ============================================================================
// SCREENS — write to backbuffer, then flush
// ============================================================================

static void renderStatusBox(const char *label) {
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2, label, themeForeground, themeBackground, 4);
  flushDisplay();
}

void startupScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 50, "ZAPBOX",   themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "Firmware", themeForeground, themeBackground, 2);
  drawCenter(SCR_W / 2, SCR_H / 2 + 55, VERSION,    themeForeground, themeBackground, 2);
  flushDisplay();
}

void initializationScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 50, "ZAPBOX",          themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "Initializing...", themeForeground, themeBackground, 2);
  flushDisplay();
}

void configModeScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 60, "CONFIG",   themeForeground, themeBackground, 4);
  drawCenter(SCR_W / 2, SCR_H / 2,      "MODE",     themeForeground, themeBackground, 4);
  drawCenter(SCR_W / 2, SCR_H / 2 + 60, "(serial)", themeForeground, themeBackground, 2);
  flushDisplay();
}

void errorReportScreen(uint8_t w, uint8_t i, uint8_t s, uint8_t ws) {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, 60, "REPORT", themeForeground, themeBackground, 4);
  char line[32];
  snprintf(line, sizeof(line), "WiFi:%u  Net:%u",  w, i);
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, line, themeForeground, themeBackground, 2);
  snprintf(line, sizeof(line), "Srv:%u  WS:%u",   s, ws);
  drawCenter(SCR_W / 2, SCR_H / 2 + 10, line, themeForeground, themeBackground, 2);
  flushDisplay();
}

void wifiReconnectScreen()      { DisplayLock l; if (_gfx) renderStatusBox("NO WIFI"); }
void internetReconnectScreen()  { DisplayLock l; if (_gfx) renderStatusBox("NO NET"); }
void serverReconnectScreen()    { DisplayLock l; if (_gfx) renderStatusBox("NO SERVER"); }
void websocketReconnectScreen() { DisplayLock l; if (_gfx) renderStatusBox("NO SOCKET"); }
void bootUpScreen()             { DisplayLock l; if (_gfx) renderStatusBox("BOOT UP"); }

// ============================================================================
// STILL TO BE IMPLEMENTED (iteration 2+) — just blank for now
// ============================================================================

static void blankScreen() {
  DisplayLock l;
  if (_gfx) { fillScreen(themeBackground); flushDisplay(); }
}

void btctickerScreen()        { blankScreen(); }
void updateBtctickerValues()  { /* TODO */ }
void stepOneScreen()          { blankScreen(); }
void stepTwoScreen()          { blankScreen(); }
void stepThreeScreen()        { blankScreen(); }
void actionTimeScreen()       { blankScreen(); }
void updateActionTimeCountdown(int) { /* TODO */ }
void nfcPendingScreen()       { blankScreen(); }
void nfcNoLuckScreen()        { blankScreen(); }
void nfcNotSupportedScreen()  { blankScreen(); }
void nfcErrorDetailScreen(const char*) { blankScreen(); }
void thankYouScreen()         { blankScreen(); }
void productBlockedScreen()   { blankScreen(); }
void supplyBinEmptyScreen()   { blankScreen(); }
void drawQRCode()             { /* TODO */ }
void showQRScreen()           { blankScreen(); }
void showThresholdQRScreen()  { blankScreen(); }
void showSpecialModeQRScreen(){ blankScreen(); }
void showProductQRScreen(String, int)    { blankScreen(); }
void showBoltCardScreen(String, int)     { blankScreen(); }
void showMobilePhoneScreen(String, int)  { blankScreen(); }
void productSelectionScreen() { blankScreen(); }
void activateScreensaver(String) { /* TODO */ }
void deactivateScreensaver()  { /* TODO */ }
bool isScreensaverActive()    { return false; }
void prepareDeepSleep()       { /* TODO */ }
void setupDeepSleepWakeup(String) { /* TODO */ }
bool isDeepSleepActive()      { return false; }
void nfcTestScreen(String)    { blankScreen(); }

#endif  // BOARD_JC3248W535C
