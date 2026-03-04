#ifdef ENABLE_DISPLAY

#include <qrcode.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>
#include "Display.h"
#include "PinConfig.h"
#include "GlobalState.h"

TFT_eSPI tft = TFT_eSPI();
#define GFXFF 1

// ============================================================================
// DISPLAY MUTEX - Thread-safe SPI access
// ============================================================================
// TFT_eSPI is NOT thread-safe. Without this mutex, concurrent SPI access from
// Core 0 (Task1: button callbacks, touch, navigation) and Core 1 (loop: payment
// screens, NFC monitoring, ticker updates) causes display corruption (horizontal
// stripes, garbled pixels, wrong colors).
//
// Uses RECURSIVE mutex because display functions call each other:
//   showQRScreen() -> showProductQRScreen() -> drawQRCode()
//   redrawQRScreen() -> btctickerScreen() / showProductQRScreen() / etc.
// A regular mutex would deadlock on these nested calls from the same task.
// ============================================================================
static SemaphoreHandle_t displayMutex = NULL;

// RAII guard: automatically takes mutex in constructor, gives in destructor.
// Guarantees release even on early return statements within display functions.
class DisplayLock {
public:
  DisplayLock() : _locked(false) {
    if (displayMutex) {
      xSemaphoreTakeRecursive(displayMutex, portMAX_DELAY);
      _locked = true;
    }
  }
  ~DisplayLock() {
    if (_locked && displayMutex) {
      xSemaphoreGiveRecursive(displayMutex);
    }
  }
private:
  bool _locked;
};

void initDisplayMutex() {
  displayMutex = xSemaphoreCreateRecursiveMutex();
  if (!displayMutex) {
    Serial.println("[DISPLAY] FATAL: Failed to create display mutex!");
  } else {
    Serial.println("[DISPLAY] SPI mutex initialized (recursive)");
  }
}

// Bitcoin Logo (64x64 pixels)
const unsigned char bitcoin_logo[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0x80, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00,
	0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00,
	0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xfc, 0x7f, 0xff, 0xf0, 0x00,
	0x00, 0x1f, 0xff, 0xfc, 0x63, 0xff, 0xf8, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x63, 0xff, 0xfc, 0x00,
	0x00, 0x7f, 0xfe, 0x38, 0xe3, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0xe3, 0xff, 0xfe, 0x00,
	0x00, 0xff, 0xfe, 0x00, 0x03, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x7f, 0xff, 0x80,
	0x01, 0xff, 0xff, 0xc1, 0xe0, 0x3f, 0xff, 0x80, 0x01, 0xff, 0xff, 0x81, 0xf8, 0x1f, 0xff, 0x80,
	0x03, 0xff, 0xff, 0x83, 0xf8, 0x1f, 0xff, 0xc0, 0x03, 0xff, 0xff, 0x83, 0xf8, 0x1f, 0xff, 0xc0,
	0x03, 0xff, 0xff, 0x83, 0xf8, 0x1f, 0xff, 0xc0, 0x03, 0xff, 0xff, 0x01, 0xf0, 0x1f, 0xff, 0xc0,
	0x03, 0xff, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x03, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0xc0,
	0x03, 0xff, 0xff, 0x06, 0x00, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xfe, 0x07, 0xc0, 0x7f, 0xff, 0xc0,
	0x03, 0xff, 0xfe, 0x0f, 0xe0, 0x3f, 0xff, 0xc0, 0x03, 0xff, 0xfe, 0x0f, 0xf0, 0x3f, 0xff, 0xc0,
	0x03, 0xff, 0xec, 0x0f, 0xf0, 0x3f, 0xff, 0xc0, 0x03, 0xff, 0xe0, 0x0f, 0xf0, 0x3f, 0xff, 0xc0,
	0x01, 0xff, 0xc0, 0x0f, 0xf0, 0x3f, 0xff, 0x80, 0x01, 0xff, 0xc0, 0x00, 0x00, 0x3f, 0xff, 0x80,
	0x01, 0xff, 0xf8, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0x00,
	0x00, 0xff, 0xfe, 0x30, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xfe, 0x38, 0xc7, 0xff, 0xff, 0x00,
	0x00, 0x7f, 0xfe, 0x31, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xfc, 0x31, 0xff, 0xff, 0xfe, 0x00,
	0x00, 0x3f, 0xff, 0xf1, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x1f, 0xff, 0xf1, 0xff, 0xff, 0xf8, 0x00,
	0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00,
	0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00,
	0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00,
	0x00, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

int x;
int y;

// External variables not in GlobalState
extern String currency;

// Helper function to draw scaled bitmap
void drawScaledBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint8_t scale) {
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (bitmap[j * ((w + 7) / 8) + i / 8] & (128 >> (i & 7))) {
        // Draw scaled pixel as a block
        tft.fillRect(x + i * scale, y + j * scale, scale, scale, color);
      }
    }
  }
}

// Theme colors - will be set based on displayConfig.theme selection
uint16_t themeBackground = TFT_WHITE;
uint16_t themeForeground = TFT_BLACK;

// Helper function to ensure correct rotation is set
// Call this at the start of any screen drawing function to prevent rotation bugs
inline void ensureCorrectRotation() {
  if (displayConfig.orientation == "v"){
    tft.setRotation(0);
  } else if (displayConfig.orientation == "vi") {
    tft.setRotation(2);
  } else if (displayConfig.orientation == "hi") {
    tft.setRotation(3);
  } else {
    tft.setRotation(1); // Default: h (horizontal)
  }
}

// Available TFT_eSPI standard colors:
// Basic: TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
// Extended: TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE, TFT_PINK, TFT_GREENYELLOW
// Dark: TFT_DARKGREY, TFT_DARKGREEN, TFT_DARKCYAN, TFT_MAROON, TFT_PURPLE, TFT_OLIVE
// Light: TFT_LIGHTGREY, TFT_NAVY, TFT_BROWN
// Custom RGB565: 0xRRRR (5 bits red, 6 bits green, 5 bits blue)

// Theme configuration struct for efficient color lookup
// Each theme defines foreground (text/elements) and background colors
// Lookup is O(n) but n=16 is negligible; can easily be extended
struct ThemeConfig {
  const char* name;
  uint16_t foreground;
  uint16_t background;
};

