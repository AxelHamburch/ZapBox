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

// Vending machine light barrier (T-Display-S3 only)
#if ENABLE_DISPLAY
  #define PIN_LIGHT_BARRIER 2  // NPN light barrier input (INPUT_PULLUP, active LOW)
#else
  // Vending machine sensor inputs (ESP32 Dev headless only)
  #define PIN_SENSOR_1 22  // Sensor input 1 (INPUT_PULLUP, active LOW)
  #define PIN_SENSOR_2 23  // Sensor input 2 (INPUT_PULLUP, active LOW)
#endif

// I2C Bus (shared: Touch + NFC)
#define PIN_IIC_SCL 17         // I2C clock
#define PIN_IIC_SDA 18         // I2C data

// GPIO 3 / GPIO 34 — free expansion / sensor input / FD (Field Detection) from NT3H2111
// T-Display-S3: GPIO 3 (only free pin; INPUT_PULLUP supported)
// ESP32 Dev headless: GPIO 34 (input-only, no internal pull-up — external pull-up required for open-drain FD)
#if ENABLE_DISPLAY
  #define PIN_GPIO3 3
  #define PIN_GPIO3_MODE INPUT_PULLUP
#else
  #define PIN_GPIO3 34
  #define PIN_GPIO3_MODE INPUT   // GPIO 34-39 on classic ESP32: input-only, no internal pull-up!
#endif

// Touch controller (CST816S/CST328)
#define PIN_TOUCH_INT 16       // Touch interrupt
#if ENABLE_DISPLAY
  #define PIN_TOUCH_RES 21     // Touch reset (T-Display-S3)
#else
  #define PIN_TOUCH_RES -1     // Touch reset disabled for ESP32 Dev (GPIO 21 used for LED)
#endif

// NFC Reader (PN532) - Optional
// GPIO 1 is safe on ESP32-S3 (T-Display-S3), but equals UART0 TX on classic ESP32 Dev.
#if ENABLE_DISPLAY
  #define PIN_NFC_IRQ 1   // T-Display-S3 (ESP32-S3): GPIO 1 is free (no UART conflict)
#else
  #define PIN_NFC_IRQ 4   // ESP32 Dev (classic ESP32): GPIO 1 = UART0 TX → use GPIO 4
#endif

// ─────────────────────────────────────────────────────────────
// Relay channels (Multi-Channel-Control)
//
// CH01 and CH02 are identical on both boards:
//   CH01 → GPIO 12  (default / single-mode)
//   CH02 → GPIO 13
//
// CH03 / CH04 differ per board:
//   T-Display-S3 (ESP32-S3, ENABLE_DISPLAY=1):
//     CH03 → GPIO 10  │ ESP32-S3: GPIO 10/11 are available breakout pins
//     CH04 → GPIO 11  │ GPIO 11 also used for channel-4 ambient-light sync
//   ESP32 Dev / headless (ENABLE_DISPLAY=0):
//     CH03 → GPIO 14  │ GPIO 10/11 are internal flash on ESP32-WROOM-32
//     CH04 → GPIO 16  │ and must NOT be used as output!
//
// Headless ESP32 Dev only – 8 additional channels (total: 12):
//   CH05 → GPIO 19
//   CH06 → GPIO 22
//   CH07 → GPIO 23
//   CH08 → GPIO 25
//   CH09 → GPIO 26
//   CH10 → GPIO 27
//   CH11 → GPIO 32
//   CH12 → GPIO 33
// ─────────────────────────────────────────────────────────────
#define PIN_RELAY_CH01 12
#define PIN_RELAY_CH02 13

#if ENABLE_DISPLAY
  // T-Display-S3 (ESP32-S3): GPIO 10/11 are valid breakout pins
  #define PIN_RELAY_CH03 10
  #define PIN_RELAY_CH04 11  // also ambient-light sync
#else
  // ESP32 Dev Module (classic ESP32-WROOM-32):
  // GPIO 6-11 are connected to internal SPI flash – NEVER use as output!
  #define PIN_RELAY_CH03 14
  #define PIN_RELAY_CH04 16
