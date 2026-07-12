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
#include "PinConfig.h"
#include "GlobalState.h"
#include "Battery.h"
#include "Log.h"

// ============================================================================
// PANEL & GEOMETRY
// ============================================================================

#define PIN_LCD_BL  LCD_BL_PIN
// Physical panel: 320×480 portrait (PANEL_W × PANEL_H).
// The logical canvas (SCR_W × SCR_H) is set at runtime based on orientation:
//   h / hi  → landscape 480×320  (SCR_W=PANEL_H, SCR_H=PANEL_W)
//   v / vi  → portrait  320×480  (SCR_W=PANEL_W, SCR_H=PANEL_H)
// putPixel() maps logical canvas coords to the physical portrait buffer.
#define PANEL_W     320
#define PANEL_H     480
static int SCR_W = 480;   // updated in initDisplay()
static int SCR_H = 320;   // updated in initDisplay()

// 0=h (90° CCW), 1=hi (90° CW), 2=v (direct), 3=vi (180°)
static int s_oriMode = 0;
static inline bool isPortrait() { return s_oriMode >= 2; }

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
  bool        invertQr;   // true = swap fg/bg in QR area so modules are dark on light
};

static const ThemeConfig themeConfigs[] = {
  {"black-white",            TFT_BLACK,          TFT_WHITE,       false},
  {"black-darkcyan",         TFT_BLACK,          TFT_DARKCYAN,    false},
  {"darkgreen-green",        TFT_DARKGREEN,      TFT_GREEN,       false},
  {"darkgreen-lightgrey",    TFT_DARKGREEN,      TFT_LIGHTGREY,   false},
  {"darkblue-lightgrey",     TFT_NAVY,           TFT_LIGHTGREY,   false},
  {"red-green",              TFT_RED,            TFT_GREEN,       false},
  {"black-blue",             TFT_BLACK,          TFT_BLUE,        false},
  {"orange-brown",           TFT_ORANGE,         TFT_BROWN,       false},
  {"black-yellow",           TFT_BLACK,          TFT_YELLOW,      false},
  {"black-btcorange",        TFT_BLACK,          COLOR_BTCORANGE, false},
  {"btcorange-black",        COLOR_BTCORANGE,    TFT_BLACK,       false}, // inv. QR: orange modules on black
  {"btcorange-black-std",    COLOR_BTCORANGE,    TFT_BLACK,       true},  // std. QR: black modules on orange
  {"darkgrey-btcorange",     TFT_DARKGREY,       COLOR_BTCORANGE, false},
  {"zapbox",                 COLOR_ZAPBOX_AMBER, TFT_BLACK,       true},
  {"maroon-magenta",         TFT_MAROON,         TFT_MAGENTA,     false},
  {"black-red",              TFT_BLACK,          TFT_RED,         false},
  {"brown-orange",           TFT_BROWN,          TFT_ORANGE,      false},
  {"black-orange",           TFT_BLACK,          TFT_ORANGE,      false},
  {"white-darkcyan",         TFT_WHITE,          TFT_DARKCYAN,    false},
  {"white-navy",             TFT_WHITE,          TFT_NAVY,        false}, // inv. QR: white modules on navy
  {"white-navy-std",         TFT_WHITE,          TFT_NAVY,        true},  // std. QR: navy modules on white
  {"navy-white",             TFT_NAVY,           TFT_WHITE,       false},
  {"darkcyan-cyan",          TFT_DARKCYAN,       TFT_CYAN,        false},
  {"black-olive",            TFT_BLACK,          TFT_OLIVE,       false},
  {"black-darkgrey",         TFT_BLACK,          TFT_DARKGREY,    false},
  {"black-lightgrey",        TFT_BLACK,          TFT_LIGHTGREY,   false},
  {"black-green",            TFT_BLACK,          TFT_GREEN,       false},
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

static bool themeInvertQr() {
  for (const auto &t : themeConfigs) {
    if (displayConfig.theme == t.name) return t.invertQr;
  }
  return false;
}

// External currency string set from config (defaults to "USD")
extern String currency;

// ============================================================================
// SHARED LAYOUT CONSTANTS (used by QR / step / NFC screens)
// ============================================================================
// Left half:  196×196 QR area  (29..225,  62..258) — equal 29/30/29 px gaps
// Right half: 196×196 info box (255..451, 62..258)   left | middle | right
#define QR_X        29
#define QR_Y        62
#define QR_MOD_SIZE 4     // 49 modules × 4 px = 196 px
#define QR_AREA_CX  ((QR_X) + (49 * QR_MOD_SIZE) / 2)   // 127
#define QR_AREA_CY  ((QR_Y) + (49 * QR_MOD_SIZE) / 2)   // 160

#define BOX_X       255
#define BOX_Y       62
#define BOX_W       196
#define BOX_H       196

// ============================================================================
// PORTRAIT LAYOUT CONSTANTS  (v / vi — canvas 320×480)
// ============================================================================
// QR: 49 modules × 4 px = 196 px, centered → x = (320-196)/2 = 62
// QR+box content is 408 px tall → top margin (480-408)/2 = 36
#define QR_V_X        62
#define QR_V_Y        36
#define QR_V_MOD       4
// Label box below QR  (36+196+12 = 244)
#define BOX_V_X       20
#define BOX_V_Y      244
#define BOX_V_W      280
#define BOX_V_H      200
// Action-time (portrait) — ACTION + TIME box + MM:SS countdown stacked and
// vertically centered on the 320×480 screen (countdown sits below the box,
// since the narrow portrait width leaves no room for side digits).
#define AT_V_BOX_X    60
#define AT_V_BOX_Y   205
#define AT_V_BOX_W   200
#define AT_V_BOX_H    80
#define AT_V_LABEL_Y 245
#define AT_V_TIME_Y  325
// NFC box (portrait — 200 px wide, centered in 320)
#define NFC_V_BOX_X   60
#define NFC_V_BOX_W  200

// ============================================================================
// BITCOIN LOGO - 96x96 monochrome bitmap (1=foreground, MSB-first)
// ============================================================================
static const uint8_t bitcoin_logo[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x7f,0xfe,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,
  0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,
  0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,
  0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,
  0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,
  0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,
  0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,
  0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,
  0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,
  0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,
  0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x1f,0xff,0xff,0xff,0xfc,0x7f,0xff,0xff,0xff,0xf8,0x00,
  0x00,0x3f,0xff,0xff,0xff,0xfc,0x1f,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x7f,0xff,0xff,0xff,0xf8,0x1e,0x7f,0xff,0xff,0xfe,0x00,
  0x00,0x7f,0xff,0xff,0xff,0xf8,0x1e,0x07,0xff,0xff,0xfe,0x00,
  0x00,0xff,0xff,0xff,0xff,0xf8,0x3e,0x0f,0xff,0xff,0xff,0x00,
  0x01,0xff,0xff,0xff,0xff,0xf8,0x3e,0x0f,0xff,0xff,0xff,0x80,
  0x01,0xff,0xff,0xff,0x8f,0xf0,0x3c,0x0f,0xff,0xff,0xff,0x80,
  0x03,0xff,0xff,0xff,0x80,0xf0,0x3c,0x0f,0xff,0xff,0xff,0xc0,
  0x03,0xff,0xff,0xff,0x80,0x00,0x7c,0x1f,0xff,0xff,0xff,0xc0,
  0x07,0xff,0xff,0xff,0x00,0x00,0x3c,0x1f,0xff,0xff,0xff,0xe0,
  0x07,0xff,0xff,0xff,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xe0,
  0x0f,0xff,0xff,0xff,0x80,0x00,0x00,0x1f,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0xf0,0x00,0x00,0x0f,0xff,0xff,0xff,0xf0,
  0x1f,0xff,0xff,0xff,0xfc,0x00,0x00,0x03,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,0x7f,0xff,0xff,0xf8,
  0x3f,0xff,0xff,0xff,0xfc,0x00,0xe0,0x00,0x1f,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xff,0xfc,0x01,0xfc,0x00,0x0f,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xff,0xf8,0x01,0xff,0x00,0x0f,0xff,0xff,0xfc,
  0x7f,0xff,0xff,0xff,0xf8,0x01,0xff,0x80,0x07,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xf8,0x01,0xff,0xc0,0x07,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xf8,0x03,0xff,0xe0,0x07,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xf0,0x03,0xff,0xe0,0x07,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xf0,0x03,0xff,0xe0,0x07,0xff,0xff,0xfe,
  0xff,0xff,0xff,0xff,0xf0,0x03,0xff,0xe0,0x07,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xf0,0x07,0xff,0xc0,0x07,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xe0,0x03,0xff,0x80,0x07,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xe0,0x00,0x3e,0x00,0x0f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xc0,0x0f,0x00,0x01,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xc0,0x1f,0xe0,0x00,0x7f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0x80,0x1f,0xf8,0x00,0x3f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0x80,0x1f,0xfe,0x00,0x1f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0x80,0x1f,0xff,0x00,0x1f,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0x80,0x3f,0xff,0x80,0x0f,0xff,0xff,0xff,
  0x7f,0xff,0xff,0xff,0x00,0x3f,0xff,0x80,0x0f,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0x00,0x3f,0xff,0x80,0x0f,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xc0,0x00,0x3f,0xff,0x80,0x0f,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0x80,0x00,0x7f,0xff,0x80,0x0f,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0x80,0x00,0x7f,0xff,0x80,0x0f,0xff,0xff,0xfe,
  0x3f,0xff,0xff,0x00,0x00,0x3f,0xff,0x00,0x0f,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0x00,0x00,0x03,0xfc,0x00,0x1f,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xfc,
  0x1f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x7f,0xff,0xff,0xf8,
  0x0f,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0x81,0xe0,0x00,0x01,0xff,0xff,0xff,0xf0,
  0x07,0xff,0xff,0xff,0x81,0xe0,0x00,0x07,0xff,0xff,0xff,0xe0,
  0x07,0xff,0xff,0xff,0x83,0xe0,0xff,0xff,0xff,0xff,0xff,0xe0,
  0x03,0xff,0xff,0xff,0x83,0xe0,0xff,0xff,0xff,0xff,0xff,0xc0,
  0x03,0xff,0xff,0xff,0x03,0xc0,0xff,0xff,0xff,0xff,0xff,0xc0,
  0x01,0xff,0xff,0xff,0x03,0xc0,0xff,0xff,0xff,0xff,0xff,0x80,
  0x01,0xff,0xff,0xff,0x07,0xc1,0xff,0xff,0xff,0xff,0xff,0x80,
  0x00,0xff,0xff,0xff,0xe7,0xc1,0xff,0xff,0xff,0xff,0xff,0x00,
  0x00,0x7f,0xff,0xff,0xff,0x81,0xff,0xff,0xff,0xff,0xfe,0x00,
  0x00,0x7f,0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xff,0xfe,0x00,
  0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,
  0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,
  0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,
  0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,
  0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,
  0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,
  0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,
  0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,
  0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,
  0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,
  0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,
  0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x7f,0xfe,0x00,0x00,0x00,0x00,0x00,
};

// BITCOIN LOGO (landscape) - 120x120 monochrome, MSB-first (1=foreground)
static const uint8_t bitcoin_logo_h[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xff,0xff,0x80,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,0x00,0x00,
  0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,
  0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,
  0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,0x00,
  0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,
  0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,
  0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,
  0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,
  0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,
  0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,
  0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,
  0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,
  0x00,0x03,0xff,0xff,0xff,0xff,0xff,0x8f,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,
  0x00,0x07,0xff,0xff,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0x80,0xf8,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0x01,0xf8,0x0f,0xff,0xff,0xff,0xf8,0x00,
  0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0x01,0xf8,0x0f,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0x01,0xf0,0x1f,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0x01,0xf0,0x1f,0xff,0xff,0xff,0xfe,0x00,
  0x00,0xff,0xff,0xff,0xff,0x83,0xfe,0x03,0xf0,0x1f,0xff,0xff,0xff,0xff,0x00,
  0x00,0xff,0xff,0xff,0xff,0x80,0x3e,0x03,0xf0,0x1f,0xff,0xff,0xff,0xff,0x00,
  0x01,0xff,0xff,0xff,0xff,0x80,0x02,0x03,0xe0,0x3f,0xff,0xff,0xff,0xff,0x80,
  0x01,0xff,0xff,0xff,0xff,0x80,0x00,0x03,0xe0,0x3f,0xff,0xff,0xff,0xff,0x80,
  0x03,0xff,0xff,0xff,0xff,0x00,0x00,0x01,0xe0,0x3f,0xff,0xff,0xff,0xff,0xc0,
  0x03,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xc0,
  0x07,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xe0,
  0x07,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xe0,
  0x0f,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xf0,
  0x1f,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xff,0xfe,0x00,0x0c,0x00,0x00,0x1f,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xff,0xfe,0x00,0x1f,0xc0,0x00,0x0f,0xff,0xff,0xff,0xf8,
  0x3f,0xff,0xff,0xff,0xff,0xfe,0x00,0x1f,0xf8,0x00,0x07,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xff,0xff,0xfe,0x00,0x1f,0xfc,0x00,0x07,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xff,0xff,0xfc,0x00,0x1f,0xff,0x00,0x03,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xff,0xff,0xfc,0x00,0x3f,0xff,0x80,0x03,0xff,0xff,0xff,0xfc,
  0x7f,0xff,0xff,0xff,0xff,0xfc,0x00,0x3f,0xff,0x80,0x03,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0xfc,0x00,0x3f,0xff,0xc0,0x03,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0xf8,0x00,0x3f,0xff,0xc0,0x03,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0xf8,0x00,0x7f,0xff,0xc0,0x03,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0xf8,0x00,0x7f,0xff,0x80,0x03,0xff,0xff,0xff,0xfe,
  0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x7f,0xff,0x80,0x03,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x7f,0xff,0x00,0x03,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x3f,0xfe,0x00,0x03,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x03,0xf8,0x00,0x07,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xc0,0x01,0xc0,0x00,0x01,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xc0,0x03,0xfc,0x00,0x00,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xc0,0x03,0xff,0x80,0x00,0x3f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xc0,0x03,0xff,0xe0,0x00,0x1f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0x80,0x03,0xff,0xf8,0x00,0x1f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0x80,0x07,0xff,0xfc,0x00,0x0f,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0x80,0x07,0xff,0xfe,0x00,0x0f,0xff,0xff,0xff,0xff,
  0x7f,0xff,0xff,0xff,0xff,0x80,0x07,0xff,0xfe,0x00,0x07,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0x00,0x07,0xff,0xfe,0x00,0x07,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0x00,0x0f,0xff,0xff,0x00,0x07,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0x0e,0x00,0x0f,0xff,0xff,0x00,0x07,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0x00,0x00,0x0f,0xff,0xfe,0x00,0x07,0xff,0xff,0xff,0xfe,
  0x3f,0xff,0xff,0xfe,0x00,0x00,0x0f,0xff,0xfe,0x00,0x07,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xfe,0x00,0x00,0x1f,0xff,0xfe,0x00,0x07,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xfc,0x00,0x00,0x0f,0xff,0xfc,0x00,0x07,0xff,0xff,0xff,0xfc,
  0x3f,0xff,0xff,0xfc,0x00,0x00,0x00,0xff,0xf0,0x00,0x0f,0xff,0xff,0xff,0xfc,
  0x1f,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xf8,
  0x1f,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xf8,
  0x0f,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xf0,
  0x0f,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xf0,
  0x07,0xff,0xff,0xff,0xff,0xc0,0x60,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xe0,
  0x07,0xff,0xff,0xff,0xff,0xc0,0x7c,0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xe0,
  0x03,0xff,0xff,0xff,0xff,0x80,0xfc,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xc0,
  0x03,0xff,0xff,0xff,0xff,0x80,0xfc,0x07,0x80,0x3f,0xff,0xff,0xff,0xff,0xc0,
  0x01,0xff,0xff,0xff,0xff,0x80,0xfc,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0x80,
  0x01,0xff,0xff,0xff,0xff,0x80,0xf8,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0x80,
  0x00,0xff,0xff,0xff,0xff,0x01,0xf8,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0x00,
  0x00,0xff,0xff,0xff,0xff,0x01,0xf8,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0x00,
  0x00,0x7f,0xff,0xff,0xff,0x01,0xf8,0x1f,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,
  0x00,0x3f,0xff,0xff,0xff,0x81,0xf0,0x1f,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x3f,0xff,0xff,0xff,0xfb,0xf0,0x1f,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,
  0x00,0x1f,0xff,0xff,0xff,0xff,0xf0,0x1f,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xf8,0x3f,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xbf,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,
  0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,
  0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,
  0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,
  0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,
  0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,
  0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,
  0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,
  0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,
  0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,
  0x00,0x00,0x03,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,
  0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,0x00,
  0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,
  0x00,0x00,0x00,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x00,0x00,0x00,
  0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,
  0x00,0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xff,0xff,0x80,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,
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

// Maps logical canvas (lx, ly) → physical portrait buffer (px, py).
//   h  (0): 90° CCW  — px = PANEL_W-1-ly,  py = lx
//   hi (1): 90° CW   — px = ly,             py = PANEL_H-1-lx
//   v  (2): direct   — px = lx,             py = ly
//   vi (3): 180°     — px = PANEL_W-1-lx,   py = PANEL_H-1-ly
static inline void putPixel(int lx, int ly, uint16_t color) {
  if ((unsigned)lx >= (unsigned)SCR_W || (unsigned)ly >= (unsigned)SCR_H) return;
  int px, py;
  switch (s_oriMode) {
    case 1:  px = ly;             py = PANEL_H - 1 - lx;  break; // hi
    case 2:  px = lx;             py = ly;                 break; // v
    case 3:  px = PANEL_W-1-lx;   py = PANEL_H - 1 - ly;  break; // vi
    default: px = PANEL_W-1-ly;   py = lx;                 break; // h
  }
  s_backbuf[py * PANEL_W + px] = color;
}

static void fillRect(int lx, int ly, int lw, int lh, uint16_t color) {
  if (!s_backbuf) return;
  if (lx < 0) { lw += lx; lx = 0; }
  if (ly < 0) { lh += ly; ly = 0; }
  if (lx + lw > SCR_W) lw = SCR_W - lx;
  if (ly + lh > SCR_H) lh = SCR_H - ly;
  if (lw <= 0 || lh <= 0) return;

  int px, py, pw, ph;
  switch (s_oriMode) {
    case 1:  px = ly;               py = PANEL_H - lx - lw;  pw = lh; ph = lw; break; // hi
    case 2:  px = lx;               py = ly;                 pw = lw; ph = lh; break; // v
    case 3:  px = PANEL_W - lx - lw; py = PANEL_H - ly - lh; pw = lw; ph = lh; break; // vi
    default: px = PANEL_W - ly - lh; py = lx;                pw = lh; ph = lw; break; // h
  }
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

  // Resolve orientation mode and canvas dimensions from config
  if      (displayConfig.orientation == "hi") { s_oriMode = 1; SCR_W = PANEL_H; SCR_H = PANEL_W; }
  else if (displayConfig.orientation == "v")  { s_oriMode = 2; SCR_W = PANEL_W; SCR_H = PANEL_H; }
  else if (displayConfig.orientation == "vi") { s_oriMode = 3; SCR_W = PANEL_W; SCR_H = PANEL_H; }
  else                                        { s_oriMode = 0; SCR_W = PANEL_H; SCR_H = PANEL_W; } // h

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
  const uint8_t size = 4;
  String s(label);
  int textW = (int)s.length() * 6 * size;
  int sp = s.indexOf(' ');
  // If the single line is too wide for the screen (portrait), split at the
  // first space into two stacked lines (e.g. "NO" / "WEB SOCKET").
  if (textW > SCR_W - 16 && sp > 0) {
    int lh = 8 * size;
    drawCenter(SCR_W / 2, SCR_H / 2 - lh / 2 - 4, s.substring(0, sp).c_str(), themeForeground, themeBackground, size);
    drawCenter(SCR_W / 2, SCR_H / 2 + lh / 2 + 4, s.substring(sp + 1).c_str(), themeForeground, themeBackground, size);
  } else {
    drawCenter(SCR_W / 2, SCR_H / 2, label, themeForeground, themeBackground, size);
  }
  flushDisplay();
}

void startupScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, "ZAPBOX",                   themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 25, "Firmware " VERSION,        themeForeground, themeBackground, 2);
  drawCenter(SCR_W / 2, SCR_H / 2 + 50, "Powered by LNbits",        themeForeground, themeBackground, 2);
  flushDisplay();
}

void initializationScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, "ZAPBOX",          themeForeground, themeBackground, 5);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "Initializing..", themeForeground, themeBackground, 2);
  flushDisplay();
}

void configModeScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, SCR_H / 2 - 20, "CONFIG",   themeForeground, themeBackground, 4);
  drawCenter(SCR_W / 2, SCR_H / 2 + 30, "MODE",     themeForeground, themeBackground, 4);
  flushDisplay();
}

void errorReportScreen(uint8_t w, uint8_t i, uint8_t s, uint8_t ws) {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  drawCenter(SCR_W / 2, 40, "REPORT", themeForeground, themeBackground, 4);
  char line[32];
  snprintf(line, sizeof(line), "WiFi: %u",       w); drawCenter(SCR_W / 2, 130, line, themeForeground, themeBackground, 2);
  snprintf(line, sizeof(line), "Internet: %u",   i); drawCenter(SCR_W / 2, 168, line, themeForeground, themeBackground, 2);
  snprintf(line, sizeof(line), "Server: %u",     s); drawCenter(SCR_W / 2, 206, line, themeForeground, themeBackground, 2);
  snprintf(line, sizeof(line), "WebSocket: %u", ws); drawCenter(SCR_W / 2, 244, line, themeForeground, themeBackground, 2);
  flushDisplay();
}

void wifiReconnectScreen()      { DisplayLock l; if (_gfx) renderStatusBox("NO WIFI"); }
void internetReconnectScreen()  { DisplayLock l; if (_gfx) renderStatusBox("NO INTERNET"); }
void serverReconnectScreen()    { DisplayLock l; if (_gfx) renderStatusBox("NO SERVER"); }
void websocketReconnectScreen() { DisplayLock l; if (_gfx) renderStatusBox("NO WEB SOCKET"); }
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
// Landscape BTC ticker layout (480×320):
//   x= 100..220  Bitcoin logo 120×120 (scale 1, vertically centered)
//   x= 222..     Value clear area; text centred around cx=330
//   Logo + text form a centred pair, with a small gap between them.
//     cy=  80    label "<currency>/BTC"  size 2
//     cy= 106    value price             size 3   ← updated
//     cy= 144    label "SAT/<currency>"  size 2
//     cy= 172    value sats              size 3   ← updated
//     cy= 208    label "Block"           size 2
//     cy= 236    value blockheight       size 3   ← updated
//
// updateBtctickerValues redraws only the 3 value rows to avoid logo flicker.

static const int BTC_H_TXT_CX  = 330;
static const int BTC_H_LBL1_Y  =  80;
static const int BTC_H_VAL1_Y  = 106;
static const int BTC_H_LBL2_Y  = 144;
static const int BTC_H_VAL2_Y  = 172;
static const int BTC_H_LBL3_Y  = 208;
static const int BTC_H_VAL3_Y  = 236;
static const int BTC_H_VAL_H   =  24;   // height of size-3 text (8×3)
static const int BTC_H_VAL_X   = 222;   // clear from just past the logo (erases "Loading..." remnant)
static const int BTC_H_VAL_W   = 258;   // width of value clear area (222 to 480)

static String calcSatsPerCurrency() {
  float price = bitcoinData.price.toFloat();
  if (price <= 0) return String("0");
  long sats = (long)((1.0f / price) * 100000000.0f);
  return String(sats);
}

