#ifdef BOARD_JC3248W535C

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>
#include <qrcode.h>
#include "Display.h"
#include "GlobalState.h"
#include "Log.h"

// ============================================================================
// PANEL & GEOMETRY
// ============================================================================

#define PIN_LCD_BL  LCD_BL_PIN
// We expose a LANDSCAPE 480×320 canvas to the rest of the code (matches the
// T-Display-S3 layout convention used in the original ZapBox screens). The
// physical panel is 320×480 portrait — software rotation in putPixel maps
// our landscape coordinates to portrait pixels (90° CCW so the user holds
// the device with its native top edge on the LEFT).
#define SCR_W       480
#define SCR_H       320
#define PANEL_W     320
#define PANEL_H     480

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
#define TFT_DARKGREY    0x7BEF
#define TFT_LIGHTGREY   0xC618
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_PINK        0xFE19
#define TFT_GREENYELLOW 0xAFE5
#define TFT_BROWN       0x9A60

uint16_t themeForeground = TFT_BLACK;
uint16_t themeBackground = TFT_WHITE;

// Special non-standard accent colors used by some themes
#define COLOR_BTCORANGE    0xFCC0
#define COLOR_ZAPBOX_AMBER 0xFEA0

// ============================================================================
// THEME LOOKUP — 24 foreground/background combinations selectable via
// displayConfig.theme (loaded from FFat config). Linear search at startup
// is plenty fast for 24 entries.
// ============================================================================

struct ThemeConfig {
  const char *name;
  uint16_t    foreground;
  uint16_t    background;
};

static const ThemeConfig themeConfigs[] = {
  {"black-white",         TFT_BLACK,     TFT_WHITE},
  {"black-darkcyan",      TFT_BLACK,     TFT_DARKCYAN},
  {"darkgreen-green",     TFT_DARKGREEN, TFT_GREEN},
  {"darkgreen-lightgrey", TFT_DARKGREEN, TFT_LIGHTGREY},
  {"darkblue-lightgrey",  TFT_NAVY,      TFT_LIGHTGREY},
  {"red-green",           TFT_RED,       TFT_GREEN},
  {"black-blue",          TFT_BLACK,     TFT_BLUE},
  {"orange-brown",        TFT_ORANGE,    TFT_BROWN},
  {"black-yellow",        TFT_BLACK,     TFT_YELLOW},
  {"black-btcorange",     TFT_BLACK,     COLOR_BTCORANGE},
  {"btcorange-black",     COLOR_BTCORANGE, TFT_BLACK},
  {"darkgrey-btcorange",  TFT_DARKGREY,  COLOR_BTCORANGE},
  {"zapbox",              COLOR_ZAPBOX_AMBER, TFT_BLACK},
  {"maroon-magenta",      TFT_MAROON,    TFT_MAGENTA},
  {"black-red",           TFT_BLACK,     TFT_RED},
  {"brown-orange",        TFT_BROWN,     TFT_ORANGE},
  {"black-orange",        TFT_BLACK,     TFT_ORANGE},
  {"white-darkcyan",      TFT_WHITE,     TFT_DARKCYAN},
  {"white-navy",          TFT_WHITE,     TFT_NAVY},
  {"darkcyan-cyan",       TFT_DARKCYAN,  TFT_CYAN},
  {"black-olive",         TFT_BLACK,     TFT_OLIVE},
  {"black-darkgrey",      TFT_BLACK,     TFT_DARKGREY},
  {"black-lightgrey",     TFT_BLACK,     TFT_LIGHTGREY},
  {"black-green",         TFT_BLACK,     TFT_GREEN},
};

static void setThemeColors() {
  // Default fallback if displayConfig.theme name doesn't match the table
  themeForeground = TFT_BLACK;
  themeBackground = TFT_WHITE;
  for (const auto &t : themeConfigs) {
    if (displayConfig.theme == t.name) {
      themeForeground = t.foreground;
      themeBackground = t.background;
      return;
    }
  }
}

// External currency string set from config (defaults to "USD")
extern String currency;

// ============================================================================
// SHARED LAYOUT CONSTANTS (used by QR / step / NFC screens)
// ============================================================================
// Left half: 245×245 area (10..255, 35..280) — for QR or big icons/text
// Right half: 200×250 colored info box (270..470, 35..285)
#define QR_X        10
#define QR_Y        35
#define QR_MOD_SIZE 5     // 49 modules × 5 px = 245 px
#define QR_AREA_CX  ((QR_X) + (49 * QR_MOD_SIZE) / 2)   // 132
#define QR_AREA_CY  ((QR_Y) + (49 * QR_MOD_SIZE) / 2)   // 157

#define BOX_X       270
#define BOX_Y       35
#define BOX_W       200
#define BOX_H       250