// Theme lookup table - 16 color combinations
// Organized by: name (foreground-background)
// Colors come from TFT_eSPI standard palette (e.g., TFT_BLACK, TFT_WHITE, TFT_DARKGREEN)
// Lookup is performed via linear search in setThemeColors() - very fast for 16 items
const ThemeConfig themeConfigs[] = {
  {"black-white", TFT_BLACK, TFT_WHITE},
  {"black-darkcyan", TFT_BLACK, TFT_DARKCYAN},
  {"darkgreen-green", TFT_DARKGREEN, TFT_GREEN},
  {"darkgreen-lightgrey", TFT_DARKGREEN, TFT_LIGHTGREY},
  {"red-green", TFT_RED, TFT_GREEN},
  {"black-blue", TFT_BLACK, TFT_BLUE},
  {"orange-brown", TFT_ORANGE, TFT_BROWN},
  {"black-yellow", TFT_BLACK, TFT_YELLOW},
  {"black-btcorange", TFT_BLACK, 0xFCC0},
  {"btcorange-black", 0xFCC0, TFT_BLACK},
  {"darkgrey-btcorange", TFT_DARKGREY, 0xFCC0},
  {"zapbox", 0xFEA0, TFT_BLACK},
  {"maroon-magenta", TFT_MAROON, TFT_MAGENTA},
  {"black-red", TFT_BLACK, TFT_RED},
  {"brown-orange", TFT_BROWN, TFT_ORANGE},
  {"black-orange", TFT_BLACK, TFT_ORANGE},
  {"white-darkcyan", TFT_WHITE, TFT_DARKCYAN},
  {"white-navy", TFT_WHITE, TFT_NAVY},
  {"darkcyan-cyan", TFT_DARKCYAN, TFT_CYAN},
  {"black-olive", TFT_BLACK, TFT_OLIVE},
  {"black-darkgrey", TFT_BLACK, TFT_DARKGREY},
  {"black-lightgrey", TFT_BLACK, TFT_LIGHTGREY}
};

// Safe fillScreen wrapper to stabilize TFT refresh after heavy screen changes
// Adds a small delay to prevent controller glitches that can cause stripes/black screens
// ZAPBOX theme uses direct fillScreen without delay to avoid display corruption
inline void safeFillScreen(uint16_t color)
{
  tft.fillScreen(color);
  // ZAPBOX theme: No delay to prevent display controller corruption
  // Other themes: Small delay for stability
  if (displayConfig.theme != "zapbox" && displayConfig.theme != "btcorange-black") {
    delay(5);
  }
}

void setThemeColors()
{
  // Default fallback (used if theme name not found in table)
  themeForeground = TFT_BLACK;
  themeBackground = TFT_WHITE;
  
  // Linear search through theme lookup table
  // O(n) complexity with n=16 is negligible and much cleaner than if-else chains
  // Can be easily extended with new themes by adding entries to themeConfigs[]
  for (const auto& config : themeConfigs) {
    if (displayConfig.theme == config.name) {
      themeForeground = config.foreground;
      themeBackground = config.background;
      return;
    }
  }
  // If not found, defaults are already set above
}

void initDisplay()
{
  DisplayLock lock;
  tft.init();
  setThemeColors(); // Set displayConfig.theme colors based on configuration
  
  // Screen displayConfig.orientation mapping:
  // h = horizontal (button right)
  // v = vertical (button bottom)
  // hi = horizontal inverse (button left)
  // vi = vertical inverse (button top)
  if (displayConfig.orientation == "v"){
    tft.setRotation(0);
    x = 85;
    y = 160;
  } else if (displayConfig.orientation == "vi") {
    tft.setRotation(2);
    x = 85;
    y = 160;
  } else if (displayConfig.orientation == "hi") {
    tft.setRotation(3);
    x = 160;
    y = 85;
  } else {
    // Default: h (horizontal)
    tft.setRotation(1);
    x = 160;
    y = 85;
  }
}

// Startup
void startupScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.setTextSize(2);
    tft.drawString("", x + 5, y - 95, GFXFF);
    tft.setTextSize(8);
    tft.drawString("ZAP", x + 5, y - 70, GFXFF);
    tft.drawString("BOX", x + 5, y - 20, GFXFF);
    tft.setTextSize(2);
    tft.drawString("", x + 5, y + 15, GFXFF);
    tft.drawString("Firmware", x + 5, y + 35, GFXFF);
    tft.drawString(VERSION, x + 5, y + 55, GFXFF);
    tft.setTextSize(1);
    tft.drawString("", x + 5, y + 70, GFXFF);
    tft.setTextSize(2);
    tft.drawString("Powered", x + 5, y + 80, GFXFF);
    tft.drawString("by LNbits", x + 5, y + 100, GFXFF);
  } else {
    tft.setTextSize(6);
    tft.drawString("ZAPBOX", x + 5, y - 15, GFXFF);
    tft.setTextSize(2);
    tft.drawString("Firmware " VERSION, x, y + 25, GFXFF);
    tft.drawString("Powered by LNbits", x, y + 45, GFXFF);
  }
}

