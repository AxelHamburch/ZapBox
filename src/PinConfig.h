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
// ESP32 Dev headless: GPIO 34 (input-only, no internal pull-up — pull-up must be provided externally,
//   e.g. on-board via NFC Tag 2 Click, or as discrete 10 kΩ to 3.3 V on bare NT3H2111 designs)
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

#if !ENABLE_DISPLAY && !defined(BOARD_ESP32C3_21_1)
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
#elif !defined(BOARD_JC3248W535C) && !defined(BOARD_ESP32C3_21_1)
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
// GPIO 9 is free on JC3248W535C (not used by display, touch, or any other peripheral).
// Must be configured as INPUT_PULLUP at boot to avoid floating → spurious reads.
#define PIN_NFC_IRQ  9

// ── FD (Field Detection) — NT3H2111 NFC Tag 2 ────────────────────
// ⚠ GPIO 46 is a STRAPPING PIN — controls ROM serial output at boot.
// LOW at boot → ROM log suppressed (boot still proceeds normally, no bricking).
// Configured as INPUT_PULLUP: open-drain FD signal is HIGH at rest;
// a phone's NFC field pulls it LOW briefly. Identical role to GPIO 3 on T-Display-S3.
#define PIN_GPIO3       46
#define PIN_GPIO3_MODE  INPUT_PULLUP   // FD signal: HIGH = no field, LOW = phone detected

// ── Flexible Output/Input Channels ───────────────────────────────
// GPIOs 14, 15, 16, 5, 6, 7 — each configurable per channel:
//   relay | servo180 | servo360 | ambient-light |
//   sensor-stop | sensor-blockage | sensor-level
//
// Pin assignment:
//   CH01 → GPIO 14   (relay default; Special Mode applies to CH01 only)
//   CH02 → GPIO 15
//   CH03 → GPIO 16
//   CH04 → GPIO  5   ⚠ shared with the battery ADC — see below
//   CH05 → GPIO  6
//   CH06 → GPIO  7   ⚠ vending Sensor 1 input — see below
//
// GPIO 14/15/16 sit on ADC2, which the WiFi driver claims — they can never be
// analog inputs on this device, only digital / PWM. They are grouped as the
// first three channels so that CH01 (the always-on primary) lands on GPIO 14.
// The three ADC1-capable pins (5, 6, 7 = ADC1_CH4/5/6) follow as CH04..CH06;
// GPIO 5 (CH04) additionally carries the battery voltage divider.
//
// ⚠ BREAKING CHANGE (was: CH01=6, CH02=7, CH03=5, CH04=14, CH05=15, CH06=16).
//   The channel numbering was re-ordered so the primary channel (CH01) is now
//   GPIO 14. Each physical function stayed on its GPIO — only the CHxx label
//   moved. Existing devices must update the pin number in their LNbits switch
//   entry to match the new channel they use (e.g. the primary relay is now
//   pin 14, not pin 6). The wiring itself does not change.
//
// GPIO 9 is reserved for NFC IRQ — not a flex channel.
// ─────────────────────────────────────────────────────────────────
#define PIN_RELAY_CH01  14
#define PIN_RELAY_CH02  15
#define PIN_RELAY_CH03  16
#define PIN_RELAY_CH04  5
#define PIN_RELAY_CH05  6
#define PIN_RELAY_CH06  7

#define RELAY_CHANNEL_MAX 6
static const int RELAY_CHANNEL_PINS[RELAY_CHANNEL_MAX] = {
    PIN_RELAY_CH01, PIN_RELAY_CH02, PIN_RELAY_CH03,
    PIN_RELAY_CH04, PIN_RELAY_CH05, PIN_RELAY_CH06
};

// ── Battery voltage measurement (GPIO 5 = ADC1_CH4) ──────────────
// The module carries a divider from the LiPo rail to GPIO 5 (vendor schematic
// JC3248W535-1.png, and Guition's own demos define BAT_ADC_PIN 5):
//
//     BAT --[R26 33K]--+--[R25 0R]-- IO5
//                      |
//                     [R27 100K]
//                      |
//                     GND
//
// ADC1 is mandatory here: ADC2 (GPIO 14/15/16) is claimed by the WiFi driver.
//
// GPIO 5 is ALSO channel CH04. The two uses are mutually exclusive — driving the
// pin as an output overrides the (high-impedance) divider. The battery gauge is
// therefore only active while CH04 is unconfigured ("off"); see Battery.cpp.
#define PIN_BAT_ADC  5

// ── Vending machine sensor inputs ────────────────────────────────
// Same three modes as the T-Display-S3 light barrier (stop / monitor / level),
// reusing the existing two-sensor implementation (LightBarrierConfig):
//
//   Sensor 1 → CH06 (GPIO 7)
//   Sensor 2 → CH04 (GPIO 5)   ⚠ mutually exclusive with the battery gauge
//   Sensor 3 → CH05 (GPIO 6)
//
// All three ADC1-capable pins (GPIO 5/6/7 = ADC1_CH4/5/6) are sensor-capable, so
// all of CH04/CH05/CH06 can be parametrised as a sensor. Each is INPUT_PULLUP,
// active LOW — electrically any GPIO would do; these are the ADC1 pins, which
// keeps an analog sensor possible later without moving the connector.
//
// The T-Display-S3 light barrier (GPIO 2) does not exist on this board — GPIO 2
// is not broken out, so undefine it or its init would touch a floating pin.
#undef  PIN_LIGHT_BARRIER
#define PIN_SENSOR_1  PIN_RELAY_CH06   // 7
#define PIN_SENSOR_2  PIN_RELAY_CH04   // 5
#define PIN_SENSOR_3  PIN_RELAY_CH05   // 6