// ============================================================================
// BITCOIN LOGO — 64×64 monochrome bitmap (from original Display.cpp)
// ============================================================================
static const uint8_t bitcoin_logo[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x3f,0xfc,0x00,0x00,0x00, 0x00,0x00,0x03,0xff,0xff,0x80,0x00,0x00,
  0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00, 0x00,0x00,0x3f,0xff,0xff,0xfc,0x00,0x00,
  0x00,0x00,0x7f,0xff,0xff,0xfe,0x00,0x00, 0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
  0x00,0x03,0xff,0xff,0xff,0xff,0xc0,0x00, 0x00,0x07,0xff,0xff,0xff,0xff,0xe0,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00, 0x00,0x0f,0xff,0xfc,0x7f,0xff,0xf0,0x00,
  0x00,0x1f,0xff,0xfc,0x63,0xff,0xf8,0x00, 0x00,0x3f,0xff,0xfc,0x63,0xff,0xfc,0x00,
  0x00,0x7f,0xfe,0x38,0xe3,0xff,0xfe,0x00, 0x00,0x7f,0xfe,0x00,0xe3,0xff,0xfe,0x00,
  0x00,0xff,0xfe,0x00,0x03,0xff,0xff,0x00, 0x00,0xff,0xff,0x80,0x03,0xff,0xff,0x00,
  0x00,0xff,0xff,0xc0,0x00,0xff,0xff,0x80, 0x01,0xff,0xff,0xc0,0x00,0x7f,0xff,0x80,
  0x01,0xff,0xff,0xc1,0xe0,0x3f,0xff,0x80, 0x01,0xff,0xff,0x81,0xf8,0x1f,0xff,0x80,
  0x03,0xff,0xff,0x83,0xf8,0x1f,0xff,0xc0, 0x03,0xff,0xff,0x83,0xf8,0x1f,0xff,0xc0,
  0x03,0xff,0xff,0x83,0xf8,0x1f,0xff,0xc0, 0x03,0xff,0xff,0x01,0xf0,0x1f,0xff,0xc0,
  0x03,0xff,0xff,0x00,0x00,0x3f,0xff,0xc0, 0x03,0xff,0xff,0x00,0x00,0x7f,0xff,0xc0,
  0x03,0xff,0xff,0x06,0x00,0xff,0xff,0xc0, 0x03,0xff,0xfe,0x07,0xc0,0x7f,0xff,0xc0,
  0x03,0xff,0xfe,0x0f,0xe0,0x3f,0xff,0xc0, 0x03,0xff,0xfe,0x0f,0xf0,0x3f,0xff,0xc0,
  0x03,0xff,0xec,0x0f,0xf0,0x3f,0xff,0xc0, 0x03,0xff,0xe0,0x0f,0xf0,0x3f,0xff,0xc0,
  0x01,0xff,0xc0,0x0f,0xf0,0x3f,0xff,0x80, 0x01,0xff,0xc0,0x00,0x00,0x3f,0xff,0x80,
  0x01,0xff,0xf8,0x00,0x00,0x7f,0xff,0x80, 0x01,0xff,0xfe,0x00,0x00,0x7f,0xff,0x00,
  0x00,0xff,0xfe,0x30,0x00,0xff,0xff,0x00, 0x00,0xff,0xfe,0x38,0xc7,0xff,0xff,0x00,
  0x00,0x7f,0xfe,0x31,0xff,0xff,0xfe,0x00, 0x00,0x7f,0xfc,0x31,0xff,0xff,0xfe,0x00,
  0x00,0x3f,0xff,0xf1,0xff,0xff,0xfc,0x00, 0x00,0x1f,0xff,0xf1,0xff,0xff,0xf8,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00, 0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x07,0xff,0xff,0xff,0xff,0xe0,0x00, 0x00,0x03,0xff,0xff,0xff,0xff,0xc0,0x00,
  0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00, 0x00,0x00,0x7f,0xff,0xff,0xfe,0x00,0x00,
  0x00,0x00,0x3f,0xff,0xff,0xfc,0x00,0x00, 0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,
  0x00,0x00,0x01,0xff,0xff,0xc0,0x00,0x00, 0x00,0x00,0x00,0x3f,0xfc,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

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

// 90° CCW rotation: landscape (lx, ly) → portrait (PANEL_W-1-ly, lx)
// Caller passes landscape coords (0..SCR_W-1, 0..SCR_H-1).
// Buffer is laid out as portrait (PANEL_W cols × PANEL_H rows).
static inline void putPixel(int lx, int ly, uint16_t color) {
  if ((unsigned)lx >= (unsigned)SCR_W || (unsigned)ly >= (unsigned)SCR_H) return;
  int px = PANEL_W - 1 - ly;
  int py = lx;
  s_backbuf[py * PANEL_W + px] = color;
}

static void fillRect(int lx, int ly, int lw, int lh, uint16_t color) {
  if (!s_backbuf) return;
  // Clip in landscape space
  if (lx < 0) { lw += lx; lx = 0; }
  if (ly < 0) { lh += ly; ly = 0; }
  if (lx + lw > SCR_W) lw = SCR_W - lx;
  if (ly + lh > SCR_H) lh = SCR_H - ly;
  if (lw <= 0 || lh <= 0) return;

  // Rotate rect to portrait: 90° CCW maps width↔height and shifts origin
  int px = PANEL_W - ly - lh;
  int py = lx;
  int pw = lh;     // landscape height → portrait width
  int ph = lw;     // landscape width  → portrait height

  for (int row = 0; row < ph; row++) {
    uint16_t *p = &s_backbuf[(py + row) * PANEL_W + px];
    for (int col = 0; col < pw; col++) p[col] = color;
  }
}

static void fillScreen(uint16_t color) {
  if (!s_backbuf) return;
  size_t total_bytes = (size_t)PANEL_W * PANEL_H * 2;
  if (color == 0x0000) {
    memset(s_backbuf, 0x00, total_bytes);
  } else if (color == 0xFFFF) {
    memset(s_backbuf, 0xFF, total_bytes);
  } else {
    int n = PANEL_W * PANEL_H;
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

// Draw a 1-bit-per-pixel bitmap (Adafruit format: MSB first per row, byte
// boundaries). Foreground bits become `fg`, background bits stay untouched
// (transparent overlay onto the existing backbuffer).
static void drawMonoBitmap(int x, int y, const uint8_t *bmp, int w, int h, uint16_t fg) {
  if (!s_backbuf) return;
  int row_bytes = (w + 7) / 8;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byte = pgm_read_byte(&bmp[j * row_bytes + i / 8]);
      if (byte & (0x80 >> (i & 7))) {
        putPixel(x + i, y + j, fg);
      }
    }
  }
}

// Same as drawMonoBitmap but each source pixel is plotted as a `scale × scale`
// block, useful for upscaling small icons (e.g. 64×64 logo → 128×128).
static void drawMonoBitmapScaled(int x, int y, const uint8_t *bmp, int w, int h,
                                  uint16_t fg, uint8_t scale) {
  if (!s_backbuf || scale == 0) return;
  int row_bytes = (w + 7) / 8;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byte = pgm_read_byte(&bmp[j * row_bytes + i / 8]);
      if (byte & (0x80 >> (i & 7))) {
        for (int dy = 0; dy < scale; dy++) {
          for (int dx = 0; dx < scale; dx++) {
            putPixel(x + i * scale + dx, y + j * scale + dy, fg);
          }
        }
      }
    }
  }
}