// Bitcoin Ticker Screen
void btctickerScreen()
{
  DisplayLock lock;
  // CRITICAL: Set rotation FIRST, before any drawing operations
  // This is especially important when switching from QR screens
  ensureCorrectRotation();
  
  // ZAPBOX/BTCORANGE theme color inversion fix
  // Problem: Inverted QR has YELLOW/ORANGE background, ticker has BLACK background
  // Need careful transition for color inversion (YELLOW/ORANGE -> BLACK)
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    // Transition from inverted QR (yellow/orange) to ticker (black)
    tft.fillScreen(themeBackground);  // Clear to black
    ensureCorrectRotation();
    delay(30);
    tft.fillScreen(themeBackground);  // Second clear for stability
    ensureCorrectRotation();
    delay(20);
  }
  
  safeFillScreen(themeBackground);
  
  // Explicitly clear QR code area to prevent ghosting when switching screens
  // QR code is typically drawn at (12, 12) with size ~72x72 pixels
  tft.fillRect(12, 12, 80, 80, themeBackground);
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    // Slight vertical offset for inverse orientation to lower logo and data
    int yOffset = (displayConfig.orientation == "vi") ? 10 : 0;
    // VERTICAL LAYOUT
    // Draw Bitcoin logo (64x64) moved up by 30 pixels more
    tft.drawBitmap(x - 32, y - 135 + yOffset, bitcoin_logo, 64, 64, themeForeground);
    
    // First line: Currency/BTC (closer to logo) - moved down 5 pixels
    tft.setTextSize(2);
    tft.drawString(currency + "/BTC", x + 5, y - 50 + yOffset, GFXFF);
    
    // Price (larger) - moved down 5 pixels
    tft.setTextSize(3);
    tft.drawString(bitcoinData.price, x + 5, y - 20 + yOffset, GFXFF);
    
    // Calculate sats per currency unit
    float priceFloat = bitcoinData.price.toFloat();
    String satsPerCurrency = "";
    if (priceFloat > 0) {
      long satsValue = (long)((1.0 / priceFloat) * 100000000.0);
      satsPerCurrency = String(satsValue);
    } else {
      satsPerCurrency = "0";
    }
    
    // New line: sats/Currency - moved down 5 pixels
    tft.setTextSize(2);
    tft.drawString("SAT/" + currency, x + 5, y + 15 + yOffset, GFXFF);
    
    // Sats value (larger) - moved down 5 pixels
    tft.setTextSize(3);
    tft.drawString(satsPerCurrency, x + 5, y + 45 + yOffset, GFXFF);
    
    // Block info at bottom (same spacing as above) - moved down 5 pixels
    tft.setTextSize(2);
    tft.drawString("Block", x + 5, y + 80 + yOffset, GFXFF);
    
    // Block number (larger, same size as price and sats) - moved down 5 pixels
    tft.setTextSize(3);
    tft.drawString(bitcoinData.blockHigh, x + 5, y + 110 + yOffset, GFXFF);
    
    // Button labels - different layout for touch vs non-touch
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    
    if (touchState.available) {
      // Touch version: HELP centered on bottom/top depending on displayConfig.orientation
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 2, 312, GFXFF); // Bottom (button at bottom)
      } else {
        tft.drawString("HELP", x + 2, 10, GFXFF); // Top for vi (button at top)
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 35, y + 150, GFXFF); // Right side bottom
        tft.drawString("NEXT", 5, y + 150, GFXFF); // Left side bottom
      } else {
        // vi: Mirror positions to top AND swap sides
        tft.drawString("HELP", 5, 10, GFXFF); // Left side top
        tft.drawString("NEXT", x + 35, 10, GFXFF); // Right side top
      }
    }
  } else {
    // HORIZONTAL LAYOUT
    // Left third: Bitcoin logo (64x64) vertically centered - moved 10 pixels right
    int logoX = (displayConfig.orientation == "hi") ? 25 : 20; // +5px for hi
    int logoY = y - 32;
    
    // Draw Bitcoin logo at normal size
    tft.drawBitmap(logoX, logoY, bitcoin_logo, 64, 64, themeForeground);
    
    // Right side (2/3): Text content - moved 20 more pixels to the left
    int textX = (displayConfig.orientation == "hi") ? x + 30 : x + 25; // +5px for hi
    
    // Calculate sats per currency unit
    float priceFloat = bitcoinData.price.toFloat();
    String satsPerCurrency = "";
    if (priceFloat > 0) {
      long satsValue = (long)((1.0 / priceFloat) * 100000000.0);
      satsPerCurrency = String(satsValue);
    } else {
      satsPerCurrency = "0";
    }
    
    // Top: Currency/BTC with price - label size 2, value size 3
    tft.setTextSize(2);
    String topLabel = currency + "/BTC: ";
    tft.drawString(topLabel + bitcoinData.price, textX, y - 40, GFXFF);
    
    // Middle: SAT per currency - label size 2, value size 3
    tft.setTextSize(2);
    String midLabel = "SAT/" + currency + ": ";
    tft.drawString(midLabel + satsPerCurrency, textX, y, GFXFF);
    
    // Bottom: Block height - label size 2, value size 3
    tft.setTextSize(2);
    tft.drawString("Block: " + bitcoinData.blockHigh, textX, y + 40, GFXFF);
    
    // Button labels - different layout for touch vs non-touch
    
    if (touchState.available) {
      // Touch version: HELP as vertical stacked letters on right/left side
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "h") {
        // Right side, top to bottom (button at right)
        tft.drawString("H", 311, y - 30, GFXFF);
        tft.drawString("E", 311, y - 10, GFXFF);
        tft.drawString("L", 311, y + 10, GFXFF);
        tft.drawString("P", 311, y + 30, GFXFF);
      } else {
        // Left side - same order as h
        tft.drawString("H", 11, y - 30, GFXFF);
        tft.drawString("E", 11, y - 10, GFXFF);
        tft.drawString("L", 11, y + 10, GFXFF);
        tft.drawString("P", 11, y + 30, GFXFF);
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "h") {
        tft.drawString("HELP", x + 110, 9, GFXFF); // Top right
        tft.drawString("NEXT", x + 110, 163, GFXFF); // Bottom right
      } else {
        // hi: Mirror positions to left side AND swap top/bottom
        tft.drawString("HELP", 5, 163, GFXFF); // Bottom left
        tft.drawString("NEXT", 5, 9, GFXFF); // Top left
      }
    }
  }
}

// Partial update of BTC ticker values - reduces flicker during auto-updates
// Only updates the dynamic values (price, sats, block) without redrawing the entire screen
void updateBtctickerValues()
{
  DisplayLock lock;
  // Only update text areas, don't redraw logo or buttons
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    int yOffset = (displayConfig.orientation == "vi") ? 10 : 0;
    
    // Calculate sats per currency
    float priceFloat = bitcoinData.price.toFloat();
    String satsPerCurrency = "";
    if (priceFloat > 0) {
      long satsValue = (long)((1.0 / priceFloat) * 100000000.0);
      satsPerCurrency = String(satsValue);
    } else {
      satsPerCurrency = "0";
    }
    
    // Clear and redraw price area
    tft.fillRect(0, y - 35 + yOffset, 170, 30, themeBackground);
    tft.setTextSize(3);
    tft.drawString(bitcoinData.price, x + 5, y - 20 + yOffset, GFXFF);
    
    // Clear and redraw sats area
    tft.fillRect(0, y + 30 + yOffset, 170, 30, themeBackground);
    tft.setTextSize(3);
    tft.drawString(satsPerCurrency, x + 5, y + 45 + yOffset, GFXFF);
    
    // Clear and redraw block area
    tft.fillRect(0, y + 95 + yOffset, 170, 30, themeBackground);
    tft.setTextSize(3);
    tft.drawString(bitcoinData.blockHigh, x + 5, y + 110 + yOffset, GFXFF);
    
  } else {
    // HORIZONTAL LAYOUT
    int textX = (displayConfig.orientation == "hi") ? x + 30 : x + 25;
    
    // Calculate sats per currency
    float priceFloat = bitcoinData.price.toFloat();
    String satsPerCurrency = "";
    if (priceFloat > 0) {
      long satsValue = (long)((1.0 / priceFloat) * 100000000.0);
      satsPerCurrency = String(satsValue);
    } else {
      satsPerCurrency = "0";
    }
    
    // Clear and redraw price line
    tft.fillRect(textX - 10, y - 55, 200, 30, themeBackground);
    tft.setTextSize(2);
    String topLabel = currency + "/BTC: ";
    tft.drawString(topLabel + bitcoinData.price, textX, y - 40, GFXFF);
    
    // Clear and redraw sats line
    tft.fillRect(textX - 10, y - 15, 200, 30, themeBackground);
    tft.setTextSize(2);
    String midLabel = "SAT/" + currency + ": ";
    tft.drawString(midLabel + satsPerCurrency, textX, y, GFXFF);
    
    // Clear and redraw block line
    tft.fillRect(textX - 10, y + 25, 200, 30, themeBackground);
    tft.setTextSize(2);
    tft.drawString("Block: " + bitcoinData.blockHigh, textX, y + 40, GFXFF);
  }
}

