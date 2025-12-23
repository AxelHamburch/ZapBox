#include <qrcode.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>
#include "display.h"
#include "PinConfig.h"

TFT_eSPI tft = TFT_eSPI();
#define GFXFF 1

int x;
int y;
extern char lightning[];
extern String orientation;
extern String theme;

// Theme colors - will be set based on theme selection
uint16_t themeBackground = TFT_WHITE;
uint16_t themeForeground = TFT_BLACK;

// Available TFT_eSPI standard colors:
// Basic: TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE
// Extended: TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE, TFT_PINK, TFT_GREENYELLOW
// Dark: TFT_DARKGREY, TFT_DARKGREEN, TFT_DARKCYAN, TFT_MAROON, TFT_PURPLE, TFT_OLIVE
// Light: TFT_LIGHTGREY, TFT_NAVY, TFT_BROWN
// Custom RGB565: 0xRRRR (5 bits red, 6 bits green, 5 bits blue)

// Theme configuration struct for cleaner lookup
struct ThemeConfig {
  const char* name;
  uint16_t foreground;
  uint16_t background;
};

// Theme lookup table - much cleaner than long if-else chain
const ThemeConfig themeConfigs[] = {
  {"black-white", TFT_BLACK, TFT_WHITE},
  {"white-black", TFT_WHITE, TFT_BLACK},
  {"darkgreen-green", TFT_DARKGREEN, TFT_GREEN},
  {"darkgreen-lightgrey", TFT_DARKGREEN, TFT_LIGHTGREY},
  {"red-green", TFT_RED, TFT_GREEN},
  {"grey-blue", TFT_LIGHTGREY, TFT_BLUE},
  {"orange-brown", TFT_ORANGE, TFT_BROWN},
  {"brown-yellow", TFT_BROWN, TFT_YELLOW},
  {"maroon-magenta", TFT_MAROON, TFT_MAGENTA},
  {"black-red", TFT_BLACK, TFT_RED},
  {"brown-orange", TFT_BROWN, TFT_ORANGE},
  {"black-orange", TFT_BLACK, TFT_ORANGE},
  {"white-darkcyan", TFT_WHITE, TFT_DARKCYAN},
  {"white-navy", TFT_WHITE, TFT_NAVY},
  {"darkcyan-cyan", TFT_DARKCYAN, TFT_CYAN},
  {"black-olive", TFT_BLACK, TFT_OLIVE},
  {"darkgrey-lightgrey", TFT_DARKGREY, TFT_LIGHTGREY},
  {"black-lightgrey", TFT_BLACK, TFT_LIGHTGREY}
};

void setThemeColors()
{
  // Default fallback
  themeForeground = TFT_BLACK;
  themeBackground = TFT_WHITE;
  
  // Lookup theme in table
  for (const auto& config : themeConfigs) {
    if (theme == config.name) {
      themeForeground = config.foreground;
      themeBackground = config.background;
      return;
    }
  }
  // If not found, defaults are already set above
}

void initDisplay()
{
  tft.init();
  setThemeColors(); // Set theme colors based on configuration
  
  if (orientation == "v"){
    tft.setRotation(0);
    x = 85;
    y = 160;
  } else {
    tft.setRotation(1);
    x = 160;
    y = 85;
  }
}

// Startup
void startupScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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

// Initialization Screen (shown during connection setup)
void initializationScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
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
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
    tft.drawString("1", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SCAN", x - 55, y + 40, GFXFF);
    tft.drawString("QR", x - 55, y + 70, GFXFF);
    tft.drawString("CODE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("1", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("SCAN", x + 20, y - 30, GFXFF);
    tft.drawString("QR", x + 20, y, GFXFF);
    tft.drawString("CODE", x + 20, y + 30, GFXFF);
  }
}

// Step two
void stepTwoScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
    tft.drawString("2", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("PAY", x - 55, y + 40, GFXFF);
    tft.drawString("IN-", x - 55, y + 70, GFXFF);
    tft.drawString("VOICE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("2", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("PAY", x + 20, y - 30, GFXFF);
    tft.drawString("IN-", x + 20, y, GFXFF);
    tft.drawString("VOICE", x + 20, y + 30, GFXFF);
  }
}