// Portrait BTC ticker layout (320×480) — T-Display-S3 style:
//   Logo 96×96 (header icon), then 3× (small label + big value)
//   Logo + text block centred together (content centre ≈ y=240).
//
//   y= 78  Bitcoin logo 96×96 (scale 1, x=112)
//   y=205  label  "USD/BTC"      size 2
//   y=240  value  price          size 4   ← updated by updateBtctickerValues
//   y=278  label  "SAT/USD"      size 2
//   y=313  value  sats           size 4   ← updated
//   y=351  label  "Block"        size 2
//   y=386  value  block height   size 4   ← updated
static const int BTC_V_VAL1_Y = 240;
static const int BTC_V_VAL2_Y = 313;
static const int BTC_V_VAL3_Y = 386;
static const int BTC_V_VAL_H  =  32;  // height of size-4 text (8×4)

static void btcDrawValues_portrait() {
  int cx = PANEL_W / 2;
  drawCenter(cx, BTC_V_VAL1_Y, bitcoinData.price.c_str(),
             themeForeground, themeBackground, 4);
  drawCenter(cx, BTC_V_VAL2_Y, calcSatsPerCurrency().c_str(),
             themeForeground, themeBackground, 4);
  drawCenter(cx, BTC_V_VAL3_Y, bitcoinData.blockHigh.c_str(),
             themeForeground, themeBackground, 4);
}

static void btcDrawValues_landscape() {
  drawCenter(BTC_H_TXT_CX, BTC_H_VAL1_Y, bitcoinData.price.c_str(),     themeForeground, themeBackground, 3);
  drawCenter(BTC_H_TXT_CX, BTC_H_VAL2_Y, calcSatsPerCurrency().c_str(), themeForeground, themeBackground, 3);
  drawCenter(BTC_H_TXT_CX, BTC_H_VAL3_Y, bitcoinData.blockHigh.c_str(), themeForeground, themeBackground, 3);
}

void btctickerScreen() {
  DisplayLock l;
  if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    drawMonoBitmapScaled((PANEL_W - 96) / 2, 78, bitcoin_logo, 96, 96, themeForeground, 1);
    int cx = PANEL_W / 2;
    drawCenter(cx, 205, (currency + "/BTC").c_str(), themeForeground, themeBackground, 2);
    drawCenter(cx, 278, ("SAT/" + currency).c_str(), themeForeground, themeBackground, 2);
    drawCenter(cx, 351, "Block",                      themeForeground, themeBackground, 2);
    btcDrawValues_portrait();
  } else {
    drawMonoBitmapScaled(100, (SCR_H - 120) / 2, bitcoin_logo_h, 120, 120, themeForeground, 1);
    drawCenter(BTC_H_TXT_CX, BTC_H_LBL1_Y, (currency + "/BTC").c_str(), themeForeground, themeBackground, 2);
    drawCenter(BTC_H_TXT_CX, BTC_H_LBL2_Y, ("SAT/" + currency).c_str(), themeForeground, themeBackground, 2);
    drawCenter(BTC_H_TXT_CX, BTC_H_LBL3_Y, "Block",                      themeForeground, themeBackground, 2);
    btcDrawValues_landscape();
  }
  flushDisplay();
}

void updateBtctickerValues() {
  DisplayLock l;
  if (!_gfx) return;
  if (isPortrait()) {
    fillRect(0, BTC_V_VAL1_Y - BTC_V_VAL_H / 2, PANEL_W, BTC_V_VAL_H, themeBackground);
    fillRect(0, BTC_V_VAL2_Y - BTC_V_VAL_H / 2, PANEL_W, BTC_V_VAL_H, themeBackground);
    fillRect(0, BTC_V_VAL3_Y - BTC_V_VAL_H / 2, PANEL_W, BTC_V_VAL_H, themeBackground);
    btcDrawValues_portrait();
  } else {
    fillRect(BTC_H_VAL_X, BTC_H_VAL1_Y - BTC_H_VAL_H / 2, BTC_H_VAL_W, BTC_H_VAL_H, themeBackground);
    fillRect(BTC_H_VAL_X, BTC_H_VAL2_Y - BTC_H_VAL_H / 2, BTC_H_VAL_W, BTC_H_VAL_H, themeBackground);
    fillRect(BTC_H_VAL_X, BTC_H_VAL3_Y - BTC_H_VAL_H / 2, BTC_H_VAL_W, BTC_H_VAL_H, themeBackground);
    btcDrawValues_landscape();
  }
  flushDisplay();
}
// ============================================================================
// STEP / FLOW SCREENS — big number left + label box right (T-Display-S3 style)
// ============================================================================
// Reuses the BOX_X/Y/W/H geometry from the QR screens for a consistent layout.

static void renderStepScreen(const char *big, uint8_t bigSize,
                              const char *l1, const char *l2, const char *l3) {
  fillScreen(themeBackground);
  if (isPortrait()) {
    // Big text + box: content ~376px → shift down ~15px so visual weight is balanced
    drawCenter(SCR_W / 2, 120, big, themeForeground, themeBackground, bigSize);
    const int bx = 20, by = 230, bw = 280, bh = 210;
    fillRect(bx, by, bw, bh, themeForeground);
    int cx = bx + bw / 2;
    drawCenter(cx, by + bh / 4,     l1, themeBackground, themeForeground, 4);
    drawCenter(cx, by + bh / 2,     l2, themeBackground, themeForeground, 4);
    drawCenter(cx, by + 3 * bh / 4, l3, themeBackground, themeForeground, 4);
  } else {
    drawCenter(QR_AREA_CX, QR_AREA_CY, big, themeForeground, themeBackground, bigSize);
    fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, themeForeground);
    int cx = BOX_X + BOX_W / 2;
    drawCenter(cx, BOX_Y + BOX_H / 4,     l1, themeBackground, themeForeground, 4);
    drawCenter(cx, BOX_Y + BOX_H / 2,     l2, themeBackground, themeForeground, 4);
    drawCenter(cx, BOX_Y + 3 * BOX_H / 4, l3, themeBackground, themeForeground, 4);
  }
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
// ACTION TIME — "ACTION" big at top, "TIME" in inverted box, MM:SS below
// ============================================================================
// Landscape layout on 480×320 (stacked and vertically centered):
//   y=82       "ACTION"          size 6 (centered)
//   y=128..208 TIME box (220×80, x=130..350)
//      y=168   "TIME" inside box, size 5
//   y=246      "MM:SS" countdown size 4 (centered below the box)

#define AT_BOX_X    130
#define AT_BOX_Y    128
#define AT_BOX_W    220
#define AT_BOX_H    80
#define AT_LABEL_Y  168
#define AT_TIME_Y   246

void actionTimeScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    // ACTION + TIME box + MM:SS countdown, stacked and centered
    drawCenter(SCR_W / 2, 160, "ACTION", themeForeground, themeBackground, 5);
    fillRect(AT_V_BOX_X, AT_V_BOX_Y, AT_V_BOX_W, AT_V_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, AT_V_LABEL_Y, "TIME", themeBackground, themeForeground, 4);
  } else {
    drawCenter(SCR_W / 2, 82, "ACTION", themeForeground, themeBackground, 6);
    fillRect(AT_BOX_X, AT_BOX_Y, AT_BOX_W, AT_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, AT_LABEL_Y, "TIME", themeBackground, themeForeground, 5);
  }
  flushDisplay();
}

// IDENTITY LOGIN-TRIGGER — Authy idle/start screen. Same look as ACTION TIME
// ("IDENTITY" big at top, "LOGIN-TRIGGER" in the inverted box), but no countdown.
// Shown while no auth QR is requested, so the NT3H tag is not rewritten. No tab
// here: the dual-page switch lives on the QR pages, not on the idle screen.
void identityTriggerScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    drawCenter(SCR_W / 2, 160, "IDENTITY", themeForeground, themeBackground, 4);
    fillRect(25, AT_V_BOX_Y, 270, AT_V_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, AT_V_LABEL_Y, "LOGIN-TRIGGER", themeBackground, themeForeground, 3);
    drawCenter(SCR_W / 2, AT_V_BOX_Y + AT_V_BOX_H + 28, "Touch screen",
               themeForeground, themeBackground, 2);
    drawCenter(SCR_W / 2, AT_V_BOX_Y + AT_V_BOX_H + 50, "to start login.",
               themeForeground, themeBackground, 2);
  } else {
    // Landscape: nudge the whole block down just a little.
    const int dy = 12;
    drawCenter(SCR_W / 2, 82 + dy, "IDENTITY", themeForeground, themeBackground, 5);
    fillRect(100, AT_BOX_Y + dy, 280, AT_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, AT_LABEL_Y + dy, "LOGIN-TRIGGER", themeBackground, themeForeground, 3);
    drawCenter(SCR_W / 2, AT_BOX_Y + dy + AT_BOX_H + 35, "Touch screen to start login.",
               themeForeground, themeBackground, 2);
  }
  flushDisplay();
}

// Shown when the device tries to open the identity login but the server reports
// Identities disabled (HTTP 403). Red message (error → colour exception OK).
void authIdentityDisabledScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  int cy = SCR_H / 2;
  drawCenter(SCR_W / 2, cy - 18, "IDENTITY LOGIN", TFT_RED, themeBackground, isPortrait() ? 2 : 3);
  drawCenter(SCR_W / 2, cy + 22, "DISABLED",       TFT_RED, themeBackground, isPortrait() ? 3 : 4);
  flushDisplay();
}

void updateActionTimeCountdown(int remainingSecs) {
  DisplayLock l; if (!_gfx) return;
  if (remainingSecs < 0) remainingSecs = 0;
  int mins = remainingSecs / 60;
  int secs = remainingSecs % 60;
  if (mins > 99) mins = 99;

  // One combined MM:SS line, centered below the TIME box (both orientations)
  int timeY = isPortrait() ? AT_V_TIME_Y : AT_TIME_Y;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
  fillRect(0, timeY - 22, SCR_W, 44, themeBackground);
  drawCenter(SCR_W / 2, timeY, buf, themeForeground, themeBackground, 4);
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
  if (isPortrait()) {
    // Content ≈165px → centered: top at ~157
    drawCenter(SCR_W / 2, 177, "PENDING", themeForeground, themeBackground, 5);
    fillRect(NFC_V_BOX_X, 242, NFC_V_BOX_W, 80, themeForeground);
    drawCenter(SCR_W / 2, 282, "NFC", themeBackground, themeForeground, 5);
  } else {
    drawCenter(SCR_W / 2, 117, "PENDING", themeForeground, themeBackground, 6);
    fillRect(NFC_BOX_X, NFC_BOX_Y + 32, NFC_BOX_W, NFC_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, NFC_BOX_LBL_Y + 32, "NFC", themeBackground, themeForeground, 5);
  }
  flushDisplay();
}

void nfcNoLuckScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    fillRect(NFC_V_BOX_X, 85, NFC_V_BOX_W, 80, themeForeground);
    drawCenter(SCR_W / 2, 125, "NFC", themeBackground, themeForeground, 5);
    drawCenter(SCR_W / 2, 255, "NO LUCK", themeForeground, themeBackground, 5);
  } else {
    fillRect(NFC_BOX_X, 85, NFC_BOX_W, NFC_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, 130, "NFC", themeBackground, themeForeground, 5);
    drawCenter(SCR_W / 2, 220, "NO LUCK", themeForeground, themeBackground, 6);
  }
  flushDisplay();
}

void nfcNotSupportedScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    fillRect(NFC_V_BOX_X, 50, NFC_V_BOX_W, 80, themeForeground);
    drawCenter(SCR_W / 2, 90,  "NFC",                  themeBackground, themeForeground, 5);
    drawCenter(SCR_W / 2, 205, "not supported",         themeForeground, themeBackground, 3);
    drawCenter(SCR_W / 2, 260, "use zapbox extension",  themeForeground, themeBackground, 2);
  } else {
    fillRect(NFC_BOX_X, 50, NFC_BOX_W, NFC_BOX_H, themeForeground);
    drawCenter(SCR_W / 2, 90,  "NFC",                  themeBackground, themeForeground, 5);
    drawCenter(SCR_W / 2, 200, "not supported",         themeForeground, themeBackground, 3);
    drawCenter(SCR_W / 2, 255, "use zapbox extension",  themeForeground, themeBackground, 2);
  }
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