#endif

#if !ENABLE_DISPLAY
  // Extra relay channels available on ESP32 Dev Module (no display pins consumed)
  #define PIN_RELAY_CH05 19
  #define PIN_RELAY_CH06 22
  #define PIN_RELAY_CH07 23
  #define PIN_RELAY_CH08 25
  #define PIN_RELAY_CH09 26
  #define PIN_RELAY_CH10 27
  #define PIN_RELAY_CH11 32
  #define PIN_RELAY_CH12 33
  #define RELAY_CHANNEL_MAX 12
  // Ordered list of all relay GPIOs (indices 0-11 = CH01-CH12)
  static const int RELAY_CHANNEL_PINS[RELAY_CHANNEL_MAX] = {
    PIN_RELAY_CH01, PIN_RELAY_CH02, PIN_RELAY_CH03, PIN_RELAY_CH04,
    PIN_RELAY_CH05, PIN_RELAY_CH06, PIN_RELAY_CH07, PIN_RELAY_CH08,
    PIN_RELAY_CH09, PIN_RELAY_CH10, PIN_RELAY_CH11, PIN_RELAY_CH12
  };
#elif !defined(BOARD_JC3248W535C)
  #define RELAY_CHANNEL_MAX 4
  static const int RELAY_CHANNEL_PINS[RELAY_CHANNEL_MAX] = {
    PIN_RELAY_CH01, PIN_RELAY_CH02, PIN_RELAY_CH03, PIN_RELAY_CH04
  };
#endif

// LED Button (Optional) - Different pins for different boards
#if ENABLE_DISPLAY
  // LilyGo T-Display-S3: External LED button with switch
  #define PIN_LED_BUTTON_LED 43  // LED an externem LED-Button (3.3V treiben)
  #define PIN_LED_BUTTON_SW 44   // Tastereingang vom LED-Button (gegen GND)
#else
  // ESP32 Dev Module: Only LED (no button)
  #define PIN_LED_BUTTON_LED 21  // LED output (switch functionality not needed)
  #define PIN_ONBOARD_LED 2      // Onboard LED (additional status LED for ESP32 Dev)
  // PIN_LED_BUTTON_SW not defined for ESP32 Dev
#endif

// ─────────────────────────────────────────────────────────────
// GPIO availability summary:
//
//   T-Display-S3: ALL GPIOs are allocated. GPIO 3 is the only physically
//   unconnected pin, but it is a STRAPPING PIN — NOT suitable for sensors
//   that can be LOW at power-on (see warning below). There is no free GPIO
//   available for additional sensors on the T-Display-S3.
//
//     GPIO 15 = Power On pin (NOT free — display board power control)
//     GPIO 19/20 = USB D-/D+ (NOT usable as general I/O)
//     GPIO 34-37 = do not exist on ESP32-S3
//
//   ESP32 Dev: GPIO 4=NFC-IRQ, 12/13/14/16/19/22/23/25/26/27/32/33=Relay
//              21=LED, 2=OnboardLED
//              22/23 = Vending sensor inputs (when configured)
//              ⚠️ GPIO 6-11 = internal SPI flash on WROOM-32, NOT usable!
//
// ⚠️ GPIO 3 (T-Display-S3 ONLY) — STRAPPING PIN:
//   - Read by ROM bootloader IN HARDWARE at power-on, before any user code runs
//   - If GPIO 3 is LOW during boot → unexpected boot mode / download mode
//   - Pull-up resistors do NOT help: sensor (low impedance) overrides pull-up
//   - RC filter does NOT help: capacitor starts discharged (0V) at cold boot,
//     so GPIO 3 is already LOW during the boot phase
//   - There is NO workaround in software (delay() in setup() is useless)
//   - ONLY safe for signals guaranteed HIGH during boot (e.g. PWM output)
//   - Do NOT use GPIO 3 for sensors that can be LOW at power-on
// ─────────────────────────────────────────────────────────────