// Initialization Screen (shown during connection setup)
void initializationScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.setTextSize(2);
    tft.drawString("", x + 5, y - 95, GFXFF);
    tft.setTextSize(8);
    tft.drawString("ZAP", x + 5, y - 70, GFXFF);
    tft.drawString("BOX", x + 5, y - 20, GFXFF);
    tft.setTextSize(2);
    tft.drawString("", x + 5, y + 15, GFXFF);
    tft.drawString("Initializ.", x + 5, y + 35, GFXFF);
    tft.drawString("in", x + 5, y + 55, GFXFF);
    tft.setTextSize(1);
    tft.drawString("", x + 5, y + 70, GFXFF);
    tft.setTextSize(2);
    tft.drawString("progress", x + 5, y + 80, GFXFF);
    tft.drawString("...", x + 5, y + 100, GFXFF);
  } else {
    tft.setTextSize(6);
    tft.drawString("ZAPBOX", x + 5, y - 15, GFXFF);
    tft.setTextSize(2);
    tft.drawString("Initialization in", x, y + 25, GFXFF);
    tft.drawString("progress...", x, y + 45, GFXFF);
  }
}

// Boot-Up Screen (shown when waking from deep sleep or restarting)
void bootUpScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("BOOT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("STATUS", x - 55, y + 40, GFXFF);
    tft.drawString("BOOT", x - 55, y + 70, GFXFF);
    tft.drawString("UP", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("BOOT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("STATUS", x + 20, y - 30, GFXFF);
    tft.drawString("BOOT", x + 20, y, GFXFF);
    tft.drawString("UP", x + 20, y + 30, GFXFF);
  }
}

// Config Mode Screen
void configModeScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("CONF", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SERIAL", x - 55, y + 40, GFXFF);
    tft.drawString("CONFIG", x - 55, y + 70, GFXFF);
    tft.drawString("MODE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("CONF", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SERIAL", x + 20, y - 30, GFXFF);
    tft.drawString("CONFIG", x + 20, y, GFXFF);
    tft.drawString("MODE", x + 20, y + 30, GFXFF);
  }
}

// Error Report Screen
void errorReportScreen(uint8_t wifiCount, uint8_t internetCount, uint8_t serverCount, uint8_t websocketCount)
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("REPORT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2); // Reduced from 3 to 2 for 2-digit numbers
    tft.setTextColor(themeBackground);
    tft.drawString(String(wifiCount) + " x NW", x - 55, y + 25, GFXFF);
    tft.drawString(String(internetCount) + " x NI", x - 55, y + 55, GFXFF);
    tft.drawString(String(serverCount) + " x NS", x - 55, y + 85, GFXFF);
    tft.drawString(String(websocketCount) + " x NWS", x - 55, y + 115, GFXFF);
  } else {
    tft.drawString("REPORT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2); // Reduced from 3 to 2 for 2-digit numbers
    tft.setTextColor(themeBackground);
    tft.drawString(String(wifiCount) + " x NW", x + 20, y - 45, GFXFF);
    tft.drawString(String(internetCount) + " x NI", x + 20, y - 15, GFXFF);
    tft.drawString(String(serverCount) + " x NS", x + 20, y + 15, GFXFF);
    tft.drawString(String(websocketCount) + " x NWS", x + 20, y + 45, GFXFF);
  }
}

// WiFi Reconnect Screen
void wifiReconnectScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("FAULT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("WIFI", x - 55, y + 70, GFXFF);
    tft.drawString("", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("FAULT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x + 20, y - 30, GFXFF);
    tft.drawString("WIFI", x + 20, y, GFXFF);
    tft.drawString("", x + 20, y + 30, GFXFF);
  }
}

// Internet/Server Reconnect Screen
void internetReconnectScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("FAULT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("INTER", x - 55, y + 70, GFXFF);
    tft.drawString("NET", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("FAULT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x + 20, y - 30, GFXFF);
    tft.drawString("INTER", x + 20, y, GFXFF);
    tft.drawString("NET", x + 20, y + 30, GFXFF);
  }
}

// WebSocket Reconnect Screen
void serverReconnectScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("FAULT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("SERVER", x - 55, y + 85, GFXFF);
  } else {
    tft.drawString("FAULT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x + 20, y - 15, GFXFF);
    tft.drawString("SERVER", x + 20, y + 15, GFXFF);
  }
}

void websocketReconnectScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("FAULT", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("WEB", x - 55, y + 70, GFXFF);
    tft.drawString("SOCKET", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("FAULT", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("NO", x + 20, y - 30, GFXFF);
    tft.drawString("WEB", x + 20, y, GFXFF);
    tft.drawString("SOCKET", x + 20, y + 30, GFXFF);
  }
}

// Step one
void stepOneScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("1", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SELECT", x - 58, y + 40, GFXFF);
    tft.drawString("YOUR", x - 58, y + 70, GFXFF);
    tft.drawString("PRODUCT", x - 58, y + 100, GFXFF);
  } else {
    tft.drawString("1", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SELECT", x + 17, y - 30, GFXFF);
    tft.drawString("YOUR", x + 17, y, GFXFF);
    tft.drawString("PRODUCT", x + 17, y + 30, GFXFF);
  }
}

// Step two
void stepTwoScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("2", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SCAN", x - 58, y + 40, GFXFF);
    tft.drawString("QR", x - 58, y + 70, GFXFF);
    tft.drawString("CODE", x - 58, y + 100, GFXFF);
  } else {
    tft.drawString("2", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SCAN", x + 17, y - 30, GFXFF);
    tft.drawString("QR", x + 17, y, GFXFF);
    tft.drawString("CODE", x + 17, y + 30, GFXFF);
  }
}

// Step three
void stepThreeScreen()
{
  DisplayLock lock;
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("3", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("PAY", x - 58, y + 40, GFXFF);
    tft.drawString("IN-", x - 58, y + 70, GFXFF);
    tft.drawString("VOICE", x - 58, y + 100, GFXFF);
  } else {
    tft.drawString("3", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("PAY", x + 17, y - 30, GFXFF);
    tft.drawString("IN-", x + 17, y, GFXFF);
    tft.drawString("VOICE", x + 17, y + 30, GFXFF);
  }
}

// Switched ON screen
void actionTimeScreen()
{
  DisplayLock lock;
  // ZAPBOX theme color inversion fix: Reset display controller with double-clear
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    tft.fillScreen(TFT_BLACK);
    delay(10);
    tft.fillScreen(TFT_BLACK);
    delay(5);
  }
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
    // "ACTION" stacked letters in foreground color
    tft.setTextSize(4);
    tft.drawString("A", x + 5, y - 105, GFXFF);
    tft.drawString("C", x + 5, y - 70, GFXFF);
    tft.drawString("T", x + 5, y - 35, GFXFF);
    tft.drawString("I", x + 5, y, GFXFF);
    tft.drawString("O", x + 5, y + 35, GFXFF);
    tft.drawString("N", x + 5, y + 70, GFXFF);
    // "TIME" in inverted-color box (same style as PENDING NFC)
    tft.fillRect(15, y + 82, 140, 48, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(3);
    tft.drawString("TIME", x + 3, y + 105, GFXFF);
  } else {
    // "ACTION" in foreground color
    tft.setTextSize(6);
    tft.drawString("ACTION", x + 5, y - 15, GFXFF);
    // "TIME" in inverted-color box (same style as PENDING NFC)
    tft.fillRect(83, y + 16, 160, 52, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("TIME", x + 3, y + 43, GFXFF);
  }
}

// NFC payment pending screen
void nfcPendingScreen()
{
  DisplayLock lock;
  // ZAPBOX theme color inversion fix: Reset display controller with double-clear
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    tft.fillScreen(TFT_BLACK);
    delay(10);
    tft.fillScreen(TFT_BLACK);
    delay(5);
  }
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
    tft.setTextSize(3);
    tft.drawString("PENDING", x + 1, y - 50, GFXFF);
    // NFC in inverted-color box
    tft.fillRect(15, y + 10, 140, 62, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y + 41, GFXFF);
  } else {
    tft.setTextSize(4);
    tft.drawString("PENDING", x + 5, y - 25, GFXFF);
    // NFC in inverted-color box
    tft.fillRect(100, y + 10, 132, 55, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y + 37, GFXFF);
  }
}

void nfcNoLuckScreen()
{
  DisplayLock lock;
  // ZAPBOX theme color inversion fix
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    tft.fillScreen(TFT_BLACK);
    delay(10);
    tft.fillScreen(TFT_BLACK);
    delay(5);
  }
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
    // NFC in inverted-color box (top)
    tft.fillRect(15, y - 95, 140, 62, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y - 64, GFXFF);
    // NO LUCK below
    tft.setTextColor(themeForeground);
    tft.setTextSize(3);
    tft.drawString("NO LUCK", x + 1, y + 20, GFXFF);
  } else {
    // NFC in inverted-color box (left)
    tft.fillRect(100, y - 55, 132, 55, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y - 28, GFXFF);
    // NO LUCK below
    tft.setTextColor(themeForeground);
    tft.setTextSize(4);
    tft.drawString("NO LUCK", x + 5, y + 37, GFXFF);
  }
}