// Push the entire backbuffer to the panel in ONE call. The Arduino_GFX library
// sets one address window then streams all pixels — no fragmented writes, no
// cache games. The buffer is in portrait (PANEL_W × PANEL_H) layout because
// the panel hardware addresses pixels in portrait native order.
static void flushDisplay() {
  if (!_gfx || !s_backbuf) return;
  _gfx->draw16bitRGBBitmap(0, 0, s_backbuf, PANEL_W, PANEL_H);
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

  // 1. PSRAM-backed framebuffer (laid out as the panel sees it: portrait)
  size_t bb_bytes = (size_t)PANEL_W * PANEL_H * sizeof(uint16_t);
  s_backbuf = (uint16_t *)heap_caps_malloc(bb_bytes, MALLOC_CAP_SPIRAM);
  if (!s_backbuf) {
    Serial.println("[DISPLAY] FATAL: backbuffer alloc failed (PSRAM)");
    LOG_ERROR("DISPLAY", "backbuffer alloc failed");
    return;
  }
  memset(s_backbuf, 0, bb_bytes);
  Serial.printf("[DISPLAY] backbuffer allocated: %u bytes in PSRAM\n", (unsigned)bb_bytes);

  // 2. QSPI bus + AXS15231B panel via Arduino_GFX (proven to do real Quad-SPI).
  // The panel is constructed with its NATIVE portrait dimensions (320×480);
  // landscape orientation is achieved by software rotation in putPixel.
  _bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK,
    LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3
  );
  _panel = new Arduino_AXS15231B(_bus, GFX_NOT_DEFINED, 0 /*rotation*/, false,
                                  PANEL_W, PANEL_H);
  _gfx = _panel;

  if (!_gfx->begin()) {
    Serial.println("[DISPLAY] ERROR: begin() failed!");
    LOG_ERROR("DISPLAY", "Arduino_GFX begin() failed");
    return;
  }
  Serial.println("[DISPLAY] begin() OK");

  // 3. Apply configured theme + initial clear + backlight on
  setThemeColors();
  Serial.printf("[DISPLAY] Theme '%s' fg=0x%04X bg=0x%04X\n",
                displayConfig.theme.c_str(), themeForeground, themeBackground);
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
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, "ZAPBOX",   themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "Firmware", themeForeground, themeBackground, 2);
  drawCenter(SCR_W / 2, SCR_H / 2 + 55, VERSION,    themeForeground, themeBackground, 2);
  flushDisplay();
}

void initializationScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, "ZAPBOX",          themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "Initializing...", themeForeground, themeBackground, 2);
  flushDisplay();
}

void configModeScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 30, "CONFIG",   themeForeground, themeBackground, 4);
  drawCenter(SCR_W / 2, SCR_H / 2 + 20, "MODE",     themeForeground, themeBackground, 4);
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

// ============================================================================
// BTC TICKER — T-Display-S3 horizontal layout (logo left, three text rows right)
// ============================================================================
// Layout on 480×320 landscape:
//   x=  20..148  Bitcoin logo (64×64 ×2 = 128×128, vertically centered)
//   x= 165..     Right-side text section (centered around cx=320)
//     y= 110     "<currency>/BTC: <price>"        (size 2)
//     y= 160     "SAT/<currency>: <sats>"         (size 2)
//     y= 210     "Block: <block>"                 (size 2)
//
// updateBtctickerValues redraws only the right-side text rows so the logo
// doesn't flicker every refresh.

static const int BTC_TXT_CX     = 320;        // horizontal center of text column
static const int BTC_LINE1_Y    = 110;
static const int BTC_LINE2_Y    = 160;
static const int BTC_LINE3_Y    = 210;
static const int BTC_TXT_X      = 160;        // left edge of text clear band
static const int BTC_TXT_W      = 320;        // width of text clear band
static const int BTC_TXT_BAND_H = 28;         // height per cleared row

static String calcSatsPerCurrency() {
  float price = bitcoinData.price.toFloat();
  if (price <= 0) return String("0");
  long sats = (long)((1.0f / price) * 100000000.0f);
  return String(sats);
}

static void btcDrawTextLines() {
  String s;
  s = currency + "/BTC: " + bitcoinData.price;
  drawCenter(BTC_TXT_CX, BTC_LINE1_Y, s.c_str(),
             themeForeground, themeBackground, 2);
  s = "SAT/" + currency + ": " + calcSatsPerCurrency();
  drawCenter(BTC_TXT_CX, BTC_LINE2_Y, s.c_str(),
             themeForeground, themeBackground, 2);
  s = "Block: " + bitcoinData.blockHigh;
  drawCenter(BTC_TXT_CX, BTC_LINE3_Y, s.c_str(),
             themeForeground, themeBackground, 2);
}

void btctickerScreen() {
  DisplayLock l;
  if (!_gfx) return;

  fillScreen(themeBackground);
  // Bitcoin logo on the left, vertically centered (logo is 128×128 after 2x scale)
  drawMonoBitmapScaled(20, (SCR_H - 128) / 2, bitcoin_logo, 64, 64,
                       themeForeground, 2);
  btcDrawTextLines();
  flushDisplay();
}

void updateBtctickerValues() {
  DisplayLock l;
  if (!_gfx) return;

  // Wipe just the three text rows (right side), leave logo intact
  fillRect(BTC_TXT_X, BTC_LINE1_Y - BTC_TXT_BAND_H / 2, BTC_TXT_W, BTC_TXT_BAND_H, themeBackground);
  fillRect(BTC_TXT_X, BTC_LINE2_Y - BTC_TXT_BAND_H / 2, BTC_TXT_W, BTC_TXT_BAND_H, themeBackground);
  fillRect(BTC_TXT_X, BTC_LINE3_Y - BTC_TXT_BAND_H / 2, BTC_TXT_W, BTC_TXT_BAND_H, themeBackground);
  btcDrawTextLines();
  flushDisplay();
}
// ============================================================================
// STEP / FLOW SCREENS — big number left + label box right (T-Display-S3 style)
// ============================================================================
// Reuses the BOX_X/Y/W/H geometry from the QR screens for a consistent layout.

static void renderStepScreen(const char *big, uint8_t bigSize,
                              const char *l1, const char *l2, const char *l3) {
  fillScreen(themeBackground);
  // Big number/text on the left (center of the QR area for visual consistency)
  drawCenter(QR_AREA_CX, QR_AREA_CY, big,
             themeForeground, themeBackground, bigSize);
  // Filled label box on the right with three lines
  fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, themeForeground);
  int cx = BOX_X + BOX_W / 2;
  drawCenter(cx, BOX_Y + BOX_H / 4,     l1, themeBackground, themeForeground, 4);
  drawCenter(cx, BOX_Y + BOX_H / 2,     l2, themeBackground, themeForeground, 4);
  drawCenter(cx, BOX_Y + 3 * BOX_H / 4, l3, themeBackground, themeForeground, 4);
  flushDisplay();
}

void stepOneScreen() {
  DisplayLock l; if (!_gfx) return;
  renderStepScreen("1", 14, "SELECT", "YOUR", "PRODUCT");
}

void stepTwoScreen() {
  DisplayLock l; if (!_gfx) return;
  renderStepScreen("2", 14, "SCAN", "QR", "CODE");
}

void stepThreeScreen() {
  DisplayLock l; if (!_gfx) return;
  renderStepScreen("3", 14, "PAY", "IN-", "VOICE");
}

