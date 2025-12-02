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

void initDisplay()
{
  tft.init();
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
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.setTextSize(10);
    tft.drawString("", x + 5, y - 85, GFXFF);
    tft.drawString("ZAP", x + 5, y - 35, GFXFF);
    tft.drawString("BOX", x + 5, y + 15, GFXFF);
    tft.drawString("", x + 5, y + 65, GFXFF);
    tft.setTextSize(2);
    tft.drawString("FW:" VERSION, x + 3, y + 115, GFXFF);
    tft.drawString("@axelhamburch", x + 1, y + 135, GFXFF);
  } else {
    tft.setTextSize(6);
    tft.drawString("ZAPBOX", x + 5, y - 15, GFXFF);
    tft.setTextSize(2);
    tft.drawString("FW:" VERSION, x, y + 40, GFXFF);
    tft.drawString("@axelhamburch", x, y + 60, GFXFF);
  }
}

// Config Mode Screen
void configModeScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("C", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SERIAL", x - 55, y + 40, GFXFF);
    tft.drawString("CONFIG", x - 55, y + 70, GFXFF);
    tft.drawString("MODE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("C", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SERIAL", x + 20, y - 30, GFXFF);
    tft.drawString("CONFIG", x + 20, y, GFXFF);
    tft.drawString("MODE", x + 20, y + 30, GFXFF);
  }
}

// WiFi Reconnect Screen
void wifiReconnectScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("W", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("WIFI", x - 55, y + 70, GFXFF);
    tft.drawString("CONNECT", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("W", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("No", x + 20, y - 30, GFXFF);
    tft.drawString("WIFI", x + 20, y, GFXFF);
    tft.drawString("CONNECT", x + 20, y + 30, GFXFF);
  }
}

// WebSocket Reconnect Screen
void websocketReconnectScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("S", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("NO", x - 55, y + 40, GFXFF);
    tft.drawString("WEBSOCK", x - 55, y + 70, GFXFF);
    tft.drawString("CONNECT", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("S", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("NO", x + 20, y - 30, GFXFF);
    tft.drawString("WEBSOCK", x + 20, y, GFXFF);
    tft.drawString("CONNECT", x + 20, y + 30, GFXFF);
  }
}

// Step one
void stepOneScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("1", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SCAN", x - 55, y + 40, GFXFF);
    tft.drawString("QR", x - 55, y + 70, GFXFF);
    tft.drawString("CODE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("1", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SCAN", x + 20, y - 30, GFXFF);
    tft.drawString("QR", x + 20, y, GFXFF);
    tft.drawString("CODE", x + 20, y + 30, GFXFF);
  }
}

// Step two
void stepTwoScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("2", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("PAY", x - 55, y + 40, GFXFF);
    tft.drawString("THE", x - 55, y + 70, GFXFF);
    tft.drawString("INVOICE", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("2", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("PAY", x + 20, y - 30, GFXFF);
    tft.drawString("THE", x + 20, y, GFXFF);
    tft.drawString("INVOICE", x + 20, y + 30, GFXFF);
  }
}

// Step three
void stepThreeScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.drawString("3", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("HAVE", x - 55, y + 40, GFXFF);
    tft.drawString("FUN &", x - 55, y + 70, GFXFF);
    tft.drawString("ENJOY", x - 55, y + 100, GFXFF);
  } else {
    tft.drawString("3", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("HAVE", x + 20, y - 30, GFXFF);
    tft.drawString("FUN", x + 20, y, GFXFF);
    tft.drawString("ENJOY", x + 20, y + 30, GFXFF);
  }
}

// Switched ON screen
void switchedOnScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK);

  if (orientation == "v"){
    tft.setTextSize(10);
    tft.drawString("Z", x + 5, y - 100, GFXFF);
    tft.drawString("A", x + 5, y - 45, GFXFF);
    tft.drawString("P", x + 5, y + 10, GFXFF);
    tft.drawString(" ", x + 5, y + 65, GFXFF);
    tft.setTextSize(3);
    tft.drawString("TIME", x + 3, y + 105, GFXFF);
  } else {
    tft.setTextSize(6);
    tft.drawString("ZAP", x + 5, y - 15, GFXFF);
    tft.setTextSize(4);
    tft.drawString("TIME", x + 3, y + 30, GFXFF);
  }
}

// Thank you
void thankYouScreen()
{
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(10);
  tft.setTextColor(TFT_BLACK);
  if (orientation == "v"){
    tft.drawString("`:)", x + 5, y - 70, GFXFF);
    tft.fillRect(15, 165, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("ENJOY", x - 55, y + 40, GFXFF);
    tft.drawString("YOUR", x - 55, y + 70, GFXFF);
    tft.drawString("DAY", x - 55, y + 100, GFXFF);
  } else {
      tft.drawString("`:)", x - 70, y, GFXFF);
    tft.fillRect(165, 15, 140, 135, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("ENJOY", x + 20, y - 30, GFXFF);
    tft.drawString("YOUR", x + 20, y, GFXFF);
    tft.drawString("DAY", x + 20, y + 30, GFXFF);
  }
}

// Show QR
void showQRScreen()
{
  tft.setTextDatum(ML_DATUM);
  tft.fillScreen(TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);

  if (orientation == "v"){
    tft.fillRect(15, 168, 140, 132, TFT_BLACK);
    tft.drawString("PAY", x - 55, y + 40, GFXFF);
    tft.drawString("FOR", x - 55, y + 70, GFXFF);
    tft.drawString("ZAP", x - 55, y + 100, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("HELP", x + 35, y + 150, GFXFF);
  } else {
    tft.fillRect(168, 18, 140, 135, TFT_BLACK);
    tft.drawString("PAY", x + 20, y - 30, GFXFF);
    tft.drawString("FOR", x + 20, y, GFXFF);
    tft.drawString("ZAP", x + 20, y + 30, GFXFF);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("HELP", x + 110, 9, GFXFF);
  }

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
          tft.fillRect(12 + 3 * x, 12 + 3 * y, 3, 3, TFT_BLACK);
      }
      else
      {
          tft.fillRect(12 + 3 * x, 12 + 3 * y, 3, 3, TFT_WHITE);
      }
    }
  }
}