// NFC not supported by active extension (e.g. bitcoinswitch instead of zapbox)
void nfcNotSupportedScreen()
{
  DisplayLock lock;
  // ZAPBOX theme color inversion fix
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    tft.fillScreen(TFT_BLACK);
    delay(10);
    tft.fillScreen(TFT_BLACK);
    delay(5);
  }
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi") {
    // NFC in inverted-color box (top)
    tft.fillRect(15, y - 95, 140, 62, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y - 64, GFXFF);
    // "not" + "supported" below
    tft.setTextColor(themeForeground);
    tft.setTextSize(2);
    tft.drawString("not", x + 1, y + 0, GFXFF);
    tft.drawString("supported", x + 1, y + 30, GFXFF);
    tft.setTextSize(1);
    tft.drawString("use zapbox extension", x + 1, y + 65, GFXFF);
  } else {
    // NFC in inverted-color box (left)
    tft.fillRect(100, y - 55, 132, 55, themeForeground);
    tft.setTextColor(themeBackground);
    tft.setTextSize(4);
    tft.drawString("NFC", x + 5, y - 28, GFXFF);
    // "not supported" below
    tft.setTextColor(themeForeground);
    tft.setTextSize(2);
    tft.drawString("not supported", x + 5, y + 20, GFXFF);
    tft.setTextSize(1);
    tft.drawString("use zapbox extension", x + 5, y + 50, GFXFF);
  }
}

// Thank you
void thankYouScreen()
{
  DisplayLock lock;
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);
  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    tft.drawString("ty", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("ENJOY", x - 55, y + 40, GFXFF);
    tft.drawString("YOUR", x - 55, y + 70, GFXFF);
    tft.drawString("DAY", x - 55, y + 100, GFXFF);
  } else {
      tft.drawString("ty", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("ENJOY", x + 20, y - 30, GFXFF);
    tft.drawString("YOUR", x + 20, y, GFXFF);
    tft.drawString("DAY", x + 20, y + 30, GFXFF);
  }
}

// Show QR for ZAP action - uses product label from backend if available
void showQRScreen()
{
  DisplayLock lock;
  int pinIndex = getPinIndex(12);
  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0) ? productLabels.labels[pinIndex] : "READY 4 ZAP ACTION";
  showProductQRScreen(label, 12);
}

void drawQRCode()
{
  // Calculate offset for inverse orientations
  int offsetX = 12;
  int offsetY = 12;
  
  // Shift QR code 8 pixels right for horizontal inverse only
  if (displayConfig.orientation == "hi") {
    offsetX = 20;
  }
  
  // Shift QR code 7 pixels down for vertical inverse only
  if (displayConfig.orientation == "vi") {
    offsetY = 19;
  }
  
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 8, 0, lightningConfig.lightning);

  for (uint8_t y = 0; y < qrcoded.size; y++)
  {
    // Each horizontal module
    for (uint8_t x = 0; x < qrcoded.size; x++)
    {
      if (qrcode_getModule(&qrcoded, x, y))
      {
          tft.fillRect(offsetX + 3 * x, offsetY + 3 * y, 3, 3, themeForeground);
      }
      else
      {
          tft.fillRect(offsetX + 3 * x, offsetY + 3 * y, 3, 3, themeBackground);
      }
    }
  }
}

// Draw QRCode using explicit foreground/background colors (used for special themes)
void drawQRCodeWithColors(uint16_t fg, uint16_t bg)
{
  int offsetX = 12;
  int offsetY = 12;
  if (displayConfig.orientation == "hi") {
    offsetX = 20;
  }
  if (displayConfig.orientation == "vi") {
    offsetY = 19;
  }

  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 8, 0, lightningConfig.lightning);

  for (uint8_t y = 0; y < qrcoded.size; y++) {
    for (uint8_t x = 0; x < qrcoded.size; x++) {
      uint16_t color = qrcode_getModule(&qrcoded, x, y) ? fg : bg;
      tft.fillRect(offsetX + 3 * x, offsetY + 3 * y, 3, 3, color);
    }
  }
}