// True when the text only uses the QR alphanumeric charset
// (digits, UPPERCASE letters, space $ % * + - . / :).
static bool qrTextIsAlphanumeric(const char *s) {
  for (; *s; s++) {
    char c = *s;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) continue;
    if (c == ' ' || c == '$' || c == '%' || c == '*' || c == '+' ||
        c == '-' || c == '.' || c == '/' || c == ':') continue;
    return false;
  }
  return true;
}

// Render a QR code (text data) into the backbuffer. Uses qrcode library version 8
// (49×49 modules) which fits typical Lightning URLs. Longer payloads (e.g.
// BOLT11 invoices in Mini-PoS mode) use version 11 (61×61 modules), rendered
// with a smaller module size centered in the same pixel area.
//
// IMPORTANT: the QRCode library does NOT bounds-check its internal buffers.
// Feeding more text than a version can hold overflows stack VLAs inside
// qrcode_initText (observed: stack canary panic / watchdog reset). The version
// MUST therefore be chosen by content length BEFORE calling the library.
static void drawQRAt(const char *text, int lx, int ly, int mod_size,
                     uint16_t fg, uint16_t bg) {
  if (!text || !*text) return;
  int areaPx = 49 * mod_size;

  // ECC_LOW capacities: v8 = 224 alphanumeric / 152 byte mode,
  //                     v11 = 468 alphanumeric / 321 byte mode.
  size_t len = strlen(text);
  size_t v8Cap  = qrTextIsAlphanumeric(text) ? 224 : 152;
  size_t v11Cap = qrTextIsAlphanumeric(text) ? 468 : 321;
  int version;
  if (len <= v8Cap) {
    version = 8;
  } else if (len <= v11Cap) {
    version = 11;
  } else {
    // Too long for version 11 — clear the area instead of crashing
    Serial.printf("[QR] Content too long for QR v11: %u chars (max %u) — area left empty\n",
                  (unsigned)len, (unsigned)v11Cap);
    fillRect(lx, ly, areaPx, areaPx, bg);
    return;
  }

  QRCode qr;
  // static: 466 B would otherwise sit on the caller's stack — all callers
  // hold DisplayLock, so access is serialized.
  // Size = qrcode_getBufferSize(11) = ((4*11+17)^2 + 7) / 8, as a constant
  // because the library function is not constexpr.
  constexpr int kQrMaxVersion = 11;
  constexpr int kQrSide = 4 * kQrMaxVersion + 17;
  static uint8_t qrBuf[(kQrSide * kQrSide + 7) / 8];
  if (qrcode_initText(&qr, qrBuf, version, 0, text) < 0) {
    // Fallback: just fill the area with bg so we don't show stale pixels
    fillRect(lx, ly, areaPx, areaPx, bg);
    return;
  }
  if (qr.size * mod_size > areaPx) {
    int mod = areaPx / qr.size;            // e.g. 196/61 = 3
    if (mod < 1) mod = 1;
    int offset = (areaPx - qr.size * mod) / 2;
    fillRect(lx, ly, areaPx, areaPx, bg);  // clear unused border pixels
    lx += offset; ly += offset;
    mod_size = mod;
  }
  for (int yy = 0; yy < qr.size; yy++) {
    for (int xx = 0; xx < qr.size; xx++) {
      uint16_t c = qrcode_getModule(&qr, xx, yy) ? fg : bg;
      fillRect(lx + xx * mod_size, ly + yy * mod_size, mod_size, mod_size, c);
    }
  }
}

// Filled label box at arbitrary position — shared by landscape and portrait paths.
static void drawLabelBoxAt(int bx, int by, int bw, int bh,
                            const String words[], int wordCount,
                            uint16_t box_color, uint16_t text_color) {
  fillRect(bx, by, bw, bh, box_color);
  int cx = bx + bw / 2;
  if (wordCount <= 1) {
    int sz = (words[0].length() >= 7) ? 2 : 4;
    drawCenter(cx, by + bh / 2, words[0].c_str(), text_color, box_color, sz);
  } else if (wordCount == 2) {
    int sz1 = (words[0].length() >= 7) ? 2 : 3;
    int sz2 = (words[1].length() >= 7) ? 2 : 3;
    drawCenter(cx, by + bh / 3,     words[0].c_str(), text_color, box_color, sz1);
    drawCenter(cx, by + 2 * bh / 3, words[1].c_str(), text_color, box_color, sz2);
  } else {
    int sz1 = (words[0].length() >= 7) ? 2 : 3;
    int sz2 = (words[1].length() >= 7) ? 2 : 3;
    drawCenter(cx, by + bh / 4,     words[0].c_str(), text_color, box_color, sz1);
    drawCenter(cx, by + bh / 2,     words[1].c_str(), text_color, box_color, sz2);
    drawCenter(cx, by + 3 * bh / 4, words[2].c_str(), text_color, box_color, 2);
  }
}

static void drawLabelBox(const String words[], int wordCount,
                          uint16_t box_color, uint16_t text_color) {
  drawLabelBoxAt(BOX_X, BOX_Y, BOX_W, BOX_H, words, wordCount, box_color, text_color);
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

  uint16_t qrFg = themeForeground;
  uint16_t qrBg = themeBackground;
  if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

  fillScreen(qrBg);
  if (isPortrait()) {
    drawQRAt(lightningConfig.lightning, QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
    drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H, words, wordCount, qrFg, qrBg);
  } else {
    drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
    drawLabelBox(words, wordCount, qrFg, qrBg);
  }
  flushDisplay();
}

void showQRScreen() {
  int activePin = (RELAY_CHANNEL_MAX > 0) ? RELAY_CHANNEL_PINS[0] : 12;
  int pinIndex = getPinIndex(activePin);
  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                  ? productLabels.labels[pinIndex]
                  : String("READY 4 ZAP ACTION");
  showProductQRScreen(label, activePin);
}

void showThresholdQRScreen() {
  DisplayLock l;
  if (!_gfx) return;

  uint16_t qrFg = themeForeground;
  uint16_t qrBg = themeBackground;
  if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

  fillScreen(qrBg);
  if (isPortrait()) {
    drawQRAt(lightningConfig.lightning, QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
    String words[3] = {"READY", "4 TH", "ACTION"};
    drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H, words, 3, qrFg, qrBg);
  } else {
    drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
    String words[3] = {"READY", "4 TH", "ACTION"};
    drawLabelBox(words, 3, qrFg, qrBg);
  }
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
  if (isPortrait()) {
    drawCenter(SCR_W / 2, 80,  "BOLT",    themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 140, "CARD",    themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 200, "Tap NFC", themeForeground, themeBackground, 2);
    drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H, words, wordCount, themeForeground, themeBackground);
  } else {
    drawCenter(QR_AREA_CX, QR_AREA_CY - 60, "BOLT",    themeForeground, themeBackground, 5);
    drawCenter(QR_AREA_CX, QR_AREA_CY,      "CARD",    themeForeground, themeBackground, 5);
    drawCenter(QR_AREA_CX, QR_AREA_CY + 70, "Tap NFC", themeForeground, themeBackground, 2);
    drawLabelBox(words, wordCount, themeForeground, themeBackground);
  }
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
  if (isPortrait()) {
    drawCenter(SCR_W / 2, 80,  "MOBILE",  themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 140, "PHONE",   themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 200, "Tap NFC", themeForeground, themeBackground, 2);
    drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H, words, wordCount, themeForeground, themeBackground);
  } else {
    drawCenter(QR_AREA_CX, QR_AREA_CY - 60, "MOBILE",  themeForeground, themeBackground, 4);
    drawCenter(QR_AREA_CX, QR_AREA_CY,      "PHONE",   themeForeground, themeBackground, 4);
    drawCenter(QR_AREA_CX, QR_AREA_CY + 70, "Tap NFC", themeForeground, themeBackground, 2);
    drawLabelBox(words, wordCount, themeForeground, themeBackground);
  }
  flushDisplay();
}

static String fmtBlockHeight(const String& raw) {
  // Format raw block height as "#X.XXX.XXX" (zero-padded to 7 digits)
  char buf[8];
  snprintf(buf, sizeof(buf), "%07ld", raw.toInt());
  String s = "";
  s += buf[0]; s += '.';
  s += buf[1]; s += buf[2]; s += buf[3]; s += '.';
  s += buf[4]; s += buf[5]; s += buf[6];
  return s;
}

void updateProductSelectBlockHeight() {
  DisplayLock l; if (!_gfx) return;
  if (bitcoinData.blockHigh == "...") return;
  String blk = fmtBlockHeight(bitcoinData.blockHigh);
  fillRect(0, SCR_H - 22, SCR_W, 20, themeBackground);
  drawCenter(SCR_W / 2, SCR_H - 14, blk.c_str(), themeForeground, themeBackground, 2);
  flushDisplay();
}

// ============================================================================
// MODE SELECTION SCREEN (Touch 3.5 — "modeselect" startup)
// ============================================================================
static void drawRectBorder(int x, int y, int w, int h, int t, uint16_t color); // defined below
// Layout constants — landscape (480×320)
static const int MS_L_BTN_W   = 219;
static const int MS_L_BTN_H   = 100;
static const int MS_L_LEFT_X  =  15;
static const int MS_L_RIGHT_X = 246;
static const int MS_L_ROW1_Y  =  80;
static const int MS_L_ROW2_Y  = 192;

// Layout constants — portrait (320×480)
static const int MS_P_BTN_W  = 280;
static const int MS_P_BTN_H  =  78;
static const int MS_P_BTN_X  =  20;
static const int MS_P_ROW1_Y = 105;
static const int MS_P_ROW2_Y = 193;
static const int MS_P_ROW3_Y = 281;
static const int MS_P_ROW4_Y = 369;

void showModeSelectionScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);

  if (isPortrait()) {
    drawCenter(SCR_W / 2, 55, "Mode Selection", themeForeground, themeBackground, 3);

    // Button 1 — Single channel (two lines)
    drawRectBorder(MS_P_BTN_X, MS_P_ROW1_Y, MS_P_BTN_W, MS_P_BTN_H, 2, themeForeground);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW1_Y + 26, "SINGLE",   themeForeground, themeBackground, 3);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW1_Y + 56, "CHANNEL",  themeForeground, themeBackground, 3);

    // Button 2 — Multi-channel (two lines)
    drawRectBorder(MS_P_BTN_X, MS_P_ROW2_Y, MS_P_BTN_W, MS_P_BTN_H, 2, themeForeground);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW2_Y + 26, "MULTI",    themeForeground, themeBackground, 3);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW2_Y + 56, "CHANNEL",  themeForeground, themeBackground, 3);

    // Button 3 — Mini-PoS (single line centered)
    drawRectBorder(MS_P_BTN_X, MS_P_ROW3_Y, MS_P_BTN_W, MS_P_BTN_H, 2, themeForeground);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW3_Y + 31, "MINI-POS", themeForeground, themeBackground, 3);

    // Button 4 — Authy (single line centered)
    drawRectBorder(MS_P_BTN_X, MS_P_ROW4_Y, MS_P_BTN_W, MS_P_BTN_H, 2, themeForeground);
    drawCenter(MS_P_BTN_X + MS_P_BTN_W/2, MS_P_ROW4_Y + 31, "IDENTITY", themeForeground, themeBackground, 3);

  } else {
    // Landscape
    drawCenter(SCR_W / 2, 42, "Mode Selection", themeForeground, themeBackground, 3);

    // Button 1 — Single channel  (top-left, two lines)
    drawRectBorder(MS_L_LEFT_X, MS_L_ROW1_Y, MS_L_BTN_W, MS_L_BTN_H, 2, themeForeground);
    drawCenter(MS_L_LEFT_X  + MS_L_BTN_W/2, MS_L_ROW1_Y + 36, "SINGLE",   themeForeground, themeBackground, 3);
    drawCenter(MS_L_LEFT_X  + MS_L_BTN_W/2, MS_L_ROW1_Y + 68, "CHANNEL",  themeForeground, themeBackground, 3);

    // Button 2 — Multi-channel  (top-right, two lines)
    drawRectBorder(MS_L_RIGHT_X, MS_L_ROW1_Y, MS_L_BTN_W, MS_L_BTN_H, 2, themeForeground);
    drawCenter(MS_L_RIGHT_X + MS_L_BTN_W/2, MS_L_ROW1_Y + 36, "MULTI",    themeForeground, themeBackground, 3);
    drawCenter(MS_L_RIGHT_X + MS_L_BTN_W/2, MS_L_ROW1_Y + 68, "CHANNEL",  themeForeground, themeBackground, 3);

    // Button 3 — Mini-PoS  (bottom-left, single line centered)
    drawRectBorder(MS_L_LEFT_X, MS_L_ROW2_Y, MS_L_BTN_W, MS_L_BTN_H, 2, themeForeground);
    drawCenter(MS_L_LEFT_X  + MS_L_BTN_W/2, MS_L_ROW2_Y + 52, "MINI-POS", themeForeground, themeBackground, 3);

    // Button 4 — Authy  (bottom-right, single line centered)
    drawRectBorder(MS_L_RIGHT_X, MS_L_ROW2_Y, MS_L_BTN_W, MS_L_BTN_H, 2, themeForeground);
    drawCenter(MS_L_RIGHT_X + MS_L_BTN_W/2, MS_L_ROW2_Y + 52, "IDENTITY", themeForeground, themeBackground, 3);
  }

  flushDisplay();
}