// ============================================================================
// ACTION TIME — "ACTION" big at top, "TIME" in inverted box, countdown sides
// ============================================================================
// Layout on 480×320:
//   y= 85    "ACTION"           size 6 (centered)
//   y=130..210  TIME box (220×80, x=130..350)
//      y=170  "TIME" inside box, size 5
//   countdown:
//      MM at (cx=60, cy=170)   size 4   (left of box)
//      SS at (cx=420, cy=170)  size 4   (right of box)

#define AT_BOX_X    130
#define AT_BOX_Y    130
#define AT_BOX_W    220
#define AT_BOX_H    80
#define AT_LABEL_Y  170
#define AT_MM_CX    60
#define AT_SS_CX    420

void actionTimeScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, 85, "ACTION", themeForeground, themeBackground, 6);
  fillRect(AT_BOX_X, AT_BOX_Y, AT_BOX_W, AT_BOX_H, themeForeground);
  drawCenter(SCR_W / 2, AT_LABEL_Y, "TIME",
             themeBackground, themeForeground, 5);
  flushDisplay();
}

void updateActionTimeCountdown(int remainingSecs) {
  DisplayLock l; if (!_gfx) return;
  if (remainingSecs < 0) remainingSecs = 0;
  int mins = remainingSecs / 60;
  int secs = remainingSecs % 60;
  if (mins > 99) mins = 99;

  char buf[8];
  // Wipe just the two countdown bands (avoid touching the TIME box)
  fillRect(AT_MM_CX - 40, AT_LABEL_Y - 22, 80, 44, themeBackground);
  fillRect(AT_SS_CX - 40, AT_LABEL_Y - 22, 80, 44, themeBackground);

  snprintf(buf, sizeof(buf), "%02d", mins);
  drawCenter(AT_MM_CX, AT_LABEL_Y, buf, themeForeground, themeBackground, 4);
  snprintf(buf, sizeof(buf), "%02d", secs);
  drawCenter(AT_SS_CX, AT_LABEL_Y, buf, themeForeground, themeBackground, 4);
  flushDisplay();
}
// ============================================================================
// NFC SCREENS — pending / no luck / not supported / error detail
// ============================================================================
// Shared layout: "label" big at top, "NFC" in inverted-color box mid/below.
// Same box geometry as actionTimeScreen for visual consistency.

#define NFC_BOX_X   130
#define NFC_BOX_Y   130
#define NFC_BOX_W   220
#define NFC_BOX_H   80
#define NFC_BOX_LBL_Y 170     // y-center of "NFC" inside the box

void nfcPendingScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, 70, "PENDING",
             themeForeground, themeBackground, 6);
  fillRect(NFC_BOX_X, NFC_BOX_Y, NFC_BOX_W, NFC_BOX_H, themeForeground);
  drawCenter(SCR_W / 2, NFC_BOX_LBL_Y, "NFC",
             themeBackground, themeForeground, 5);
  flushDisplay();
}

void nfcNoLuckScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  // NFC box at TOP this time (smaller)
  fillRect(NFC_BOX_X, 50, NFC_BOX_W, NFC_BOX_H, themeForeground);
  drawCenter(SCR_W / 2, 90, "NFC",
             themeBackground, themeForeground, 5);
  // "NO LUCK" big below
  drawCenter(SCR_W / 2, 220, "NO LUCK",
             themeForeground, themeBackground, 6);
  flushDisplay();
}

void nfcNotSupportedScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  fillRect(NFC_BOX_X, 50, NFC_BOX_W, NFC_BOX_H, themeForeground);
  drawCenter(SCR_W / 2, 90, "NFC",
             themeBackground, themeForeground, 5);
  drawCenter(SCR_W / 2, 200, "not supported",
             themeForeground, themeBackground, 3);
  drawCenter(SCR_W / 2, 255, "use zapbox extension",
             themeForeground, themeBackground, 2);
  flushDisplay();
}