void showThresholdQRScreen()
{
  DisplayLock lock;
  tft.setTextDatum(ML_DATUM);
  safeFillScreen(themeBackground);
  tft.setTextSize(3);
  tft.setTextColor(themeBackground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    int boxY = (displayConfig.orientation == "vi") ? 175 : 168;
    tft.fillRect(15, boxY, 140, 132, themeForeground);
    tft.drawString("READY", x - 55, y + 40, GFXFF);
    tft.drawString("4 TH", x - 55, y + 70, GFXFF);
    tft.drawString("ACTION", x - 55, y + 100, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    if (!touchState.available && !externalButtonState.enabled) {
      // Only show HELP if not touch and external button not enabled
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 35, y + 150, GFXFF); // Right side bottom
      } else {
        tft.drawString("HELP", 5, 10, GFXFF); // Left side top for vi
      }
    }
  } else {
    int boxX = (displayConfig.orientation == "hi") ? 173 : 168;
    tft.fillRect(boxX, 18, 140, 135, themeForeground);
    int textOffset = (displayConfig.orientation == "hi") ? 25 : 20;
    tft.drawString("READY", x + textOffset, y - 30, GFXFF);
    tft.drawString("4 TH", x + textOffset, y, GFXFF);
    tft.drawString("ACTION", x + textOffset, y + 30, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    if (!touchState.available && !externalButtonState.enabled) {
      // Only show HELP if not touch and external button not enabled
      if (displayConfig.orientation == "h") {
        tft.drawString("HELP", x + 110, 9, GFXFF); // Top right
      } else {
        tft.drawString("HELP", 5, 163, GFXFF); // Bottom left for hi
      }
    }
  }

  drawQRCode();
}

void showSpecialModeQRScreen()
{
  DisplayLock lock;
  int pinIndex = getPinIndex(12);
  String label = (pinIndex >= 0 && productLabels.labels[pinIndex].length() > 0) ? productLabels.labels[pinIndex] : "READY 4 SP ACTION";
  showProductQRScreen(label, 12);
}

// Multi-Channel-Control Product QR Screen - displays label text and QR code
// Label can contain 1-3 words separated by spaces
void showProductQRScreen(String label, int pin)
{
  DisplayLock lock;
  // CRITICAL: Set rotation FIRST, before any drawing operations
  // This is especially important for themes with color inversion
  ensureCorrectRotation();
  
  // Select colors; invert for "zapbox" theme only on product QR screens
  uint16_t fg = themeForeground;
  uint16_t bg = themeBackground;
  if (displayConfig.theme == "zapbox") {
    fg = TFT_BLACK;
    bg = 0xFEA0;
  } else if (displayConfig.theme == "btcorange-black") {
    fg = TFT_BLACK;
    bg = 0xFCC0;
  }

  // Replace currency symbols with text abbreviations for better compatibility
  // GFXFF fonts only support ASCII, so we use standard abbreviations
  label.replace("€", "EUR");
  label.replace("$", "USD");
  label.replace("£", "GBP");
  label.replace("¥", "YEN");
  label.replace("₿", "BTC");
  label.replace("₹", "INR");
  label.replace("₽", "RUB");
  label.replace("¢", "ct");
  
  // Parse label: first word in line 1, second word in line 2, rest in line 3
  String words[3] = {"", "", ""};
  int wordCount = 0;
  
  // Find first space to get first word
  int firstSpace = label.indexOf(' ');
  if (firstSpace == -1) {
    // No spaces - entire label is one word
    words[0] = label;
    wordCount = 1;
  } else {
    // First word
    words[0] = label.substring(0, firstSpace);
    wordCount = 1;
    
    // Find second space to get second word
    int secondSpace = label.indexOf(' ', firstSpace + 1);
    if (secondSpace == -1) {
      // Only two words total
      words[1] = label.substring(firstSpace + 1);
      wordCount = 2;
    } else {
      // Second word
      words[1] = label.substring(firstSpace + 1, secondSpace);
      // Rest (everything after second space)
      words[2] = label.substring(secondSpace + 1);
      wordCount = 3;
    }
  }
  
  // If no words found (empty label), use pin number as fallback
  if (wordCount == 0 || words[0].length() == 0) {
    words[0] = "Pin " + String(pin);
    wordCount = 1;
  }

  // Now do all display operations - COMPLETE refresh like help screens
  // ZAPBOX/BTCORANGE theme color inversion fix
  // Problem: Ticker has BLACK background, inverted QR has YELLOW/ORANGE background
  // Display controller needs careful transition sequence for this complete color inversion
  // IMPORTANT: Must reset rotation after each fillScreen() to prevent rotation bugs
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    // Step 1: Clear to BLACK (ensures clean starting point from ticker)
    tft.fillScreen(TFT_BLACK);
    ensureCorrectRotation();
    delay(20);
    
    // Step 2: Transition to target background color (yellow or orange)
    tft.fillScreen(bg);
    ensureCorrectRotation();
    delay(30);
    
    // Step 3: Confirm with second fill of target color
    tft.fillScreen(bg);
    ensureCorrectRotation();
    delay(20);
  }
  
  safeFillScreen(bg);
  // CRITICAL: Reset rotation after safeFillScreen to prevent rotation bugs
  ensureCorrectRotation();
  
  // Draw QR code immediately after screen clear, before anything else
  if (displayConfig.theme == "zapbox" || displayConfig.theme == "btcorange-black") {
    drawQRCodeWithColors(fg, bg);
  } else {
    drawQRCode();
  }
  
  // CRITICAL: Reset rotation after QR drawing - the many fillRect() calls can affect rotation
  ensureCorrectRotation();
  
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    int boxY = (displayConfig.orientation == "vi") ? 175 : 168;
    tft.fillRect(15, boxY, 140, 132, fg);
    // CRITICAL: Reset rotation after large fillRect operation
    ensureCorrectRotation();
    
    // Display up to 3 lines of text
    tft.setTextSize(3);
    tft.setTextColor(bg);
    int startY = y + 40; // Starting Y position
    if (wordCount == 1) {
      tft.drawString(words[0], x - 58, startY + 30, GFXFF);
    } else if (wordCount == 2) {
      tft.drawString(words[0], x - 58, startY + 15, GFXFF);
      tft.drawString(words[1], x - 58, startY + 45, GFXFF);
    } else { // 3 words
      tft.drawString(words[0], x - 58, startY, GFXFF);
      tft.drawString(words[1], x - 58, startY + 30, GFXFF);
      tft.setTextSize(2); // Smaller font for third line (currency text)
      tft.drawString(words[2], x - 58, startY + 60, GFXFF);
    }
    
    // Button labels - different layout for touch vs non-touch
    tft.setTextSize(2);
    tft.setTextColor(fg);
    
    if (touchState.available) {
      // Touch version: HELP on bottom for v, top for vi (where touch button is)
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 2, 312, GFXFF);
      } else {
        tft.drawString("HELP", x + 2, 10, GFXFF);
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 35, y + 150, GFXFF);
        tft.drawString("NEXT", 5, y + 150, GFXFF);
      } else {
        tft.drawString("HELP", 5, 10, GFXFF);
        tft.drawString("NEXT", x + 35, 10, GFXFF);
      }
    }
  } else {
    int boxX = (displayConfig.orientation == "hi") ? 171 : 163;
    tft.fillRect(boxX, 18, 137, 135, fg);
    // CRITICAL: Reset rotation after large fillRect operation
    ensureCorrectRotation();
    
    // Display up to 3 lines of text
    tft.setTextSize(3);
    tft.setTextColor(bg);
    int startY = y - 30; // Starting Y position
    int textOffset = (displayConfig.orientation == "hi") ? 25 : 17;
    if (wordCount == 1) {
      tft.drawString(words[0], x + textOffset, startY + 30, GFXFF);
    } else if (wordCount == 2) {
      tft.drawString(words[0], x + textOffset, startY + 15, GFXFF);
      tft.drawString(words[1], x + textOffset, startY + 45, GFXFF);
    } else { // 3 words
      tft.drawString(words[0], x + textOffset, startY, GFXFF);
      tft.drawString(words[1], x + textOffset, startY + 30, GFXFF);
      tft.setTextSize(2); // Smaller font for third line (currency text)
      tft.drawString(words[2], x + textOffset, startY + 60, GFXFF);
    }
    
    // Button labels - different layout for touch vs non-touch
    tft.setTextSize(2);
    tft.setTextColor(fg);
    
    if (touchState.available) {
      // Touch version: HELP as vertical stacked letters on right/left side
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "h") {
        // Right side, top to bottom (button at right)
        tft.drawString("H", 311, y - 30, GFXFF);
        tft.drawString("E", 311, y - 10, GFXFF);
        tft.drawString("L", 311, y + 10, GFXFF);
        tft.drawString("P", 311, y + 30, GFXFF);
      } else {
        // Left side - same order as h
        tft.drawString("H", 11, y - 30, GFXFF);
        tft.drawString("E", 11, y - 10, GFXFF);
        tft.drawString("L", 11, y + 10, GFXFF);
        tft.drawString("P", 11, y + 30, GFXFF);
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "h") {
        tft.drawString("HELP", x + 110, 9, GFXFF);
        tft.drawString("NEXT", x + 110, 163, GFXFF);
      } else {
        tft.drawString("HELP", 5, 163, GFXFF);
        tft.drawString("NEXT", 5, 9, GFXFF);
      }
    }
  }
}