// Returns: 1=Single, 2=Multi-channel, 3=Mini-PoS, 4=Authy, -1=no hit
int modeSelectHitTest(uint16_t x, uint16_t y) {
  if (isPortrait()) {
    const uint16_t bx = MS_P_BTN_X, bw = MS_P_BTN_W;
    if (x < bx || x > bx + bw) return -1;
    if (y >= MS_P_ROW1_Y && y < MS_P_ROW1_Y + MS_P_BTN_H) return 1;
    if (y >= MS_P_ROW2_Y && y < MS_P_ROW2_Y + MS_P_BTN_H) return 2;
    if (y >= MS_P_ROW3_Y && y < MS_P_ROW3_Y + MS_P_BTN_H) return 3;
    if (y >= MS_P_ROW4_Y && y < MS_P_ROW4_Y + MS_P_BTN_H) return 4;
    return -1;
  } else {
    // Top row
    if (y >= MS_L_ROW1_Y && y < MS_L_ROW1_Y + MS_L_BTN_H) {
      if (x >= MS_L_LEFT_X  && x < MS_L_LEFT_X  + MS_L_BTN_W) return 1;
      if (x >= MS_L_RIGHT_X && x < MS_L_RIGHT_X + MS_L_BTN_W) return 2;
    }
    // Bottom row
    if (y >= MS_L_ROW2_Y && y < MS_L_ROW2_Y + MS_L_BTN_H) {
      if (x >= MS_L_LEFT_X  && x < MS_L_LEFT_X  + MS_L_BTN_W) return 3;
      if (x >= MS_L_RIGHT_X && x < MS_L_RIGHT_X + MS_L_BTN_W) return 4;
    }
    return -1;
  }
}

void productSelectionScreen() {
  DisplayLock l; if (!_gfx) return;
  fillScreen(themeBackground);
  if (isPortrait()) {
    drawCenter(SCR_W / 2, 177, "ZAPBOX",      themeForeground, themeBackground, 5);
    drawCenter(SCR_W / 2, 242, "the machine", themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 299, "touch me..",  themeForeground, themeBackground, 3);
  } else {
    drawCenter(SCR_W / 2,  92, "ZAPBOX",      themeForeground, themeBackground, 5);
    drawCenter(SCR_W / 2, 157, "the machine", themeForeground, themeBackground, 4);
    drawCenter(SCR_W / 2, 214, "touch me..",  themeForeground, themeBackground, 3);
  }
  if (bitcoinData.blockHigh != "...") {
    String blk = fmtBlockHeight(bitcoinData.blockHigh);
    drawCenter(SCR_W / 2, SCR_H - 14, blk.c_str(), themeForeground, themeBackground, 2);
  }
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

static void syncAmbientPins(bool on) {
  // Flex slot i is channel CH02+i — never hard-code the GPIO numbers here.
  for (int i = 0; i < T35AmbientConfig::FLEX_COUNT; i++) {
    if (!t35AmbientConfig.flexAmbient[i]) continue;
    const int gpio = RELAY_CHANNEL_PINS[i + 1];
    digitalWrite(gpio, on ? HIGH : LOW);
    Serial.printf("[AMBIENT LIGHT] GPIO %d turned %s (display sync)\n", gpio, on ? "ON" : "OFF");
  }
}

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
    if (t35AmbientConfig.anyEnabled()) syncAmbientPins(false);
  }
}

void deactivateScreensaver() {
  DisplayLock l;
  if (!screensaverIsActive) return;
  Serial.println("[SCREENSAVER] Deactivating mode: " + screensaverMode);

  if (screensaverMode == "backlight") {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    if (t35AmbientConfig.anyEnabled()) syncAmbientPins(true);
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
  if (t35AmbientConfig.anyEnabled()) syncAmbientPins(false);
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
    #ifdef PIN_LED_BUTTON_SW
    // External LED button (GPIO 44): light sleep can wake on ANY GPIO via the
    // digital GPIO-wakeup path. (Deep sleep cannot use it — GPIO 44 is not
    // RTC-capable; ext0/ext1 wake needs an RTC GPIO 0-21.) INPUT_PULLUP means
    // an unconnected pin stays HIGH and won't cause a spurious wake.
    pinMode(PIN_LED_BUTTON_SW, INPUT_PULLUP);
    gpio_wakeup_enable((gpio_num_t)PIN_LED_BUTTON_SW, GPIO_INTR_LOW_LEVEL);
    #endif
    esp_sleep_enable_gpio_wakeup();
    #ifdef PIN_LED_BUTTON_SW
    Serial.println("[LIGHT_SLEEP] Wake on BOOT (GPIO 0) or LED button (GPIO 44). Entering light sleep.");
    #else
    Serial.println("[LIGHT_SLEEP] Wake on BOOT (GPIO 0). Entering light sleep.");
    #endif
    Serial.flush();
    delay(200);
    esp_light_sleep_start();

    // Light sleep returns here on wake.
    Serial.println("[LIGHT_SLEEP] Woke up — restarting for clean state.");
    gpio_wakeup_disable(GPIO_NUM_0);
    #ifdef PIN_LED_BUTTON_SW
    gpio_wakeup_disable((gpio_num_t)PIN_LED_BUTTON_SW);
    #endif
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

// ============================================================================
// PIN PAD SCREEN  (landscape 480×320)
// ============================================================================
//
// Horizontal layout:
//   Left panel  0..209  — "Enter PIN", 4 dots, attempt counter, Cancel button
//   Divider     x=209   — 1px vertical line
//   Right panel 210..479 — 3×4 numpad grid
//
// Touch coordinates from AXS15231B are already in landscape space (0..479, 0..319),
// matching the canvas coordinate system used here.

// ── Layout constants — landscape 480×320 ─────────────────────────────────────
static const int PP_LEFT_W   = 210;
static const int PP_LEFT_CX  = PP_LEFT_W / 2;   // = 105
static const int PP_NP_X     = 210;              // numpad left edge
static const int PP_NP_COL_W = 90;              // 3 cols × 90 = 270 px
static const int PP_NP_ROW_H = 80;              // 4 rows × 80 = 320 px
static const int PP_BTN_M    = 5;               // margin inside each cell
static const int PP_BTN_W    = PP_NP_COL_W - 2 * PP_BTN_M;  // 80
static const int PP_BTN_H    = PP_NP_ROW_H - 2 * PP_BTN_M;  // 70
static const int PP_DOT_Y    = 115;             // y-center of dot row
static const int PP_DOT_HALF = 9;              // half-size of dot square (18×18)
static const int PP_DOT_GAP  = 38;             // dot center-to-center spacing

// ── Layout constants — portrait 320×480 ──────────────────────────────────────
static const int PP_V_TOP_H      = 205;  // top panel ends here; numpad below
static const int PP_V_COL_W      = 106;  // 3 × 106 = 318 ≈ 320
static const int PP_V_ROW_H      = 68;   // 4 × 68 = 272; starts at 205, ends at 477
static const int PP_V_BTN_M      = 4;
static const int PP_V_BTN_W      = PP_V_COL_W - 2 * PP_V_BTN_M;  // 98
static const int PP_V_BTN_H      = PP_V_ROW_H - 2 * PP_V_BTN_M;  // 60
static const int PP_V_DOT_Y      = 80;   // y-center of dot row
static const int PP_V_DOT_GAP    = 44;   // center-to-center spacing
static const int PP_V_CANCEL_Y   = 163;  // cancel button top y
static const int PP_V_CANCEL_H   = 35;   // cancel button height

// Numpad key labels (row, col)
static const char *kPinLabels[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"C","0","<"},
};

// ── Helpers ──────────────────────────────────────────────────────────────────

// Draw a rectangle border (4 filled strips).
static void drawRectBorder(int x, int y, int w, int h, int t, uint16_t color) {
    fillRect(x,       y,       w,       t,       color);
    fillRect(x,       y+h-t,   w,       t,       color);
    fillRect(x,       y+t,     t,       h-2*t,   color);
    fillRect(x+w-t,   y+t,     t,       h-2*t,   color);
}

// Draw a single PIN dot. Filled square = digit entered; outlined square = empty.
static void drawPinDot(int cx, int cy, bool filled, uint16_t fg, uint16_t bg) {
    int h = PP_DOT_HALF;
    fillRect(cx-h, cy-h, 2*h, 2*h, filled ? fg : bg);
    if (!filled) drawRectBorder(cx-h, cy-h, 2*h, 2*h, 2, fg);
}

// Draw error message (word-wrapped, size 2, white on red background).
// Number of lines is derived from the rect height so the box is fully used.
static void drawPinError(const String &msg, int rectX, int rectY, int rectW, int rectH, int cx) {
    fillRect(rectX, rectY, rectW, rectH, TFT_RED);
    const int sz      = 2;
    const int charW   = 6 * sz;   // 12px per char at size 2
    const int lineH   = 8 * sz;   // 16px per line
    const int lineGap = 2;
    const int maxChars = rectW / charW;
    const int kMaxLines = 6;
    int maxLines = (rectH + lineGap) / (lineH + lineGap);
    if (maxLines < 1) maxLines = 1;
    if (maxLines > kMaxLines) maxLines = kMaxLines;

    // Split on explicit newlines, then word-wrap each segment
    String lines[kMaxLines];
    int numLines = 0;
    String rem = msg;
    while (numLines < maxLines && rem.length() > 0) {
        int nl = rem.indexOf('\n');
        if (nl >= 0 && nl <= maxChars) {
            lines[numLines++] = rem.substring(0, nl);
            rem = rem.substring(nl + 1);
        } else if (nl < 0 && (int)rem.length() <= maxChars) {
            lines[numLines++] = rem;
            rem = "";
        } else {
            int split = maxChars;
            while (split > 0 && rem[split] != ' ') split--;
            if (split == 0) {
                lines[numLines++] = rem.substring(0, maxChars);
                rem = rem.substring(maxChars);
            } else {
                lines[numLines++] = rem.substring(0, split);
                rem = rem.substring(split + 1);
            }
        }
    }
    // Message longer than the box — mark the last line as truncated
    if (rem.length() > 0 && numLines > 0) {
        String &last = lines[numLines - 1];
        if ((int)last.length() > maxChars - 3) last = last.substring(0, maxChars - 3);
        last += "...";
    }

    int totalH = numLines * lineH + (numLines - 1) * lineGap;
    int startY = rectY + (rectH - totalH) / 2 + lineH / 2;
    for (int i = 0; i < numLines; i++) {
        drawCenter(cx, startY + i * (lineH + lineGap),
                   lines[i].c_str(), TFT_WHITE, TFT_RED, sz);
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

void showPinPadScreen(const PinPadState &state) {
    DisplayLock l;
    if (!_gfx) return;
    fillScreen(themeBackground);

    if (isPortrait()) {
        // ── Portrait: top info panel (0..PP_V_TOP_H) + numpad below ─────────

        // Horizontal divider
        fillRect(0, PP_V_TOP_H - 1, PANEL_W, 1, themeForeground);

        // "Enter PIN" / "Teach PIN"
        drawCenter(PANEL_W / 2, 28, state.teachMode ? "Teach PIN" : "Enter PIN",
                   themeForeground, themeBackground, 3);

        // PIN dots (4 for payment, 6 for teach) centered in top panel
        int firstDotX = PANEL_W / 2 - (state.maxDigits - 1) * PP_V_DOT_GAP / 2;
        for (int i = 0; i < state.maxDigits; i++) {
            drawPinDot(firstDotX + i * PP_V_DOT_GAP, PP_V_DOT_Y,
                       i < state.numDigits, themeForeground, themeBackground);
        }

        // Attempt counter / error
        if (state.attemptNum > 0 && !state.showError) {
            char buf[24];
            snprintf(buf, sizeof(buf), "Attempt %d of %d",
                     state.attemptNum, state.maxAttempts);
            drawCenter(PANEL_W / 2, 120, buf, TFT_RED, themeBackground, 2);
        }
        if (state.showError) {
            bool isTimeout = state.errorMsg.indexOf("timed out") >= 0;
            if (state.attemptNum > 0 && !isTimeout) {
                char buf[24];
                snprintf(buf, sizeof(buf), "Attempt %d of %d",
                         state.attemptNum, state.maxAttempts);
                drawCenter(PANEL_W / 2, 108, buf, TFT_RED, themeBackground, 2);
            }
            String displayMsg = isTimeout ? "PIN entry\ntimed out." : state.errorMsg;
            // Error box extends over the Cancel button area (4 lines).
            // The button is not drawn while the error shows — it reappears on
            // the auto-dismiss redraw, and its touch zone keeps working.
            drawPinError(displayMsg, 4, 120, PANEL_W - 8, PP_V_TOP_H - 124, PANEL_W / 2);
        }

        // Cancel button (hidden while the error message uses its space)
        if (!state.showError) {
            drawRectBorder(20, PP_V_CANCEL_Y, PANEL_W - 40, PP_V_CANCEL_H, 2, themeForeground);
            drawCenter(PANEL_W / 2, PP_V_CANCEL_Y + PP_V_CANCEL_H / 2,
                       "CANCEL", themeForeground, themeBackground, 2);
        }

        // ── Numpad ───────────────────────────────────────────────────────────
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = col * PP_V_COL_W + PP_V_BTN_M;
                int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
                drawRectBorder(bx, by, PP_V_BTN_W, PP_V_BTN_H, 2, themeForeground);
                drawCenter(bx + PP_V_BTN_W / 2, by + PP_V_BTN_H / 2,
                           kPinLabels[row][col], themeForeground, themeBackground, 4);
            }
        }

    } else {
        // ── Landscape: left info panel (0..PP_NP_X) + numpad right ──────────

        // Vertical divider
        fillRect(PP_NP_X - 1, 0, 1, SCR_H, themeForeground);

        // "Enter PIN" / "Teach PIN"
        drawCenter(PP_LEFT_CX, 38, state.teachMode ? "Teach PIN" : "Enter PIN",
                   themeForeground, themeBackground, 3);

        // PIN dots (4 for payment, 6 for teach). Shrink the spacing if the row
        // would otherwise collide with the left edge and the numpad divider —
        // 6 dots at the 4-digit gap overflow the narrow left panel.
        int dotGap = PP_DOT_GAP;
        const int maxSpan = PP_NP_X - 40;      // ~20px margin each side
        if ((state.maxDigits - 1) * dotGap > maxSpan)
            dotGap = maxSpan / (state.maxDigits - 1);
        int firstDotX = PP_LEFT_CX - (state.maxDigits - 1) * dotGap / 2;
        for (int i = 0; i < state.maxDigits; i++) {
            drawPinDot(firstDotX + i * dotGap, PP_DOT_Y,
                       i < state.numDigits, themeForeground, themeBackground);
        }

        if (state.attemptNum > 0 && !state.showError) {
            char buf[24];
            snprintf(buf, sizeof(buf), "Attempt %d of %d",
                     state.attemptNum, state.maxAttempts);
            drawCenter(PP_LEFT_CX, 162, buf, TFT_RED, themeBackground, 2);
        }
        if (state.showError) {
            bool isTimeout = state.errorMsg.indexOf("timed out") >= 0;
            if (state.attemptNum > 0 && !isTimeout) {
                char buf[24];
                snprintf(buf, sizeof(buf), "Attempt %d of %d",
                         state.attemptNum, state.maxAttempts);
                drawCenter(PP_LEFT_CX, 143, buf, TFT_RED, themeBackground, 2);
            }
            String displayMsg = isTimeout ? "PIN entry\ntimed out." : state.errorMsg;
            drawPinError(displayMsg, 4, 155, PP_LEFT_W - 8, 108, PP_LEFT_CX);
        }

        // Cancel button
        drawRectBorder(20, 268, PP_LEFT_W - 40, 38, 2, themeForeground);
        drawCenter(PP_LEFT_CX, 287, "CANCEL", themeForeground, themeBackground, 2);

        // ── Numpad ───────────────────────────────────────────────────────────
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
                int by = row * PP_NP_ROW_H + PP_BTN_M;
                drawRectBorder(bx, by, PP_BTN_W, PP_BTN_H, 2, themeForeground);
                drawCenter(bx + PP_BTN_W / 2, by + PP_BTN_H / 2,
                           kPinLabels[row][col], themeForeground, themeBackground, 4);
            }
        }
    }

    flushDisplay();
}