void nfcErrorDetailScreen(const char *detail) {
  DisplayLock l; if (!_gfx) return;
  if (!detail) detail = "";
  fillScreen(themeBackground);

  // Word-wrap into up to 4 lines at text size 2 (12 px per char wide)
  const int textSize = 2;
  const int charW    = 6 * textSize;            // 12
  const int lineH    = 8 * textSize + 8;        // 24 (with extra spacing)
  const int margin   = 10;
  const int maxW     = SCR_W - 2 * margin;
  const int maxChars = maxW / charW;            // ~38

  String full(detail);
  String lines[4];
  int lineCount = 0;

  // Split on first ": " so the label keeps its own line (server convention)
  int colonSplit = full.indexOf(": ");
  String remaining;
  if (colonSplit >= 0) {
    lines[lineCount++] = full.substring(0, colonSplit + 1);
    remaining = full.substring(colonSplit + 2);
  } else {
    remaining = full;
  }

  while (remaining.length() > 0 && lineCount < 4) {
    remaining.trim();
    if ((int)remaining.length() <= maxChars) {
      lines[lineCount++] = remaining;
      break;
    }
    // Look for the last space within maxChars chars
    int breakAt = -1;
    for (int i = 0; i < maxChars && i < (int)remaining.length(); i++) {
      if (remaining.charAt(i) == ' ') breakAt = i;
    }
    if (breakAt <= 0) {
      // Hard break — no space fits, slice at maxChars
      lines[lineCount++] = remaining.substring(0, maxChars);
      remaining = remaining.substring(maxChars);
    } else {
      lines[lineCount++] = remaining.substring(0, breakAt);
      remaining = remaining.substring(breakAt + 1);
    }
  }

  int totalH = lineCount * lineH;
  int startY = (SCR_H - totalH) / 2 + lineH / 2;
  for (int i = 0; i < lineCount; i++) {
    drawCenter(SCR_W / 2, startY + i * lineH, lines[i].c_str(),
               themeForeground, themeBackground, textSize);
  }
  flushDisplay();
}
void thankYouScreen() {
  DisplayLock l; if (!_gfx) return;
  renderStepScreen("ty", 14, "ENJOY", "YOUR", "DAY");
}

// ============================================================================
// WARNING SCREENS — warm amber background (0xFBE0), black text, full canvas
// ============================================================================

#define WARN_BG  0xFBE0   // warm amber
#define WARN_FG  TFT_BLACK

static void renderWarningScreen(const char *l1_big, const char *l2_big,
                                 const char *l3_small) {
  fillScreen(WARN_BG);
  drawCenter(SCR_W / 2, 90,  l1_big,    WARN_FG, WARN_BG, 4);
  drawCenter(SCR_W / 2, 145, l2_big,    WARN_FG, WARN_BG, 4);
  drawCenter(SCR_W / 2, 215, l3_small,  WARN_FG, WARN_BG, 2);
  flushDisplay();
}

void productBlockedScreen() {
  DisplayLock l; if (!_gfx) return;
  renderWarningScreen("PRODUCT", "BLOCKED", "Remove the product");
}

void supplyBinEmptyScreen() {
  DisplayLock l; if (!_gfx) return;
  renderWarningScreen("SUPPLY BIN", "IS EMPTY", "Please restock it");
}
// ============================================================================
// QR / PRODUCT SCREENS — landscape 480×320 layout
// ============================================================================
// Left side: 245×245 QR (49 modules × 5 px) at (10, 35)
// Right side: 200×250 colored info box at (270, 35) with up to 3 lines text
// For BoltCard/MobilePhone variants the QR area shows text instead of a QR.

static String sanitizeLabel(String label) {
  // QR/box font supports only ASCII — replace common currency symbols.
  label.replace("€", "EUR");  // €
  label.replace("$",      "USD");
  label.replace("£", "GBP");  // £
  label.replace("¥", "YEN");  // ¥
  label.replace("₿", "BTC");  // ₿
  label.replace("₹", "INR");  // ₹
  label.replace("₽", "RUB");  // ₽
  label.replace("¢", "ct");   // ¢
  return label;
}

// Split into up to 3 words: 1st word, 2nd word, rest. Falls back to "Pin <n>"
// if input is empty.
static void splitLabelWords(const String &src, int pin,
                             String words[3], int &wordCount) {
  words[0] = words[1] = words[2] = "";
  wordCount = 0;
  int firstSpace = src.indexOf(' ');
  if (firstSpace < 0) {
    words[0] = src;
    wordCount = (src.length() > 0) ? 1 : 0;
  } else {
    words[0] = src.substring(0, firstSpace);
    wordCount = 1;
    int secondSpace = src.indexOf(' ', firstSpace + 1);
    if (secondSpace < 0) {
      words[1] = src.substring(firstSpace + 1);
      wordCount = 2;
    } else {
      words[1] = src.substring(firstSpace + 1, secondSpace);
      words[2] = src.substring(secondSpace + 1);
      wordCount = 3;
    }
  }
  if (wordCount == 0 || words[0].length() == 0) {
    words[0] = "Pin " + String(pin);
    wordCount = 1;
  }
}

// Render a QR code (text data) into the backbuffer. Uses qrcode library version 8
// (49×49 modules) which fits typical Lightning URLs.
static void drawQRAt(const char *text, int lx, int ly, int mod_size,
                     uint16_t fg, uint16_t bg) {
  if (!text || !*text) return;
  QRCode qr;
  uint8_t qrBuf[qrcode_getBufferSize(8)];
  if (qrcode_initText(&qr, qrBuf, 8, 0, text) < 0) {
    // Fallback: just fill the area with bg so we don't show stale pixels
    fillRect(lx, ly, qr.size * mod_size, qr.size * mod_size, bg);
    return;
  }
  for (int yy = 0; yy < qr.size; yy++) {
    for (int xx = 0; xx < qr.size; xx++) {
      uint16_t c = qrcode_getModule(&qr, xx, yy) ? fg : bg;
      fillRect(lx + xx * mod_size, ly + yy * mod_size, mod_size, mod_size, c);
    }
  }
}