// Step three
void stepThreeScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
    tft.drawString("3", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("HAVE", x - 55, y + 40, GFXFF);
    tft.drawString("A NICE", x - 55, y + 70, GFXFF);
    tft.drawString("DAY", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("3", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("HAVE", x + 20, y - 30, GFXFF);
    tft.drawString("A NICE", x + 20, y, GFXFF);
    tft.drawString("DAY", x + 20, y + 30, GFXFF);
  }
}

// Switched ON screen
void actionTimeScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
    tft.setTextSize(10);
    tft.drawString("A", x + 5, y - 100, GFXFF);
    tft.drawString("C", x + 5, y - 45, GFXFF);
    tft.drawString("T", x + 5, y + 10, GFXFF);
    tft.drawString(" ", x + 5, y + 65, GFXFF);
    tft.setTextSize(3);
    tft.drawString("TIME", x + 3, y + 105, GFXFF);
  } else {
    tft.setTextSize(6);
    tft.drawString("ACTION", x + 5, y - 15, GFXFF);
    tft.setTextSize(4);
    tft.drawString("TIME", x + 3, y + 30, GFXFF);
  }
}

// Thank you
void thankYouScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(themeForeground);
  if (orientation == "v"){
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
  extern String label12;
  showProductQRScreen(label12.length() > 0 ? label12 : "READY 4 ZAP ACTION", 12);
}

void drawQRCode()
{
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 8, 0, lightning);

  for (uint8_t y = 0; y < qrcoded.size; y++)
  {
    // Each horizontal module
    for (uint8_t x = 0; x < qrcoded.size; x++)
    {
      if (qrcode_getModule(&qrcoded, x, y))
      {
          tft.fillRect(12 + 3 * x, 12 + 3 * y, 3, 3, themeForeground);
      }
      else
      {
          tft.fillRect(12 + 3 * x, 12 + 3 * y, 3, 3, themeBackground);
      }
    }
  }
}

void showThresholdQRScreen()
{
  tft.setTextDatum(ML_DATUM);
  tft.fillScreen(themeBackground);
  tft.setTextSize(3);
  tft.setTextColor(themeBackground);

  if (orientation == "v"){
    tft.fillRect(15, 168, 140, 132, themeForeground);
    tft.drawString("READY", x - 55, y + 40, GFXFF);
    tft.drawString("4 TH", x - 55, y + 70, GFXFF);
    tft.drawString("ACTION", x - 55, y + 100, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 35, y + 150, GFXFF);
  } else {
    tft.fillRect(168, 18, 140, 135, themeForeground);
    tft.drawString("READY", x + 20, y - 30, GFXFF);
    tft.drawString("4 TH", x + 20, y, GFXFF);
    tft.drawString("ACTION", x + 20, y + 30, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 110, 9, GFXFF);
  }

  drawQRCode();
}

void showSpecialModeQRScreen()
{
  extern String label12;
  showProductQRScreen(label12.length() > 0 ? label12 : "READY 4 SP ACTION", 12);
}

// Multi-Control Product QR Screen - displays label text and QR code
// Label can contain 1-3 words separated by spaces
void showProductQRScreen(String label, int pin)
{
  tft.setTextDatum(ML_DATUM);
  tft.fillScreen(themeBackground);
  tft.setTextColor(themeForeground);

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
  
  // Parse label into up to 3 words (split by space)
  String words[3] = {"", "", ""};
  int wordCount = 0;
  int lastIndex = 0;
  
  // Split by spaces
  for (int i = 0; i <= label.length() && wordCount < 3; i++) {
    if (i == label.length() || label.charAt(i) == ' ') {
      if (i > lastIndex) {
        words[wordCount] = label.substring(lastIndex, i);
        wordCount++;
      }
      lastIndex = i + 1;
    }
  }
  
  // If no words found, use pin number as fallback
  if (wordCount == 0) {
    words[0] = "Pin " + String(pin);
    wordCount = 1;
  }

  if (orientation == "v"){
    tft.fillRect(15, 168, 140, 132, themeForeground);
    
    // Display up to 3 lines of text
    tft.setTextSize(3);
    tft.setTextColor(themeBackground); // White text on black background
    int startY = y + 40; // Starting Y position
    if (wordCount == 1) {
      tft.drawString(words[0], x - 55, startY + 30, GFXFF);
    } else if (wordCount == 2) {
      tft.drawString(words[0], x - 55, startY + 15, GFXFF);
      tft.drawString(words[1], x - 55, startY + 45, GFXFF);
    } else { // 3 words
      tft.drawString(words[0], x - 55, startY, GFXFF);
      tft.drawString(words[1], x - 55, startY + 30, GFXFF);
      tft.setTextSize(2); // Smaller font for third line (currency text)
      tft.drawString(words[2], x - 55, startY + 60, GFXFF);
    }
    
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 35, y + 150, GFXFF);
  } else {
    tft.fillRect(168, 18, 140, 135, themeForeground);
    
    // Display up to 3 lines of text
    tft.setTextSize(3);
    tft.setTextColor(themeBackground); // White text on black background
    int startY = y - 30; // Starting Y position
    if (wordCount == 1) {
      tft.drawString(words[0], x + 20, startY + 30, GFXFF);
    } else if (wordCount == 2) {
      tft.drawString(words[0], x + 20, startY + 15, GFXFF);
      tft.drawString(words[1], x + 20, startY + 45, GFXFF);
    } else { // 3 words
      tft.drawString(words[0], x + 20, startY, GFXFF);
      tft.drawString(words[1], x + 20, startY + 30, GFXFF);
      tft.setTextSize(2); // Smaller font for third line (currency text)
      tft.drawString(words[2], x + 20, startY + 60, GFXFF);
    }
    
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 110, 9, GFXFF);
  }

  drawQRCode();
}