// Returns: 0-9=digit, 10=backspace(<), 11=clear(X), 12=cancel, -1=no hit.
int pinPadHitTest(uint16_t x, uint16_t y) {
    static const int kMap[4][3] = {{1,2,3},{4,5,6},{7,8,9},{11,0,10}};

    if (isPortrait()) {
        // Top info panel
        if (y < (uint16_t)PP_V_TOP_H) {
            if (y >= (uint16_t)PP_V_CANCEL_Y &&
                y <= (uint16_t)(PP_V_CANCEL_Y + PP_V_CANCEL_H)) return 12;
            return -1;
        }
        // Numpad
        int col = x / PP_V_COL_W;
        int row = (y - PP_V_TOP_H) / PP_V_ROW_H;
        if (col > 2 || row > 3) return -1;
        int bx = col * PP_V_COL_W + PP_V_BTN_M;
        int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
        if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_V_BTN_W)) return -1;
        if (y < (uint16_t)by  || y > (uint16_t)(by + PP_V_BTN_H)) return -1;
        return kMap[row][col];
    }

    // Landscape
    if (x < (uint16_t)PP_NP_X) {
        if (y >= 268 && y <= 306) return 12;
        return -1;
    }
    int col = (x - PP_NP_X) / PP_NP_COL_W;
    int row = y / PP_NP_ROW_H;
    if (col > 2 || row > 3) return -1;
    int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
    int by = row * PP_NP_ROW_H + PP_BTN_M;
    if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_BTN_W)) return -1;
    if (y < (uint16_t)by  || y > (uint16_t)(by + PP_BTN_H)) return -1;
    return kMap[row][col];
}

// ============================================================================
// MINI-POS SCREENS  (amount entry → invoice QR → paid)
// ============================================================================
//
// The amount entry screen reuses the PIN pad geometry: info panel + 3×4 numpad.
// Numpad bottom row is  < 0 .  (backspace, zero, decimal point).
// The info panel holds "Amount in", the currency, the amount display box and
// the INVOICE / LAST PAY buttons (same look as the PIN pad CANCEL button).

// Numpad key labels for Mini-PoS (row, col)
static const char *kMpLabels[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"<","0","."},
};
// Hit codes for Mini-PoS numpad: 10=backspace, 13=decimal point
static const int kMpMap[4][3] = {{1,2,3},{4,5,6},{7,8,9},{10,0,13}};

// Landscape button geometry (left panel, like PIN pad CANCEL)
static const int MP_INVOICE_Y = 215;   // y 215..253
static const int MP_LASTPAY_Y = 268;   // y 268..306 (same as PIN pad CANCEL)
static const int MP_BTN_H     = 38;
// Portrait button geometry (two buttons side by side in the top panel)
static const int MP_V_BTN_Y   = 158;   // y 158..198
static const int MP_V_BTN_H   = 40;

// Cancel button on the Mini-PoS QR screen (bottom-right corner)
static const int MP_QRC_W = 90, MP_QRC_H = 34;
// Corner margin for on-screen edge buttons (CANCEL, dual-page tab). Must be
// >= the 14 px physical-button reservation at each screen edge (see the
// inButtonArea check in the touch loop), otherwise the bottom/side of the
// button falls into that strip and its taps get swallowed before reaching the
// button handler.
static const int MP_QRC_M = 20;

// Battery charge, top-left of the portrait input screen. The header
// ("Amount in EUR", size 2, centred) starts at x≈82, so x 8..56 is free.
// Landscape has no free corner there — the numpad owns the full right half —
// so this is portrait only for now.
static void drawMiniPosBattery() {
    if (!batteryAvailable()) return;
    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", batteryPercent());
    int w = strlen(buf) * 6 * 2;   // 6 px per char at size 1, ×2 for size 2
    drawCenter(8 + w / 2, 22, buf, themeForeground, themeBackground, 2);
}

static void drawMiniPosAmountBox(int bx, int by, int bw, int bh) {
    drawRectBorder(bx, by, bw, bh, 2, themeForeground);
    uint16_t txtColor = miniPosState.amountLocked ? TFT_ORANGE : themeForeground;
    const char *txt = (miniPosState.numChars > 0) ? miniPosState.amount : "";
    if (*txt) {
        drawCenter(bx + bw / 2, by + bh / 2, txt, txtColor, themeBackground, 3);
    } else {
        String hint = miniPosConfig.decimal ? "0.00" : "0";
        drawCenter(bx + bw / 2, by + bh / 2, hint.c_str(), TFT_DARKGREY, themeBackground, 3);
    }
}