// Product Selection Screen - shown after 5 seconds of QR screen
void productSelectionScreen()
{
  DisplayLock lock;
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (displayConfig.orientation == "v" || displayConfig.orientation == "vi"){
    // Vertical displayConfig.orientation
    tft.setTextSize(2);
    tft.drawString("SELECT", x, y - 40, GFXFF);
    tft.drawString("PRODUCT", x, y - 20, GFXFF);
    
    // Draw navigation arrows
    tft.setTextSize(4);
    tft.drawString("<->", x, y + 30, GFXFF);
    
    // Draw instruction text
    tft.setTextSize(2);
    tft.drawString("NEXT", x, y + 60, GFXFF);
    
    // Button labels - different layout for touch vs non-touch
    tft.setTextSize(2);
    
    if (touchState.available) {
      // Touch version: HELP on bottom for v, top for vi (where touch button is)
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 2, 312, GFXFF); // Bottom (button at bottom)
      } else {
        tft.drawString("HELP", x + 2, 10, GFXFF); // Top for vi (button at top)
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "v") {
        tft.drawString("HELP", x + 35, y + 150, GFXFF); // Right side bottom
        tft.drawString("NEXT", 5, y + 150, GFXFF); // Left side bottom
      } else {
        // vi: Mirror positions to top AND swap sides (180° rotation)
        tft.drawString("HELP", 5, 10, GFXFF); // Left side top
        tft.drawString("NEXT", x + 35, 10, GFXFF); // Right side top
      }
    }
    
  } else {
    // Horizontal displayConfig.orientation
    tft.setTextSize(3);
    tft.drawString("SELECT", x, y - 30, GFXFF);
    tft.drawString("PRODUCT", x, y, GFXFF);
    
    // Draw navigation arrows
    tft.setTextSize(3);
    tft.drawString("<-NEXT->", x, y + 40, GFXFF);
    
    // Button labels - different layout for touch vs non-touch
    tft.setTextSize(2);
    
    if (touchState.available) {
      // Touch version: HELP as vertical stacked letters on right/left side
      tft.setTextDatum(MC_DATUM);
      if (displayConfig.orientation == "h") {
        // Right side, top to bottom (button at right)
        tft.drawString("H", 311, y - 30, GFXFF);
        tft.drawString("E", 311, y - 10, GFXFF);
        tft.drawString("L", 311, y + 10, GFXFF);
        tft.drawString("P", 311, y + 30, GFXFF);
      } else {
        // Left side - same order as h
        tft.drawString("H", 11, y - 30, GFXFF);
        tft.drawString("E", 11, y - 10, GFXFF);
        tft.drawString("L", 11, y + 10, GFXFF);
        tft.drawString("P", 11, y + 30, GFXFF);
      }
    } else if (!externalButtonState.enabled) {
      // Non-touch version: mirror labels for inverse displayConfig.orientation (only if external button is NOT enabled)
      tft.setTextDatum(ML_DATUM);
      if (displayConfig.orientation == "h") {
        tft.drawString("HELP", x + 110, 9, GFXFF); // Top right
        tft.drawString("NEXT", x + 110, 163, GFXFF); // Bottom right
      } else {
        // hi: Mirror positions to left side AND swap top/bottom (180° rotation)
        tft.drawString("HELP", 5, 163, GFXFF); // Bottom left
        tft.drawString("NEXT", 5, 9, GFXFF); // Top left
      }
    }
  }
}

// Screensaver management
static bool screensaverIsActive = false;
static String screensaverMode = "off";

void activateScreensaver(String mode)
{
  DisplayLock lock;
  Serial.println("[SCREENSAVER] Activating powerConfig.screensaver mode: " + mode);
  screensaverIsActive = true;
  screensaverMode = mode;

  if (mode == "black") {
    // Black screen - display stays on but shows black
    tft.fillScreen(TFT_BLACK);
    Serial.println("[SCREENSAVER] Black screen activated");
  } else if (mode == "backlight") {
    // Turn off backlight (most efficient while keeping display controller running)
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, LOW);
    Serial.println("[SCREENSAVER] Backlight turned off");
    
    // If channel 4 ambient light mode is enabled, turn off GPIO 11 when backlight is off
    if (channel4AmbientConfig.enabled) {
      pinMode(11, OUTPUT);
      digitalWrite(11, LOW);
      Serial.println("[SCREENSAVER] Channel 4 (GPIO 11) turned OFF (ambient light sync)");
    }
  }
}