// ── External LED Button (same wiring as T-Display-S3) ─────────────
// PIN_LED_BUTTON_LED = 43  (inherited from ENABLE_DISPLAY=1 block above)
// PIN_LED_BUTTON_SW  = 44  (inherited from ENABLE_DISPLAY=1 block above)
// GPIO 43 (board label: TX): LED output — sources 3.3 V when device is ready
// GPIO 44 (board label: RX): Button input — INPUT_PULLUP, active LOW on press;
//           also Light-Sleep wake-up source (same as T-Display-S3)

#endif  // BOARD_JC3248W535C

// ════════════════════════════════════════════════════════════════════════════
// Board: ESP32-C3-WROOM-02 "esp32-c3-21-1"
// ESP32-C3, single core RISC-V 160 MHz, 4 MB Flash
// Native USB CDC/JTAG on GPIO19(D-)/GPIO20(D+) — no external UART chip.
// IO9 = BOOT: hold LOW during reset to enter download mode.
// ════════════════════════════════════════════════════════════════════════════
#ifdef BOARD_ESP32C3_21_1

// ── Relay output (HFD3/5 — 1A 30VDC / 0.5A 125VAC) ─────────────────────
#define PIN_RELAY    4   // IO4 → Relay coil driver

// ── Status / Ready LED ───────────────────────────────────────────────────
// GPIO5 is a status LED output (like GPIO21 on the headless esp32dev board).
// Solid ON = WiFi connected & ready  |  Slow blink = config mode
// Fast blink = error / payment received
#undef  PIN_LED_BUTTON_LED
#define PIN_LED_BUTTON_LED 5  // IO5 → status LED

// ── Flex channels (configurable as relay / servo / sensor) ───────────────
// GPIO6 and GPIO7 are secondary actuator / sensor channels.
// Each is configured independently in the web installer (modes below).
// Both channels are triggered together with GPIO4 when in actor mode.
//   relay    — secondary relay output, fires together with GPIO4
//   servo180 — 180° positional servo
//   servo360 — 360° continuous servo
//   yes      — sensor: stop relay action when triggered (INPUT_PULLUP, active LOW)
//   monitor  — sensor: block next payment until path cleared
//   level    — sensor: block payments when bin is empty (HIGH = empty)
#define PIN_FLEX_CH01  6  // IO6 → flex channel 1 (output or input)
#define PIN_FLEX_CH02  7  // IO7 → flex channel 2 (output or input)

// ── NFC IRQ (PN532 Bolt Card reader) ─────────────────────────────────────
// IO10 is free and close to the I2C header — used as PN532 IRQ/RSTPDN line.
#undef  PIN_NFC_IRQ
#define PIN_NFC_IRQ 10  // IO10 → PN532 IRQ (active LOW)

// ── Free GPIOs ───────────────────────────────────────────────────────────
// IO0, IO1, IO2, IO3, IO18 — unassigned (available for future use)

// ── BOOT button (IO9) ────────────────────────────────────────────────────
// ESP32-C3-WROOM-02: IO9 is the BOOT/strapping pin (active LOW, internal pull-up).
// Pressing the physical BOOT button pulls IO9 LOW → leftButton detects long press.
// (T-Display-S3 default PIN_BUTTON_1=0 does NOT map to the C3 BOOT button.)
#undef  PIN_BUTTON_1
#define PIN_BUTTON_1 9   // IO9 = BOOT button (active LOW)

// ── UART / I2C (via pin header: TX=GPIO21, RX=GPIO20) ──────────────────────
// GPIO20 (RXD) and GPIO21 (TXD) are exposed on the pin header.
// With ARDUINO_USB_CDC_ON_BOOT=1, Serial goes over USB — UART0 is idle,
// so GPIO20/21 are free for I2C (SDA=GPIO20, SCL=GPIO21).
#undef  PIN_IIC_SDA
#define PIN_IIC_SDA 20  // I2C data  (header pin: RXD)
#undef  PIN_IIC_SCL
#define PIN_IIC_SCL 21  // I2C clock (header pin: TXD)

#define PIN_LED_BTN  -1
#undef  PIN_ONBOARD_LED        // GPIO2 not connected to any LED on this PCB

// ── Relay channel array (single primary relay on GPIO4) ──────────────────
// GPIO6/GPIO7 flex channels are not LNbits payment channels — they always
// fire together with GPIO4 based on their configured mode (relay/servo/sensor).
#undef RELAY_CHANNEL_MAX
#define RELAY_CHANNEL_MAX 1
static const int RELAY_CHANNEL_PINS[RELAY_CHANNEL_MAX] = { PIN_RELAY };

// ── Power-on pin — NOT available on C3 ─────────────────────────────────
// GPIO 15 on ESP32-C3-WROOM-02 = SPI Flash WP (internal flash pin).
// NEVER configure GPIO 11-17 as output on C3 — doing so corrupts flash
// SPI and causes an immediate panic/reboot.
#undef  PIN_POWER_ON
#define PIN_POWER_ON -1   // No power-on pin on C3-21-1 (setup() checks >= 0)

// NOTE: I2C on this board uses GPIO20 (SDA) / GPIO21 (SCL) via pin header.
// touch.begin() is skipped in main.cpp — no touch controller on this board.
// Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL) is called explicitly in setup().

#endif  // BOARD_ESP32C3_21_1