void showMiniPosInputScreen() {
    DisplayLock l;
    if (!_gfx) return;
    fillScreen(themeBackground);

    bool showInfo = miniPosState.infoMsg.length() > 0;

    if (isPortrait()) {
        // ── Portrait: top panel + numpad below (PIN pad geometry) ───────────
        fillRect(0, PP_V_TOP_H - 1, PANEL_W, 1, themeForeground);

        String header = "Amount in " + miniPosConfig.currency;
        drawCenter(PANEL_W / 2, 22, header.c_str(), themeForeground, themeBackground, 2);
        drawMiniPosBattery();

        drawMiniPosAmountBox(40, 45, PANEL_W - 80, 52);

        if (showInfo) {
            drawPinError(miniPosState.infoMsg, 4, 108, PANEL_W - 8, 40, PANEL_W / 2);
        }

        // INVOICE (left) and LAST PAY (right) side by side
        drawRectBorder(10,  MP_V_BTN_Y, 142, MP_V_BTN_H, 2, themeForeground);
        drawCenter(10 + 71, MP_V_BTN_Y + MP_V_BTN_H / 2, "INVOICE", themeForeground, themeBackground, 2);
        drawRectBorder(168, MP_V_BTN_Y, 142, MP_V_BTN_H, 2, themeForeground);
        drawCenter(168 + 71, MP_V_BTN_Y + MP_V_BTN_H / 2, "LAST PAY", themeForeground, themeBackground, 2);

        // Numpad
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = col * PP_V_COL_W + PP_V_BTN_M;
                int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
                bool isDot = (row == 3 && col == 2);
                uint16_t keyColor = (isDot && !miniPosConfig.decimal) ? TFT_DARKGREY : themeForeground;
                drawRectBorder(bx, by, PP_V_BTN_W, PP_V_BTN_H, 2, keyColor);
                drawCenter(bx + PP_V_BTN_W / 2, by + PP_V_BTN_H / 2,
                           kMpLabels[row][col], keyColor, themeBackground, 4);
            }
        }
    } else {
        // ── Landscape: left panel + numpad right (PIN pad geometry) ─────────
        fillRect(PP_NP_X - 1, 0, 1, SCR_H, themeForeground);

        drawCenter(PP_LEFT_CX, 28, "Amount in", themeForeground, themeBackground, 3);
        drawCenter(PP_LEFT_CX, 64, miniPosConfig.currency.c_str(), themeForeground, themeBackground, 3);

        drawMiniPosAmountBox(10, 95, PP_LEFT_W - 20, 46);

        if (showInfo) {
            drawPinError(miniPosState.infoMsg, 4, 155, PP_LEFT_W - 8, 48, PP_LEFT_CX);
        }

        drawRectBorder(20, MP_INVOICE_Y, PP_LEFT_W - 40, MP_BTN_H, 2, themeForeground);
        drawCenter(PP_LEFT_CX, MP_INVOICE_Y + MP_BTN_H / 2, "INVOICE", themeForeground, themeBackground, 2);
        drawRectBorder(20, MP_LASTPAY_Y, PP_LEFT_W - 40, MP_BTN_H, 2, themeForeground);
        drawCenter(PP_LEFT_CX, MP_LASTPAY_Y + MP_BTN_H / 2, "LAST PAY", themeForeground, themeBackground, 2);

        // Numpad
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
                int by = row * PP_NP_ROW_H + PP_BTN_M;
                bool isDot = (row == 3 && col == 2);
                uint16_t keyColor = (isDot && !miniPosConfig.decimal) ? TFT_DARKGREY : themeForeground;
                drawRectBorder(bx, by, PP_BTN_W, PP_BTN_H, 2, keyColor);
                drawCenter(bx + PP_BTN_W / 2, by + PP_BTN_H / 2,
                           kMpLabels[row][col], keyColor, themeBackground, 4);
            }
        }
    }

    flushDisplay();
}

// Returns: 0-9=digit, 10=backspace, 13=decimal point, 14=Invoice, 15=Last Pay,
// -1=no hit. The decimal key reports -1 when the decimal separator is disabled.
int miniPosHitTest(uint16_t x, uint16_t y) {
    if (isPortrait()) {
        if (y < (uint16_t)PP_V_TOP_H) {
            if (y >= (uint16_t)MP_V_BTN_Y && y <= (uint16_t)(MP_V_BTN_Y + MP_V_BTN_H)) {
                if (x >= 10 && x <= 152)  return 14;  // INVOICE
                if (x >= 168 && x <= 310) return 15;  // LAST PAY
            }
            return -1;
        }
        int col = x / PP_V_COL_W;
        int row = (y - PP_V_TOP_H) / PP_V_ROW_H;
        if (col > 2 || row > 3) return -1;
        int bx = col * PP_V_COL_W + PP_V_BTN_M;
        int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
        if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_V_BTN_W)) return -1;
        if (y < (uint16_t)by || y > (uint16_t)(by + PP_V_BTN_H)) return -1;
        int hit = kMpMap[row][col];
        if (hit == 13 && !miniPosConfig.decimal) return -1;
        return hit;
    }

    // Landscape
    if (x < (uint16_t)PP_NP_X) {
        if (y >= (uint16_t)MP_INVOICE_Y && y <= (uint16_t)(MP_INVOICE_Y + MP_BTN_H)) return 14;
        if (y >= (uint16_t)MP_LASTPAY_Y && y <= (uint16_t)(MP_LASTPAY_Y + MP_BTN_H)) return 15;
        return -1;
    }
    int col = (x - PP_NP_X) / PP_NP_COL_W;
    int row = y / PP_NP_ROW_H;
    if (col > 2 || row > 3) return -1;
    int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
    int by = row * PP_NP_ROW_H + PP_BTN_M;
    if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_BTN_W)) return -1;
    if (y < (uint16_t)by || y > (uint16_t)(by + PP_BTN_H)) return -1;
    int hit = kMpMap[row][col];
    if (hit == 13 && !miniPosConfig.decimal) return -1;
    return hit;
}

// QR screen with invoice: lines 1+2 from the LNbits pin-5 label, line 3 shows
// the invoice amount (e.g. "23.50 EUR"). Small CANCEL button bottom-right —
// only a touch on this button returns to the amount entry screen.
void showMiniPosQRScreen() {
    DisplayLock l;
    if (!_gfx) return;

    int activePin = (RELAY_CHANNEL_MAX > 0) ? RELAY_CHANNEL_PINS[0] : 5;
    int pinIndex = getPinIndex(activePin);
    String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0)
                    ? productLabels.labels[pinIndex]
                    : String("Mini-PoS");
    label = sanitizeLabel(label);
    String words[3];
    int wordCount;
    splitLabelWords(label, activePin, words, wordCount);
    // Third line always shows the invoice amount
    words[2] = sanitizeLabel(miniPosState.amountLine);
    wordCount = 3;

    uint16_t qrFg = themeForeground;
    uint16_t qrBg = themeBackground;
    if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

    // BOLT11 is bech32 (case-insensitive). Uppercase the QR content so the
    // encoder can use alphanumeric mode: QR version 11 then holds 468 chars
    // instead of 321 — needed for invoices with route hints. The NFC tag
    // keeps the original lowercase URI (written elsewhere).
    String qrText = String(lightningConfig.lightning);
    qrText.toUpperCase();

    fillScreen(qrBg);
    if (isPortrait()) {
        drawQRAt(qrText.c_str(), QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
        drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H - 44, words, wordCount, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    } else {
        drawQRAt(qrText.c_str(), QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
        drawLabelBox(words, wordCount, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    }
    flushDisplay();
}

bool miniPosQrCancelHit(uint16_t x, uint16_t y) {
    // Generous hit zone: the whole bottom-right corner, from a bit above/left of
    // the drawn button out to the screen edge. The drawn button stays small, but
    // the touch target is large so a quick tap registers reliably.
    const int pad = 26;
    int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
    return x >= (uint16_t)(cbx - pad) && y >= (uint16_t)(cby - pad);
}

void miniPosPaidScreen() {
    DisplayLock l;
    if (!_gfx) return;
    fillScreen(themeBackground);
    int cx = SCR_W / 2;
    int cy = SCR_H / 2;
    drawCenter(cx, cy - 30, "PAID", TFT_GREEN, themeBackground, 6);
    String line = miniPosState.amountLine;
    if (line.length() > 0) {
        drawCenter(cx, cy + 45, sanitizeLabel(line).c_str(), themeForeground, themeBackground, 3);
    }
    drawCenter(cx, cy + 95, "Thank you", themeForeground, themeBackground, 2);
    flushDisplay();
}

// ============================================================================
// NUMERICAL PRODUCT SELECTION SCREENS  (keypad → product QR)
// ============================================================================
//
// Same panel/numpad geometry as the PIN pad. Numpad bottom row is  < 0 OK
// (backspace, zero, confirm — the 5×7 font is ASCII-only, so no ✓ glyph).
// Everything uses the two theme colors only (errors excepted). The info
// panel holds the "Select product number" header, the entered number, a
// two-line error field and a small CANCEL button back to the main screen.

// Numpad key labels for product selection (row, col)
static const char *kPsLabels[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"<","0","OK"},
};
// Hit codes: 10=backspace, 11=OK confirm; the CANCEL button reports 12
static const int kPsMap[4][3] = {{1,2,3},{4,5,6},{7,8,9},{10,0,11}};

// Landscape CANCEL button geometry (left panel bottom, small)
static const int PS_CANCEL_X = 45,  PS_CANCEL_Y = 264;
static const int PS_CANCEL_W = 120, PS_CANCEL_H = 36;
// Portrait CANCEL button geometry (small, centered in the top panel)
static const int PS_V_CANCEL_X = 105, PS_V_CANCEL_Y = 160;
static const int PS_V_CANCEL_W = 110, PS_V_CANCEL_H = 32;

// Bold variant of drawCenter: second pass shifted 1 px with transparent
// background thickens the 5×7 strokes.
static void drawCenterBold(int cx, int cy, const char *s, uint16_t fg, uint16_t bg, uint8_t size) {
    int len = 0;
    for (const char *p = s; *p; p++) len++;
    int x = cx - (len * 6 * size) / 2;
    int y = cy - (8 * size) / 2;
    drawString(x, y, s, fg, bg, size);
    drawString(x + 1, y, s, fg, bg, size, true);
}

// Entered number, or a single "-" while empty (no surrounding box).
static void drawProductSelectNumber(int cx, int cy) {
    const char *txt = (productSelectState.numDigits > 0)
                      ? productSelectState.digits : "-";
    drawCenter(cx, cy, txt, themeForeground, themeBackground, 4);
}

void showProductSelectScreen() {
    DisplayLock l;
    if (!_gfx) return;
    fillScreen(themeBackground);

    bool showInfo = productSelectState.infoMsg.length() > 0;

    if (isPortrait()) {
        // ── Portrait: top panel + numpad below (PIN pad geometry) ───────────
        fillRect(0, PP_V_TOP_H - 1, PANEL_W, 1, themeForeground);

        drawCenterBold(PANEL_W / 2, 22, "Select product number", themeForeground, themeBackground, 2);
        drawProductSelectNumber(PANEL_W / 2, 58);

        if (showInfo) {
            drawPinError(productSelectState.infoMsg, 4, 84, PANEL_W - 8, 40, PANEL_W / 2);
        }

        drawRectBorder(PS_V_CANCEL_X, PS_V_CANCEL_Y, PS_V_CANCEL_W, PS_V_CANCEL_H, 2, themeForeground);
        drawCenter(PANEL_W / 2, PS_V_CANCEL_Y + PS_V_CANCEL_H / 2, "CANCEL", themeForeground, themeBackground, 2);

        // Numpad
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = col * PP_V_COL_W + PP_V_BTN_M;
                int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
                drawRectBorder(bx, by, PP_V_BTN_W, PP_V_BTN_H, 2, themeForeground);
                drawCenter(bx + PP_V_BTN_W / 2, by + PP_V_BTN_H / 2,
                           kPsLabels[row][col], themeForeground, themeBackground, 4);
            }
        }
    } else {
        // ── Landscape: left panel + numpad right (PIN pad geometry) ─────────
        fillRect(PP_NP_X - 1, 0, 1, SCR_H, themeForeground);

        drawCenterBold(PP_LEFT_CX, 30, "Select product", themeForeground, themeBackground, 2);
        drawCenterBold(PP_LEFT_CX, 54, "number", themeForeground, themeBackground, 2);
        drawProductSelectNumber(PP_LEFT_CX, 100);

        if (showInfo) {
            drawPinError(productSelectState.infoMsg, 4, 140, PP_LEFT_W - 8, 40, PP_LEFT_CX);
        }

        drawRectBorder(PS_CANCEL_X, PS_CANCEL_Y, PS_CANCEL_W, PS_CANCEL_H, 2, themeForeground);
        drawCenter(PP_LEFT_CX, PS_CANCEL_Y + PS_CANCEL_H / 2, "CANCEL", themeForeground, themeBackground, 2);

        // Numpad
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
                int by = row * PP_NP_ROW_H + PP_BTN_M;
                drawRectBorder(bx, by, PP_BTN_W, PP_BTN_H, 2, themeForeground);
                drawCenter(bx + PP_BTN_W / 2, by + PP_BTN_H / 2,
                           kPsLabels[row][col], themeForeground, themeBackground, 4);
            }
        }
    }

    flushDisplay();
}