void deactivateScreensaver()
{
  DisplayLock lock;
  if (!screensaverIsActive) {
    return;
  }

  Serial.println("[SCREENSAVER] Deactivating powerConfig.screensaver mode: " + screensaverMode);
  
  if (screensaverMode == "backlight") {
    // Turn backlight back on
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    Serial.println("[SCREENSAVER] Backlight turned on");
    
    // If channel 4 ambient light mode is enabled, turn on GPIO 11 when backlight is on
    if (channel4AmbientConfig.enabled) {
      pinMode(11, OUTPUT);
      digitalWrite(11, HIGH);
      Serial.println("[SCREENSAVER] Channel 4 (GPIO 11) turned ON (ambient light sync)");
    }
  }
  
  screensaverIsActive = false;
  screensaverMode = "off";
  
  // Screen will be redrawn by main loop
}

bool isScreensaverActive()
{
  return screensaverIsActive;
}

// Deep Sleep management
static bool deepSleepIsActive = false;
static String deepSleepMode = "off";

void prepareDeepSleep()
{
  DisplayLock lock;
  Serial.println("[DEEP_SLEEP] Preparing for deep sleep...");
  
  // Disable watchdog timers to prevent reset during sleep preparation
  esp_task_wdt_delete(NULL);
  Serial.println("[DEEP_SLEEP] Watchdog disabled");
  
  // Fill screen with black before sleep
  tft.fillScreen(TFT_BLACK);
  Serial.println("[DEEP_SLEEP] Screen cleared");
  
  // Turn off backlight to save power during sleep
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);
  Serial.println("[DEEP_SLEEP] Backlight turned OFF");
  
  // If channel 4 ambient light mode is enabled, turn off GPIO 11 when backlight is off
  if (channel4AmbientConfig.enabled) {
    pinMode(11, OUTPUT);
    digitalWrite(11, LOW);
    Serial.println("[DEEP_SLEEP] Channel 4 (GPIO 11) turned OFF (ambient light sync)");
  }
  
  // Longer delay to ensure all operations complete
  delay(500);
  
  Serial.println("[DEEP_SLEEP] Display prepared, ready for sleep");
}

void setupDeepSleepWakeup(String mode)
{
  Serial.println("[DEEP_SLEEP] Setting up wake-up sources, mode: " + mode);
  
  deepSleepIsActive = true;
  deepSleepMode = mode;
  
  // Configure power domain settings based on mode
  if (mode == "freeze") {
    // Deep sleep/Freeze: CPU off, only RTC active
    // WiFi/Bluetooth will be disconnected
    
    Serial.println("[DEEP_SLEEP] Configuring RTC GPIOs for wake-up...");
    
    // Initialize GPIO 0 (BOOT button) as RTC GPIO
    rtc_gpio_init(GPIO_NUM_0);
    rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    
    // Initialize GPIO 14 (HELP button) as RTC GPIO
    rtc_gpio_init(GPIO_NUM_14);
    rtc_gpio_set_direction(GPIO_NUM_14, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_14);
    rtc_gpio_pulldown_dis(GPIO_NUM_14);
    
    // Wait for pull-ups to stabilize
    delay(100);
    
    // Check current GPIO states
    int gpio0_state = rtc_gpio_get_level(GPIO_NUM_0);
    int gpio14_state = rtc_gpio_get_level(GPIO_NUM_14);
    Serial.printf("[DEEP_SLEEP] GPIO 0 (BOOT) level: %s\n", gpio0_state ? "HIGH" : "LOW");
    Serial.printf("[DEEP_SLEEP] GPIO 14 (HELP) level: %s\n", gpio14_state ? "HIGH" : "LOW");
    
    if (gpio0_state == 0) {
      Serial.println("[DEEP_SLEEP] ERROR: BOOT button is pressed! Aborting.");
      rtc_gpio_deinit(GPIO_NUM_0);
      rtc_gpio_deinit(GPIO_NUM_14);
      return;
    }
    if (gpio14_state == 0) {
      Serial.println("[DEEP_SLEEP] ERROR: HELP button is pressed! Aborting.");
      rtc_gpio_deinit(GPIO_NUM_0);
      rtc_gpio_deinit(GPIO_NUM_14);
      return;
    }
    
    // Configure EXT0 wake-up for GPIO 0 (BOOT button) - wake on LOW
    Serial.println("[DEEP_SLEEP] Setting EXT0 wake-up: GPIO 0 (BOOT), trigger on LOW");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    
    // Configure EXT1 wake-up for GPIO 14 (HELP button) - wake on LOW
    Serial.println("[DEEP_SLEEP] Setting EXT1 wake-up: GPIO 14 (HELP), trigger on LOW");
    esp_sleep_enable_ext1_wakeup(BIT64(GPIO_NUM_14), ESP_EXT1_WAKEUP_ANY_LOW);
    
    // Disable most power domains for maximum savings
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // Keep RTC periph for GPIO
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    
    Serial.println("[DEEP_SLEEP] Wake-up sources: BOOT button (GPIO 0) OR HELP button (GPIO 14)");
    Serial.println("[DEEP_SLEEP] WiFi will be disconnected");
    Serial.println("[DEEP_SLEEP] Entering Deep Sleep/Freeze (~0.01-0.15mA)");
    Serial.println("[DEEP_SLEEP] Press BOOT or HELP button to wake up (device will restart)");
    
    // Add delay and flush serial before deep sleep
    Serial.flush();
    delay(200);
    
    esp_deep_sleep_start();
    // Note: esp_deep_sleep_start() does not return - device restarts on wake
  }
}

bool isDeepSleepActive()
{
  return deepSleepIsActive;
}

// NFC Hardware Test Screen
// Shown when ENABLE_NFC=1 and ENABLE_NFC_TEST=1.
// Displays the raw LNURLW read from the Bolt Card so
// hardware functionality can be verified without a server.
void nfcTestScreen(String lnurlw)
{
  DisplayLock lock;
  safeFillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(3);
  tft.drawString("NFC OK!", x, y - 45, GFXFF);

  tft.setTextColor(themeForeground);
  tft.setTextSize(1);
  // Show first 24 chars of lnurlw (fits horizontal layout)
  String preview = lnurlw.substring(0, 24);
  tft.drawString(preview, x, y, GFXFF);
  if (lnurlw.length() > 24) {
    String preview2 = lnurlw.substring(24, 48);
    tft.drawString(preview2, x, y + 14, GFXFF);
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("See Serial for full LNURLW", x, y + 45, GFXFF);
}

#endif // ENABLE_DISPLAY

