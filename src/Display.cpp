#include <qrcode.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "display.h"

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

void setThemeColors()
{
  if (theme == "black-white") {
    themeForeground = TFT_BLACK;
    themeBackground = TFT_WHITE;
  } else if (theme == "white-black") {
    themeForeground = TFT_WHITE;
    themeBackground = TFT_BLACK;
  } else if (theme == "darkgreen-green") {
    themeForeground = TFT_DARKGREEN;
    themeBackground = TFT_GREEN;
  } else if (theme == "darkgreen-lightgrey") {
    themeForeground = TFT_DARKGREEN;
    themeBackground = TFT_LIGHTGREY;
  } else if (theme == "red-green") {
    themeForeground = TFT_RED;
    themeBackground = TFT_GREEN;
  } else if (theme == "grey-blue") {
    themeForeground = TFT_LIGHTGREY;
    themeBackground = TFT_BLUE;
  } else if (theme == "orange-brown") {
    themeForeground = TFT_ORANGE;
    themeBackground = TFT_BROWN;
  } else if (theme == "brown-yellow") {
    themeForeground = TFT_BROWN;
    themeBackground = TFT_YELLOW;
  } else if (theme == "maroon-magenta") {
    themeForeground = TFT_MAROON;
    themeBackground = TFT_MAGENTA;
  } else if (theme == "black-red") {
    themeForeground = TFT_BLACK;
    themeBackground = TFT_RED;
  } else if (theme == "brown-orange") {
    themeForeground = TFT_BROWN;
    themeBackground = TFT_ORANGE;
  } else if (theme == "black-orange") {
    themeForeground = TFT_BLACK;
    themeBackground = TFT_ORANGE;
  } else if (theme == "white-darkcyan") {
    themeForeground = TFT_WHITE;
    themeBackground = TFT_DARKCYAN;
  } else if (theme == "white-navy") {
    themeForeground = TFT_WHITE;
    themeBackground = TFT_NAVY;
  } else if (theme == "darkcyan-cyan") {
    themeForeground = TFT_DARKCYAN;
    themeBackground = TFT_CYAN;
  } else if (theme == "black-olive") {
    themeForeground = TFT_BLACK;
    themeBackground = TFT_OLIVE;
  } else if (theme == "darkgrey-lightgrey") {
    themeForeground = TFT_DARKGREY;
    themeBackground = TFT_LIGHTGREY;
  } else {
    // Default fallback
    themeForeground = TFT_BLACK;
    themeBackground = TFT_WHITE;
  }
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
void switchedOnScreen()
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

// Special Mode Screen
void specialModeScreen()
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
    tft.drawString("^_^", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("ENJOY", x - 55, y + 40, GFXFF);
    tft.drawString("YOUR", x - 55, y + 70, GFXFF);
    tft.drawString("DAY", x - 55, y + 100, GFXFF);
  } else {
      tft.drawString("^_^", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, themeForeground);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(themeBackground);
    tft.drawString("ENJOY", x + 20, y - 30, GFXFF);
    tft.drawString("YOUR", x + 20, y, GFXFF);
    tft.drawString("DAY", x + 20, y + 30, GFXFF);
  }
}

// Show QR
void showQRScreen()
{
  tft.setTextDatum(ML_DATUM);
  tft.fillScreen(themeBackground);
  tft.setTextSize(3);
  tft.setTextColor(themeBackground);

  if (orientation == "v"){
    tft.fillRect(15, 168, 140, 132, themeForeground);
    tft.drawString("READY", x - 55, y + 40, GFXFF);
    tft.drawString("4 ZAP", x - 55, y + 70, GFXFF);
    tft.drawString("ACTION", x - 55, y + 100, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 35, y + 150, GFXFF);
  } else {
    tft.fillRect(168, 18, 140, 135, themeForeground);
    tft.drawString("READY", x + 20, y - 30, GFXFF);
    tft.drawString("4 ZAP", x + 20, y, GFXFF);
    tft.drawString("ACTION", x + 20, y + 30, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 110, 9, GFXFF);
  }

  drawQRCode();
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
  tft.setTextDatum(ML_DATUM);
  tft.fillScreen(themeBackground);
  tft.setTextSize(3);
  tft.setTextColor(themeBackground);

  if (orientation == "v"){
    tft.fillRect(15, 168, 140, 132, themeForeground);
    tft.drawString("READY", x - 55, y + 40, GFXFF);
    tft.drawString("4 SP", x - 55, y + 70, GFXFF);
    tft.drawString("ACTION", x - 55, y + 100, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 35, y + 150, GFXFF);
  } else {
    tft.fillRect(168, 18, 140, 135, themeForeground);
    tft.drawString("READY", x + 20, y - 30, GFXFF);
    tft.drawString("4 SP", x + 20, y, GFXFF);
    tft.drawString("ACTION", x + 20, y + 30, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(themeForeground);
    tft.drawString("HELP", x + 110, 9, GFXFF);
  }

  drawQRCode();
}