// ═══════════════════════════════════════════════════════════════
// JC3248W535C — 3.5" QSPI Touch Display (ESP32-S3-WROOM-1)
// ═══════════════════════════════════════════════════════════════
#ifdef BOARD_JC3248W535C

// ── Undefine T-Display-S3 / ESP32-Dev macros that conflict ───────
#undef PIN_LCD_BL
#undef PIN_TOUCH_RES
#undef PIN_NFC_IRQ
#undef PIN_GPIO3
#undef PIN_GPIO3_MODE
#undef PIN_RELAY_CH01
#undef PIN_RELAY_CH02
#undef PIN_RELAY_CH03
#undef PIN_RELAY_CH04
#undef RELAY_CHANNEL_MAX

// ── Display (QSPI, internal to module — NOT on breakout header) ──
// Pins defined as build flags in platformio.ini:
//   LCD_QSPI_CS=45, LCD_QSPI_CLK=47, LCD_QSPI_D0=21
//   LCD_QSPI_D1=48, LCD_QSPI_D2=40,  LCD_QSPI_D3=39
// Backlight: GPIO 1, defined as build flag LCD_BL_PIN=1
#define PIN_LCD_BL  LCD_BL_PIN   // 1 — backlight on/off

// ── External I2C Bus (PN532 NFC + NT3H2111 — broken out on header) ──
#define PIN_IIC_SCL  17
#define PIN_IIC_SDA  18

// ── Touch Controller (AXS15231B, internal to module) ─────────────
// AXS15231B touch shares the display chip; on JC3248W535C it sits on
// internal pins SDA=4 / SCL=8 (NOT on the external I2C bus above) and
// has no separate INT pin. Touch driver not yet ported.
#define PIN_TOUCH_RES  -1

// ── NFC Reader (PN532) — optional, on external I2C bus ───────────
// GPIO 16 is free on JC3248W535C (not used by display, touch, or any other peripheral).
// Note: PIN_TOUCH_INT defaults to 16 globally but is not read on this board —
// handleTouchButton() uses I2C polling (touch.isPressed()) instead of digitalRead().
// GPIO 16 must be configured as INPUT_PULLUP at boot to avoid floating → spurious reads.
#define PIN_NFC_IRQ  16

// ── Special Reserve (like GPIO 3 on T-Display-S3) ────────────────
// ⚠ GPIO 46 is a STRAPPING PIN — same warnings as GPIO 3 above apply.
// Only use for signals guaranteed HIGH during boot (e.g. PWM output, LED).
#define PIN_GPIO3       46
#define PIN_GPIO3_MODE  OUTPUT   // Safe: drive HIGH after boot

// ── Flexible Output/Input Channels ───────────────────────────────
// GPIOs 5, 6, 7, 9, 14, 15 — each configurable per channel:
//   relay | servo180 | servo360 | ambient-light |
//   sensor-stop | sensor-blockage | sensor-level
//
// Default pin assignment (overridable via Serial config / NVS):
//   CH01 → GPIO  5
//   CH02 → GPIO  6
//   CH03 → GPIO  7
//   CH04 → GPIO  9
//   CH05 → GPIO 14
//   CH06 → GPIO 15
//
// All 6 GPIOs are RTC-capable → deep-sleep wake-up supported.
// GPIO 16 is reserved for NFC IRQ — not a flex channel.
// ─────────────────────────────────────────────────────────────────
#define PIN_RELAY_CH01  5
#define PIN_RELAY_CH02  6
#define PIN_RELAY_CH03  7
#define PIN_RELAY_CH04  9
#define PIN_RELAY_CH05  14
#define PIN_RELAY_CH06  15

#define RELAY_CHANNEL_MAX 6
static const int RELAY_CHANNEL_PINS[RELAY_CHANNEL_MAX] = {
    PIN_RELAY_CH01, PIN_RELAY_CH02, PIN_RELAY_CH03,
    PIN_RELAY_CH04, PIN_RELAY_CH05, PIN_RELAY_CH06
};

// No external LED button — capacitive touch screen handles wake-up.

#endif  // BOARD_JC3248W535C