// Product Selection Screen - shown after 5 seconds of QR screen
void productSelectionScreen()
{
  tft.fillScreen(themeBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeForeground);

  if (orientation == "v"){
    // Vertical orientation
    tft.setTextSize(2);
    tft.drawString("SELECT", x, y - 40, GFXFF);
    tft.drawString("PRODUCT", x, y - 20, GFXFF);
    
    // Draw navigation arrows
    tft.setTextSize(4);
    tft.drawString("<->", x, y + 30, GFXFF);
    
  } else {
    // Horizontal orientation
    tft.setTextSize(3);
    tft.drawString("SELECT", x, y - 30, GFXFF);
    tft.drawString("PRODUCT", x, y, GFXFF);
    
    // Draw navigation arrows
    tft.setTextSize(5);
    tft.drawString("<->", x, y + 40, GFXFF);
  }
}

// Screensaver management
static bool screensaverIsActive = false;
static String screensaverMode = "off";

void activateScreensaver(String mode)
{
  Serial.println("[SCREENSAVER] Activating screensaver mode: " + mode);
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
  }
}

void deactivateScreensaver()
{
  if (!screensaverIsActive) {
    return;
  }

  Serial.println("[SCREENSAVER] Deactivating screensaver mode: " + screensaverMode);
  
  if (screensaverMode == "backlight") {
    // Turn backlight back on
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    Serial.println("[SCREENSAVER] Backlight turned on");
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
  if (mode == "light") {
    // Light sleep: CPU pauses, RAM active, faster wake-up than freeze
    // WiFi disconnects but reconnects faster than full reboot
    // NO payment processing possible during sleep (CPU is paused)
    
    // Configure BOOT button (GPIO 0) for wake-up on LOW (button pressed)
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    
    // Configure IO14 button (GPIO 14) for wake-up on LOW (button pressed)
    gpio_wakeup_enable(GPIO_NUM_14, GPIO_INTR_LOW_LEVEL);
    
    // Enable GPIO wake-up
    esp_sleep_enable_gpio_wakeup();
    
    Serial.println("[DEEP_SLEEP] Light Sleep mode configured");
    Serial.println("[DEEP_SLEEP] Wake-up sources: BOOT (GPIO 0) and IO14 (GPIO 14)");
    Serial.println("[DEEP_SLEEP] Display OFF, WiFi reconnects after wake");
    Serial.println("[DEEP_SLEEP] Power consumption: ~0.8-3mA");
    Serial.println("[DEEP_SLEEP] Wake-up time: ~1-2 seconds");
    Serial.println("[DEEP_SLEEP] NO payment processing during sleep");
    Serial.println("[DEEP_SLEEP] Press BOOT or IO14 button to wake up");
    Serial.println("[DEEP_SLEEP] Entering Light Sleep now...");
    
    esp_light_sleep_start();
    
    // Execution continues here after wake-up
    Serial.println("[WAKE_UP] Device woke from light sleep");
    
  } 
  else if (mode == "freeze") {
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