// Filled label box on the right side, text in box-bg color, centered
static void drawLabelBox(const String words[], int wordCount,
                          uint16_t box_color, uint16_t text_color) {
  fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, box_color);
  int cx = BOX_X + BOX_W / 2;
  if (wordCount <= 1) {
    int sz = (words[0].length() >= 7) ? 2 : 4;
    drawCenter(cx, BOX_Y + BOX_H / 2, words[0].c_str(),
               text_color, box_color, sz);
  } else if (wordCount == 2) {
    int sz1 = (words[0].length() >= 7) ? 2 : 3;
    int sz2 = (words[1].length() >= 7) ? 2 : 3;
    drawCenter(cx, BOX_Y + BOX_H / 3,     words[0].c_str(),
               text_color, box_color, sz1);
    drawCenter(cx, BOX_Y + 2 * BOX_H / 3, words[1].c_str(),
               text_color, box_color, sz2);
  } else {
    int sz1 = (words[0].length() >= 7) ? 2 : 3;
    int sz2 = (words[1].length() >= 7) ? 2 : 3;
    drawCenter(cx, BOX_Y + BOX_H / 4,     words[0].c_str(),
               text_color, box_color, sz1);
    drawCenter(cx, BOX_Y + BOX_H / 2,     words[1].c_str(),
               text_color, box_color, sz2);
    drawCenter(cx, BOX_Y + 3 * BOX_H / 4, words[2].c_str(),
               text_color, box_color, 2);
  }
}

void drawQRCode() {
  // External callers expect this draws lightningConfig.lightning at the
  // standard QR position with current theme colors.
  drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE,
           themeForeground, themeBackground);
}

void showProductQRScreen(String label, int pin) {
  DisplayLock l;
  if (!_gfx) return;
  label = sanitizeLabel(label);
  String words[3];
  int wordCount;
  splitLabelWords(label, pin, words, wordCount);

  // Invert QR screen for themes where fg is dark-on-light:
  // QR modules must be dark on a light background to be scannable.
  // Convention: first value = text color, second = background color;
  // on the QR page those are flipped so the QR area becomes light.
  uint16_t qrFg = themeForeground;
  uint16_t qrBg = themeBackground;
  if (displayConfig.theme == "btcorange-black" ||
      displayConfig.theme == "zapbox" ||
      displayConfig.theme == "white-navy") {
    qrFg = themeBackground;   // dark modules
    qrBg = themeForeground;   // light/colored background
  }

  fillScreen(qrBg);
  drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
  drawLabelBox(words, wordCount, qrFg, qrBg);
  flushDisplay();
}

void showQRScreen() {
  int pinIndex = getPinIndex(12);
  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                  ? productLabels.labels[pinIndex]
                  : String("READY 4 ZAP ACTION");
  showProductQRScreen(label, 12);
}

void showThresholdQRScreen() {
  DisplayLock l;
  if (!_gfx) return;

  uint16_t qrFg = themeForeground;
  uint16_t qrBg = themeBackground;
  if (displayConfig.theme == "btcorange-black" ||
      displayConfig.theme == "zapbox" ||
      displayConfig.theme == "white-navy") {
    qrFg = themeBackground;
    qrBg = themeForeground;
  }

  fillScreen(qrBg);
  drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
  String words[3] = {"READY", "4 TH", "ACTION"};
  drawLabelBox(words, 3, qrFg, qrBg);
  flushDisplay();
}

void showBoltCardScreen(String label, int pin) {
  DisplayLock l;
  if (!_gfx) return;
  label = sanitizeLabel(label);
  String words[3];
  int wordCount;
  splitLabelWords(label, pin, words, wordCount);

  fillScreen(themeBackground);
  // Left: BOLT / CARD / Tap NFC where the QR would be
  drawCenter(QR_AREA_CX, QR_AREA_CY - 60, "BOLT",
             themeForeground, themeBackground, 5);
  drawCenter(QR_AREA_CX, QR_AREA_CY,      "CARD",
             themeForeground, themeBackground, 5);
  drawCenter(QR_AREA_CX, QR_AREA_CY + 70, "Tap NFC",
             themeForeground, themeBackground, 2);
  drawLabelBox(words, wordCount, themeForeground, themeBackground);
  flushDisplay();
}

void showMobilePhoneScreen(String label, int pin) {
  DisplayLock l;
  if (!_gfx) return;
  label = sanitizeLabel(label);
  String words[3];
  int wordCount;
  splitLabelWords(label, pin, words, wordCount);

  fillScreen(themeBackground);
  drawCenter(QR_AREA_CX, QR_AREA_CY - 60, "MOBILE",
             themeForeground, themeBackground, 4);
  drawCenter(QR_AREA_CX, QR_AREA_CY,      "PHONE",
             themeForeground, themeBackground, 4);
  drawCenter(QR_AREA_CX, QR_AREA_CY + 70, "Tap NFC",
             themeForeground, themeBackground, 2);
  drawLabelBox(words, wordCount, themeForeground, themeBackground);
  flushDisplay();
}

void productSelectionScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, 90,  "SELECT",
             themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, 155, "PRODUCT",
             themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, 240, "<-NEXT->",
             themeForeground, themeBackground, 4);
  flushDisplay();
}
// ============================================================================
// SCREENSAVER + DEEP SLEEP
// ============================================================================
// Screensaver modes:
//   "off"        — display always on (no-op)
//   "black"      — display fills black, controller stays on
//   "backlight"  — backlight off (BL pin LOW), display content unchanged
// Deep-sleep modes:
//   "freeze"     — esp_deep_sleep, restart on wake (only BOOT button for now)
//   "light"      — esp_light_sleep, resumes (then ESP.restart for clean state)

static bool   screensaverIsActive = false;
static String screensaverMode     = "off";
static bool   deepSleepIsActive   = false;
static String deepSleepMode       = "off";

void activateScreensaver(String mode) {
  DisplayLock l;
  Serial.println("[SCREENSAVER] Activating mode: " + mode);
  screensaverIsActive = true;
  screensaverMode = mode;

  if (mode == "black") {
    fillScreen(TFT_BLACK);
    flushDisplay();
  } else if (mode == "backlight") {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, LOW);
  }
}

void deactivateScreensaver() {
  DisplayLock l;
  if (!screensaverIsActive) return;
  Serial.println("[SCREENSAVER] Deactivating mode: " + screensaverMode);

  if (screensaverMode == "backlight") {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
  }
  screensaverIsActive = false;
  screensaverMode = "off";
  // The main loop will redraw the appropriate screen.
}

bool isScreensaverActive() { return screensaverIsActive; }

void prepareDeepSleep() {
  DisplayLock l;
  Serial.println("[DEEP_SLEEP] Preparing display for deep sleep...");
  esp_task_wdt_delete(NULL);
  fillScreen(TFT_BLACK);
  flushDisplay();
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);
  delay(200);
}

void setupDeepSleepWakeup(String mode) {
  Serial.println("[DEEP_SLEEP] Setup wake sources for mode: " + mode);
  deepSleepIsActive = true;
  deepSleepMode = mode;

  // BOOT button (GPIO 0) is the only reliable wake source on this board until
  // the AXS15231B touch driver is ported (touch INT on GPIO 16 would be ideal).

  if (mode == "freeze") {
    rtc_gpio_init(GPIO_NUM_0);
    rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    delay(100);

    if (rtc_gpio_get_level(GPIO_NUM_0) == 0) {
      Serial.println("[DEEP_SLEEP] Aborting: BOOT button is currently pressed");
      rtc_gpio_deinit(GPIO_NUM_0);
      deepSleepIsActive = false;
      deepSleepMode = "off";
      return;
    }
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);   // wake on LOW
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    Serial.println("[DEEP_SLEEP] Wake on BOOT (GPIO 0). Entering deep sleep.");
    Serial.flush();
    delay(200);
    esp_deep_sleep_start();   // does not return
  } else if (mode == "light") {
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    Serial.println("[LIGHT_SLEEP] Wake on BOOT (GPIO 0). Entering light sleep.");
    Serial.flush();
    delay(200);
    esp_light_sleep_start();

    // Light sleep returns here on wake.
    Serial.println("[LIGHT_SLEEP] Woke up — restarting for clean state.");
    gpio_wakeup_disable(GPIO_NUM_0);
    deepSleepIsActive = false;
    deepSleepMode = "off";
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    Serial.println("[DEEP_SLEEP] Unknown mode, ignoring: " + mode);
    deepSleepIsActive = false;
    deepSleepMode = "off";
  }
}

bool isDeepSleepActive() { return deepSleepIsActive; }
void nfcTestScreen(String lnurlw) {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  // Big green confirmation header
  drawCenter(SCR_W / 2, 70, "NFC OK!",
             TFT_GREEN, themeBackground, 5);

  // LNURL-W preview: first 38 chars on line 1, next 38 on line 2
  // (480 wide - 20 px margins = 460 / 12 px per char at size 2 ≈ 38 chars)
  const int chunk = 38;
  int total = lnurlw.length();
  String line1 = lnurlw.substring(0, total > chunk ? chunk : total);
  drawCenter(SCR_W / 2, 160, line1.c_str(),
             themeForeground, themeBackground, 2);
  if (total > chunk) {
    String line2 = lnurlw.substring(chunk, total > 2 * chunk ? 2 * chunk : total);
    drawCenter(SCR_W / 2, 200, line2.c_str(),
               themeForeground, themeBackground, 2);
  }

  drawCenter(SCR_W / 2, 270, "See Serial for full LNURLW",
             TFT_DARKGREY, themeBackground, 2);
  flushDisplay();
}

#endif  // BOARD_JC3248W535C