// Returns: 0-9=digit, 10=backspace, 11=OK confirm, 12=CANCEL, -1=no hit.
int productSelectHitTest(uint16_t x, uint16_t y) {
    if (isPortrait()) {
        if (y < (uint16_t)PP_V_TOP_H) {
            if (x >= (uint16_t)PS_V_CANCEL_X && x <= (uint16_t)(PS_V_CANCEL_X + PS_V_CANCEL_W) &&
                y >= (uint16_t)PS_V_CANCEL_Y && y <= (uint16_t)(PS_V_CANCEL_Y + PS_V_CANCEL_H)) return 12;
            return -1;
        }
        int col = x / PP_V_COL_W;
        int row = (y - PP_V_TOP_H) / PP_V_ROW_H;
        if (col > 2 || row > 3) return -1;
        int bx = col * PP_V_COL_W + PP_V_BTN_M;
        int by = PP_V_TOP_H + row * PP_V_ROW_H + PP_V_BTN_M;
        if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_V_BTN_W)) return -1;
        if (y < (uint16_t)by || y > (uint16_t)(by + PP_V_BTN_H)) return -1;
        return kPsMap[row][col];
    }

    // Landscape
    if (x < (uint16_t)PP_NP_X) {
        if (x >= (uint16_t)PS_CANCEL_X && x <= (uint16_t)(PS_CANCEL_X + PS_CANCEL_W) &&
            y >= (uint16_t)PS_CANCEL_Y && y <= (uint16_t)(PS_CANCEL_Y + PS_CANCEL_H)) return 12;
        return -1;
    }
    int col = (x - PP_NP_X) / PP_NP_COL_W;
    int row = y / PP_NP_ROW_H;
    if (col > 2 || row > 3) return -1;
    int bx = PP_NP_X + col * PP_NP_COL_W + PP_BTN_M;
    int by = row * PP_NP_ROW_H + PP_BTN_M;
    if (x < (uint16_t)bx || x > (uint16_t)(bx + PP_BTN_W)) return -1;
    if (y < (uint16_t)by || y > (uint16_t)(by + PP_BTN_H)) return -1;
    return kPsMap[row][col];
}

// Product QR screen opened via GO: standard product QR layout plus a small
// CANCEL button (same geometry as the Mini-PoS QR screen) to return to the
// keypad panel.
void showProductSelectQRScreen(String label, int pin) {
    DisplayLock l;
    if (!_gfx) return;
    label = sanitizeLabel(label);
    String words[3];
    int wordCount;
    splitLabelWords(label, pin, words, wordCount);

    uint16_t qrFg = themeForeground;
    uint16_t qrBg = themeBackground;
    if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

    fillScreen(qrBg);
    if (isPortrait()) {
        drawQRAt(lightningConfig.lightning, QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
        drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H - 44, words, wordCount, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    } else {
        drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
        drawLabelBox(words, wordCount, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    }
    flushDisplay();
}

bool productSelectQrCancelHit(uint16_t x, uint16_t y) {
    return miniPosQrCancelHit(x, y);  // identical button geometry
}

// Authy teach screen: registration QR (from requestAuthLnurl) with a "Learning
// Identities" indicator and a dedicated CANCEL button bottom-right (same look as
// the Mini-PoS invoice screen). Touching anywhere else keeps teaching — only the
// CANCEL button ends the session. Landscape shows the indicator in the QR label
// box; portrait shows it top-left so the CANCEL button keeps the bottom-right.
void showAuthTeachScreen(String label, int pin) {
    DisplayLock l;
    if (!_gfx) return;
    label = sanitizeLabel(label);
    String words[3];
    int wordCount;
    splitLabelWords(label, pin, words, wordCount);

    uint16_t qrFg = themeForeground;
    uint16_t qrBg = themeBackground;
    if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

    fillScreen(qrBg);
    if (isPortrait()) {
        // "Learning Identities" top-left (two lines), QR + CANCEL below.
        drawString(10, 12, "Learning", qrFg, qrBg, 2);
        drawString(10, 34, "Identities", qrFg, qrBg, 2);
        drawQRAt(lightningConfig.lightning, QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    } else {
        drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
        drawLabelBox(words, wordCount, qrFg, qrBg);
        int cbx = SCR_W - MP_QRC_W - MP_QRC_M, cby = SCR_H - MP_QRC_H - MP_QRC_M;
        drawRectBorder(cbx, cby, MP_QRC_W, MP_QRC_H, 2, qrFg);
        drawCenter(cbx + MP_QRC_W / 2, cby + MP_QRC_H / 2, "CANCEL", qrFg, qrBg, 2);
    }
    flushDisplay();
}

bool authTeachCancelHit(uint16_t x, uint16_t y) {
    return miniPosQrCancelHit(x, y);  // identical button geometry
}

// Greedy word-wrap of `s` into `out[]` (up to maxN entries) so each line fits
// within maxW pixels at the given font size. Returns number of lines written.
static int wrapWords(const String &s, uint8_t size, int maxW, String out[], int maxN) {
    int maxChars = maxW / (6 * size);
    if (maxChars < 1) maxChars = 1;
    int n = 0;
    int start = 0;
    while (start < (int)s.length() && n < maxN) {
        int remaining = s.length() - start;
        if (remaining <= maxChars) {
            out[n++] = s.substring(start);
            break;
        }
        int split = -1;
        for (int i = start + maxChars; i > start; i--) {
            if (s[i] == ' ') { split = i; break; }
        }
        if (split < 0) split = start + maxChars;
        out[n++] = s.substring(start, split);
        start = split + (s[split] == ' ' ? 1 : 0);
    }
    return n;
}

void showAuthPinError(const String &l1, const String &l2, const String &l3) {
    DisplayLock lock;
    if (!_gfx) return;
    const uint16_t errBg = TFT_WHITE;
    const int maxLines = 4;
    int sz = 3;
    int maxW = SCR_W - 20;
    String lines[maxLines];
    int n = 0;
    const String *src[3] = { &l1, &l2, &l3 };
    for (int i = 0; i < 3 && n < maxLines; i++) {
        if (src[i]->length() == 0) continue;
        n += wrapWords(*src[i], sz, maxW, lines + n, maxLines - n);
    }
    // Long single-word error strings can still overflow at size 3 — drop to a
    // smaller size and re-wrap so text never runs off the screen edges.
    if (n >= maxLines) {
        sz = 2;
        n = 0;
        for (int i = 0; i < 3 && n < maxLines; i++) {
            if (src[i]->length() == 0) continue;
            n += wrapWords(*src[i], sz, maxW, lines + n, maxLines - n);
        }
    }

    const int lineH = 8 * sz + 10;
    int totalH = n * lineH;
    // Landscape: center at SCR_H/2; portrait: upper third so it sits over the QR
    int centerY = isPortrait() ? SCR_H / 3 : SCR_H / 2;
    int startY  = centerY - totalH / 2;
    fillRect(0, startY - 10, SCR_W, totalH + 20, errBg);
    int y = startY + lineH / 2;
    for (int i = 0; i < n; i++) {
        drawCenter(SCR_W / 2, y, lines[i].c_str(), TFT_RED, errBg, sz);
        y += lineH;
    }
    flushDisplay();
}

void showAuthToast(const String &msg, bool isError) {
    DisplayLock l;
    if (!_gfx) return;
    uint16_t fg = isError ? TFT_RED : TFT_GREEN;
    uint16_t bg = themeBackground;

    // Server error strings (e.g. "tagid not configured on this device.") can be
    // wider than the screen at readable sizes — wrap onto a second line rather
    // than letting drawCenter overflow off both edges.
    uint8_t size = 2;
    int maxW = SCR_W - 16;
    String l1 = msg, l2 = "";
    if ((int)msg.length() * 6 * size > maxW) {
        int maxChars = maxW / (6 * size);
        int split = -1;
        for (int i = maxChars; i > 0; i--) {
            if (msg[i] == ' ') { split = i; break; }
        }
        if (split < 0) split = maxChars;
        l1 = msg.substring(0, split);
        l2 = msg.substring(split + (msg[split] == ' ' ? 1 : 0));
        if ((int)l2.length() * 6 * size > maxW) l2 = l2.substring(0, maxChars);
    }

    int lineH = 8 * size;
    int toastH = l2.length() > 0 ? lineH * 2 + 8 : 30;
    int toastY = SCR_H - MP_QRC_H - MP_QRC_M - toastH - 4;
    fillRect(0, toastY, SCR_W, toastH, bg);
    if (l2.length() > 0) {
        drawCenter(SCR_W / 2, toastY + toastH / 2 - lineH / 2 - 2, l1.c_str(), fg, bg, size);
        drawCenter(SCR_W / 2, toastY + toastH / 2 + lineH / 2 + 2, l2.c_str(), fg, bg, size);
    } else {
        drawCenter(SCR_W / 2, toastY + toastH / 2, l1.c_str(), fg, bg, size);
    }
    flushDisplay();
}

// ── Authy dual-page tab button (bottom-right corner) ────────────────────────
// Mirrors the CANCEL button geometry in the same (right) corner. Wide enough
// for the "pay login >" / "< ID login" labels.
static const int AT_TAB_W = 150, AT_TAB_H = 34;
static void drawAuthTab(const char *txt, uint16_t fg, uint16_t bg) {
    int tx = SCR_W - AT_TAB_W - MP_QRC_M, ty = SCR_H - AT_TAB_H - MP_QRC_M;
    drawRectBorder(tx, ty, AT_TAB_W, AT_TAB_H, 2, fg);
    drawCenter(tx + AT_TAB_W / 2, ty + AT_TAB_H / 2, txt, fg, bg, 2);
}
bool authTabHit(uint16_t x, uint16_t y) {
    // Generous hit zone: the whole bottom-right corner, kept clear of the
    // physical-button edge strip via MP_QRC_M.
    const int pad = 26;
    int tx = SCR_W - AT_TAB_W - MP_QRC_M, ty = SCR_H - AT_TAB_H - MP_QRC_M;
    return x >= (uint16_t)(tx - pad) && y >= (uint16_t)(ty - pad);
}

// Shared: QR (from lightningConfig.lightning) + label box + a bottom-left tab.
// Used by both dual-page QR pages. In portrait the label box is shortened so it
// does not collide with the tab.
static void drawAuthQrWithTab(String label, int pin, const char *tabTxt) {
    label = sanitizeLabel(label);
    String words[3];
    int wordCount;
    splitLabelWords(label, pin, words, wordCount);

    uint16_t qrFg = themeForeground;
    uint16_t qrBg = themeBackground;
    if (themeInvertQr()) { qrFg = themeBackground; qrBg = themeForeground; }

    fillScreen(qrBg);
    if (isPortrait()) {
        drawQRAt(lightningConfig.lightning, QR_V_X, QR_V_Y, QR_V_MOD, qrFg, qrBg);
        drawLabelBoxAt(BOX_V_X, BOX_V_Y, BOX_V_W, BOX_V_H - 44, words, wordCount, qrFg, qrBg);
    } else {
        drawQRAt(lightningConfig.lightning, QR_X, QR_Y, QR_MOD_SIZE, qrFg, qrBg);
        drawLabelBox(words, wordCount, qrFg, qrBg);
    }
    drawAuthTab(tabTxt, qrFg, qrBg);
    flushDisplay();
}

// Dual-page identity-trigger QR: the auth login QR plus a "pay login >" tab
// (bottom-left) that switches to the classic payment page.
void showAuthIdentityScreen(String label, int pin) {
    DisplayLock l;
    if (!_gfx) return;
    drawAuthQrWithTab(label, pin, "pay login >");
}

// Dual-page payment screen: the classic ZapBox product QR for the auth pin plus
// a "< ID login" tab (bottom-left) that switches back to the identity QR.
void showAuthPayScreen(String label, int pin) {
    DisplayLock l;
    if (!_gfx) return;
    drawAuthQrWithTab(label, pin, "< ID login");
}

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
