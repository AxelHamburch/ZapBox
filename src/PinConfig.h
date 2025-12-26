#pragma once

/*ESP32S3 - LilyGo T-Display-S3 Pin Configuration*/

// Display - Backlight
#define PIN_LCD_BL 38          // LCD backlight control

// Display - 8-bit parallel data bus
#define PIN_LCD_D0 39
#define PIN_LCD_D1 40
#define PIN_LCD_D2 41
#define PIN_LCD_D3 42
#define PIN_LCD_D4 45
#define PIN_LCD_D5 46
#define PIN_LCD_D6 47
#define PIN_LCD_D7 48

// Power management
#define PIN_POWER_ON 15        // Power control pin

// Display - Control signals
#define PIN_LCD_RES 5          // LCD reset
#define PIN_LCD_CS 6           // LCD chip select
#define PIN_LCD_DC 7           // LCD data/command
#define PIN_LCD_WR 8           // LCD write
#define PIN_LCD_RD 9           // LCD read

// Physical buttons & battery
#define PIN_BUTTON_1 0         // BOOT button (left)
#define PIN_BUTTON_2 14        // HELP button (right)
#define PIN_BAT_VOLT 4         // Battery voltage ADC

// I2C Bus (shared: Touch + NFC)
#define PIN_IIC_SCL 17         // I2C clock
#define PIN_IIC_SDA 18         // I2C data

// Touch controller (CST816S/CST328)
#define PIN_TOUCH_INT 16       // Touch interrupt
#define PIN_TOUCH_RES 21       // Touch reset

// NFC Reader (PN532) - Optional
#define PIN_NFC_IRQ 1          // NFC interrupt (card detection)
#define PIN_NFC_RST 2          // NFC hardware reset

// Relay channels (Multi-Channel-Control)
// PIN 12 - Relay channel 1 (default)
// PIN 13 - Relay channel 2
// PIN 10 - Relay channel 3
// PIN 11 - Relay channel 4

// LED Button (Optional)
// PIN 43 - LED from LED-button
// PIN 44 - Switch from LED-button

// Free GPIO pins (RTC-capable): 3
// Note: GPIO 1, 2, 10, 11 are assigned to NFC/Relay but can be repurposed
// Note: GPIO 12, 13 are assigned to Relay but can be repurposed
// Note: GPIO 43, 44 are assigned to LED-button but NOT RTC-capable