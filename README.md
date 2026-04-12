# Lightning - Zap⚡Box

> **Forked from [danielcharrua/satoffee](https://github.com/danielcharrua/satoffee)**  
> Expanded with many new features and hardware for many new use cases.

Bitcoin Lightning-controlled switching unit for the LilyGo T-Display-S3 and standard ESP32.

## What is the ZapBox?

The Lightning ZapBox is a compact device that controls a USB output via Bitcoin Lightning payment. Various 5V devices can be operated on the USB output, such as LED lamps, fans, or other USB-powered devices. It features multiple operation modes, customizable display themes, and advanced relay control patterns. 

Ease of use, verifiability and reliability are paramount – all in one package.

**Supported Hardware:**
- **LilyGo T-Display-S3**: Full-featured version with integrated display (Touch and Non-Touch variants)
- **ESP32 Dev Module**: Headless version for embedded applications (no display, status LED only)

Detailed descriptions, images and application examples can be found at [zapbox.space](https://zapbox.space/), [ereignishorizont.xyz](https://ereignishorizont.xyz/en/zapbox-en/) or in the [white paper](https://github.com/AxelHamburch/ZapBox/tree/main/assets/white-paper).

## Table of Contents

- [How it Works](#how-it-works)
- [Hardware](#hardware)
- [Operation](#operation)
- [Features](#features)
- [Web Installer](#web-installer)
- [PlatformIO Project](#platformio-project)
- [Compatibility](#compatibility)
- [Versioning](#versioning)
- [Acknowledgement](#acknowledgement)
- [Support](#support)

---

## How it Works

1. **QR Code Display**: The integrated display of the T-Display-S3 shows a QR code with the LNURL for scanning *(T-Display-S3 only)*
2. **Lightning Payment**: After scanning and paying the invoice, the payment is sent to the LNbits server — or tap an **NFC Bolt Card / NTAG21x tag** on the PN532 reader for contactless payment — or hold a **smartphone** near the PN532 to read the LNURLp via **NFC Card Emulation**
3. **WebSocket Trigger**: The LNbits server sends a signal via WebSocket to the ESP32 microcontroller
4. **Relay Switching**: The ESP32 activates the relay, which turns on the USB output for a specified period (with optional special modes like blinking, pulsing, or strobing)
5. **Confirmation**: The display shows that the payment has been received and the relay has been switched *(T-Display-S3 only)*

### LNURL Generation Flow

```
Device String (switchStr)
  ├── lnbitsServer  (host)
  └── deviceId      (last 22 chars)
          │
          ▼
  GET https://{server}/{ext}/api/v1/public/{deviceId}
          │
          ▼
  Server responds: { title, switches[{pin, label, amount, duration}] }
          │
          ▼
  For each pin:
  https://{server}/{ext}/api/v1/lnurl/{deviceId}?pin={pin}
          │
     ┌────┴────┐
     │         │
  LUD17:    Bech32:
  lnurlp://...  LNURL1DP68GURN...
     │         │
     └────┬────┘
          ▼
  lightning:{lnurl}  →  QR code on display
```

## Hardware

### LilyGo T-Display-S3 (Full Version)

- **Microcontroller**: ESP32-S3 with integrated 170x320 LCD display
  - Available in two versions: **Touch** (with CST816S touch controller) and **Non-Touch**
  - Software automatically detects touch capability at startup
- **Display**: 170x320 pixel ST7789 TFT (8-bit parallel interface)
- **Memory**: 16MB Flash, 8MB PSRAM
- **Relay Module**: Switches the USB output
- **USB Output Socket**: Provides 5V for connected devices
- **Two Physical Buttons**: For navigation and access to features
- **Touch Display** (Touch version): Virtual touch button for Help/Report/Config modes
- **3-Position Switch**:
  - **Position 0**: Everything off
  - **Position 1**: Output permanently on (bypass mode)
  - **Position A**: Automatic mode - ESP32 active, waiting for Lightning payment
- **PN532 NFC Reader** (Optional): For contactless NFC card/tag payment
  - Connected via I2C (shared bus with Touch controller)
  - Supports **Bolt Cards** (NTAG424 DNA) and **NTAG21x (213/215/216) / LNURL tags**
  - Tap-to-pay with automatic card removal detection and payment timeout
  - Note: The LNbits [ZapBox extension](https://github.com/AxelHamburch/zapbox_extension) is required for the NFC function.

### ESP32 Dev Module (Headless Version)

- **Microcontroller**: ESP32 (classic) without display
- **Memory**: 4MB Flash, 512KB SRAM
- **Operation**: Fully functional headless mode - all core features work via serial configuration
- **Status LED**: GPIO 21 with distinct blink patterns (3 fast boot blinks, fast blink during startup, slow blink in config mode, solid when ready, 200ms/800ms blink during NFC payment pending, 2× fast confirmation blinks on payment success, 3× fast blinks on NFC timeout/error, error blink patterns 1-4 for network issues)
- **Use Cases**: Embedded installations, wall-mounted relay control, hidden installations
- **Configuration**: Serial terminal for WiFi, LNbits, and device settings
- **Advantages**: Lower cost, smaller footprint, lower power consumption

### Pin Configuration

#### T-Display-S3 GPIO Mapping (ENABLE_DISPLAY=1)

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 0 | BOOT Button | Input | Pull-up | Left physical button - Config mode trigger (5s hold) |
| 14 | HELP Button | Input | Pull-up | Right physical button - Help/Report mode |
| 2 | Light Barrier | Input | Pull-up | NPN vending machine light barrier (active LOW) |
| 4 | Battery Voltage | ADC Input | - | Battery voltage monitoring |
| **Display Control** |
| 38 | LCD Backlight | Output | HIGH=ON | Display brightness (PWM capable) |
| 5 | LCD RES | Output | - | Display reset signal |
| 6 | LCD CS | Output | - | Display chip select |
| 7 | LCD DC | Output | - | Display data/command signal |
| 8 | LCD WR | Output | - | Display write signal |
| 9 | LCD RD | Output | - | Display read signal |
| 39-42, 45-48 | LCD Data (D0-D7) | Output | - | 8-bit parallel display data bus |
| **Touch & NFC** |
| 17 | I2C SCL | I2C | - | Shared: Touch + NFC |
| 18 | I2C SDA | I2C | - | Shared: Touch + NFC |
| 16 | Touch INT | Input | - | Touch controller interrupt |
| 21 | Touch RES | Output | - | Touch controller reset |
| 1 | NFC IRQ | Input | - | PN532 interrupt (card detection) |
| **Power & External Controls** |
| 15 | Power On | Output | - | Power control pin |
| 43 | LED Button (LED) | Output | HIGH=ON | External illuminated button LED (3.3V) |
| 44 | LED Button (SW) | Input | Pull-up | External button switch (active LOW) |
| **Relay Channels (Multi-Channel-Control)** |
| 12 | Relay 1 | Output | - | Single mode default, Duo/Quattro/Servo mode 1 |
| 13 | Relay 2 / Servo 1 | Output | - | Duo/Quattro mode 2, Servo mode PWM output |
| 10 | Relay 3 / Servo 2 | Output | - | Quattro mode 3, Servo mode PWM output |
| 11 | Relay 4 / Ambient | Output | - | Quattro mode 4, Servo mode relay 2, or ambient lighting (synced with backlight) |

**I2C Bus Addresses:**
- Touch CST816S/CST328: `0x15` or `0x5A`
- PN532 NFC Reader: `0x24`

#### ESP32 Dev Module GPIO Mapping (ENABLE_DISPLAY=0 - Headless)

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 0 | BOOT Button | Input | Pull-up | Wake from sleep / Config mode |
| 14 | HELP Button | Input | Pull-up | Help/Report mode |
| 2 | Onboard LED | Output | HIGH=ON | Additional status LED (not used as sensor on headless) |
| **Vending Sensors / Relay Output (Optional)** |
| 22 | Sensor 1 / Relay Out | Input or Output | Pull-up / HIGH | Vending sensor input or relay output synced with Pin 12 (when configured, replaces CH06) |
| 23 | Sensor 2 / Relay Out | Input or Output | Pull-up / HIGH | Vending sensor input or relay output synced with Pin 12 (when configured, replaces CH07) |
| **LEDs & Status** |
| 21 | Status LED | Output | HIGH=ON | Status indication (RTC-capable) |
| **I2C (Optional)** |
| 17 | I2C SCL | I2C | - | Optional: NFC reader |
| 18 | I2C SDA | I2C | - | Optional: NFC reader |
| 4 | NFC IRQ | Input | - | Optional: PN532 interrupt (GPIO 1 = UART0 TX on classic ESP32) |
| **Power & Control** |
| 15 | Power On | Output | - | Power control pin |
| **Relay Channels (Multi-Channel-Control)** |
| 12 | Relay 1 (CH01) | Output | - | Single/Duo/Quattro mode 1 |
| 13 | Relay 2 (CH02) | Output | - | Duo/Quattro mode 2 |
| 14 | Relay 3 (CH03) | Output | - | Quattro mode 3 (⚠️ NOT GPIO 10/11 – internal flash!) |
| 16 | Relay 4 (CH04) | Output | - | Quattro mode 4 |
| 19 | Relay 5 (CH05) | Output | - | Headless extended channel |
| 22 | Relay 6 (CH06) | Output | - | Headless extended channel (⚠️ reserved when Sensor 1 active) |
| 23 | Relay 7 (CH07) | Output | - | Headless extended channel (⚠️ reserved when Sensor 2 active) |
| 25 | Relay 8 (CH08) | Output | - | Headless extended channel |
| 26 | Relay 9 (CH09) | Output | - | Headless extended channel |
| 27 | Relay 10 (CH10) | Output | - | Headless extended channel |
| 32 | Relay 11 (CH11) | Output | - | Headless extended channel |
| 33 | Relay 12 (CH12) | Output | - | Headless extended channel |

#### Key Differences Between Variants

| Feature | T-Display-S3 | ESP32 Dev |
|---------|--------------|-----------|
| Display | LCD (170x320) | None (Headless) |
| Touch | CST816S/CST328 | N/A |
| External LED Button | Supported (GPIO 43/44) | N/A |
| Light Barrier | Supported (GPIO 2) | Dual sensors (GPIO 22/23) |
| Status Indication | Display + LED | LED only (GPIO 21 and onboard LED GPIO 2) |
| NFC Support | Yes (GPIO 1, 17, 18) | Yes (GPIO 4, 17, 18) |
| Power Consumption | ~150-250mA | Lower (no display overhead) |
| Configuration Method | Web Installer + Serial | Web Installer + Serial |
| Deep Sleep Wake | GPIO 0, 14 (not 43/44) | N/A |

### Vending Machine Light Barrier (Optional)

**Feature:** Optical item detection for vending machines via infrared light barrier.

**Two operating modes:**

- **Stop the advance** (`yes`): The conveying cycle is terminated early as soon as the product falls through the light barrier. This stops the mechanism as soon as the item is detected.

- **Monitoring Product Blockage** (`monitor`): After each payment the device checks whether the product exit path is clear. If the light barrier is still active (product blocked), the display shows a warning screen ("PRODUCT BLOCKED – Remove the product") and all further payments are locked — NFC taps and QR payments are refused — until the path is physically cleared. Once the sensor goes inactive again, payment is re-enabled automatically.

**Hardware:** NPN phototransistor light barrier module (3-wire, active LOW)
- **Pin Assignment**: GPIO 2 (T-Display-S3 only)
- **Input Type**: Digital input with internal pull-up
- **Active State**: LOW (barrier broken / item detected)
- **Inactive State**: HIGH (barrier intact / no item)

**Wiring:**
```
Light Barrier Module    →    T-Display-S3    →    GND
────────────────────────────────────────────────────
+5V / +3.3V            →    Power supply
GND                    →    GND
Signal (NPN output)    →    GPIO 2
```

### Vending Machine Sensors — Headless (Optional)

**Feature:** Two independent sensor inputs for headless vending machine operation on GPIO 22 and GPIO 23.

**Three operating modes per sensor:**

- **Stop the advance** (`yes`): Stops the relay action when the sensor detects a product (LOW signal). Minimum 2-second action time before the sensor can trigger.

- **Monitoring product blockage** (`monitor`): Continuously monitors whether the product exit is blocked (sensor LOW = blocked). Blocks further payments until cleared.

- **Level monitoring** (`level`): Continuously monitors the supply bin fill level. When the sensor goes HIGH (no product), the bin is considered empty and payments are blocked until restocked.

- **Relay output** (`relay`): Configures the GPIO as an additional relay output that switches in parallel with Pin 12 (CH01). Works with all ZapBox Modes (Relay, 180° Servo, 360° Servo) and Special Modes. Useful for driving additional relays, indicator lights, or secondary actuators synced with the main output.

**Payment blocking behavior:** When a sensor condition blocks payments, the LED blinks very fast (10 Hz, 50 ms ON/OFF), the WebSocket connection is disconnected (LNbits server rejects static QR payments), and NFC Bolt Card taps are blocked. Once cleared, the WebSocket auto-reconnects and normal operation resumes.

**Pin Assignment**: GPIO 22 (Sensor 1) and GPIO 23 (Sensor 2)
- ⚠️ When a sensor or relay output function is active on GPIO 22 or 23, those GPIOs are no longer available as relay channels (CH06/CH07).

### Relay Control Pins

**T-Display-S3 (Multi-Channel-Control with display):**
- **Single Mode (default)**: Pin 12 only
- **Duo Mode**: Pins 12 and 13
- **Quattro Mode**: Pins 12, 13, 10, and 11
  - **Special Option**: Pin 11 can be configured as ambient lighting switch (syncs with display backlight)
- **Servo Mode**: 2 relay channels (Pins 12 and 11) + 2 servo PWM outputs (Pins 13 and 10)
  - Pin 12 → Relay 1 (controls power to servo 1 circuit / primary QR trigger)
  - Pin 13 → Servo 1 PWM signal (180° servo)
  - Pin 10 → Servo 2 PWM signal (360° servo)
  - Pin 11 → Relay 2 (controls power to servo 2 circuit / ambient)
  - **One for All (OFA)**: Single payment on Pin 12 activates all channels concurrently

**ESP32 Dev Module (Headless – up to 12 independent channels):**

> The headless version does not use Single/Duo/Quattro modes for channel selection.
> Instead, the active GPIO is determined directly by the switch configuration in LNbits.
> Simply assign the desired GPIO pin to each switch in the LNbits extension.

**ZapBox Mode (Pin 12 / CH01):**
Pin 12 can be configured to operate in one of three modes:
- **Relay (default)**: Standard digital relay output (HIGH/LOW)
- **180° Servo**: Positional servo on Pin 12 — sweeps between configurable start and end angles
- **360° Servo**: Continuous rotation servo on Pin 12 — runs at configurable speed for set duration

Servo parameters (angle start/end, speed, duration) are configured via the Web Installer.
When servo mode is active, Pin 12 is reserved for the servo and skipped in the relay channel list.

| Channel | GPIO | Note |
|---------|------|------|
| CH01 | 12 | Default / single-mode (or Servo in 180°/360° ZapBox Mode) |
| CH02 | 13 | |
| CH03 | 14 | ⚠️ GPIO 10 = internal flash on WROOM-32! |
| CH04 | 16 | ⚠️ GPIO 11 = internal flash on WROOM-32! |
| CH05 | 19 | |
| CH06 | 22 | ⚠️ reserved when Sensor 1 or Relay Output active |
| CH07 | 23 | ⚠️ reserved when Sensor 2 or Relay Output active |
| CH08 | 25 | |
| CH09 | 26 | |
| CH10 | 27 | |
| CH11 | 32 | RTC-capable |
| CH12 | 33 | RTC-capable |

**Output Type:** Digital GPIO outputs (HIGH = relay activated)
**Max Current per Pin:** ~40mA (requires external relay driver for high-power loads)
**Typical Usage:** Relay driver IC (ULN2003/ULN2803) or MOSFET for external circuits

### NFC Reader Setup (Optional)

**Hardware:** PN532 NFC Module (HW-147, I2C mode)

**Wiring:**
```
PN532 HW-147    →    T-Display-S3
────────────────────────────────
VCC (3.3V)      →    3.3V
GND             →    GND
SDA             →    GPIO 18 (shared with Touch)
SCL             →    GPIO 17 (shared with Touch)
IRQ             →    GPIO 1
```

```
PN532 HW-147    →    ESP32 Dev Module
────────────────────────────────────
VCC (3.3V)      →    3.3V
GND             →    GND
SDA             →    GPIO 18
SCL             →    GPIO 17
IRQ             →    GPIO 4  (GPIO 1 = UART0 TX on classic ESP32 → not usable)
```

**Note:** The HW-147 module does not expose a hardware reset pin (RSTPD_N).
The PN532 chip initializes automatically on power-up.

**Features:**
- Reads ISO14443A cards (Mifare Classic, Mifare Ultralight, NTAG, etc.)
- IRQ-based card detection (low power, fast response)
- Shared I2C bus - no GPIO conflicts with Touch controller
- Automatic firmware version detection

**Software Integration:**
- Include `NFCPN532.h` in your code
- Initialize: `NFCPN532 nfc(Wire, PIN_IIC_SDA, PIN_IIC_SCL, PIN_NFC_IRQ);`
- Call `nfc.begin()` after Touch initialization
- Use `nfc.readPassiveTargetID()` to read card UIDs

### Electrical Layout

See the complete wiring diagram:

- [E-Layout-ZapBox-Compact.png](assets/electric/E-Layout-ZapBox-Compact.png)
- [E-Layout-ZapBox-Duo.png](assets/electric/E-Layout-ZapBox-Duo.png)
- [E-Layout-ZapBox-Quattro.png](assets/electric/E-Layout-ZapBox-Quattro.png)
- [E-Layout-ZapBox-Headless.png](assets/electric/E-Layout-ZapBox-Headless.png)
- [E-Layout-ZapBox-USB-Power-Hub.png](assets/electric/E-Layout-ZapBox-USB-Power-Hub.png)
- [E-Layout-ZapBox-ZapOMat.png](assets/electric/E-Layout-ZapBox-ZapOMat.png)
- [E-Layout-ZapBox-Servo.png](assets/electric/E-Layout-ZapBox-Servo.png)
- [E-Layout-ZapBox-Headless-Servo.png](assets/electric/E-Layout-ZapBox-Headless-Servo.png)

### External LED Button (Optional)

**Optional Feature:** Connect an external illuminated push button (T-Display-S3) or status LED (ESP32 Dev) for enhanced user interaction and status indication.

#### T-Display-S3 Configuration

**GPIO Assignment:**
| GPIO | Function | Direction | Configuration |
|------|----------|-----------|---------------|
| 43 | LED Control | Output | Sources 3.3V when device is ready |
| 44 | Button Input | Input | Pull-up to 3.3V, active LOW on press |

**Wiring:**
```
External LED Button    →    T-Display-S3    →    GND
─────────────────────────────────────────────────────
LED Anode (+)          →    GPIO 43         
LED Cathode (-)        →                    →    GND
Button Terminal 1      →    GPIO 44
Button Terminal 2      →                    →    GND
```

**3-Wire Connection:**
- **3.3V (GPIO 43)**: Powers the LED when device is ready
- **GND (Common)**: Shared ground for LED and button
- **Input (GPIO 44)**: Button switches GPIO 44 to GND when pressed

**Button Functions:**
- **Single Press**: Wake from screensaver / Navigate to next product or QR screen
- **Hold ≥2 seconds**: Open Help page (3 screens with instructions)
- **Triple-click** (within 2 seconds): Open Report page (error diagnostics)
- **Double-click, hold 2nd press ≥3 seconds**: Enter Config mode
- **In Config mode** (after 2s guard time): Press again to exit and restart

**Features:**
- 50ms hardware debounce for reliable operation
- Works in all operation modes (Single, Duo, Quattro)
- Compatible with screensaver wake-up (not with deep sleep)
- Reuses existing navigation/report/help functions from physical buttons

**Note:** GPIOs 43 and 44 are not RTC-capable and cannot be used for deep sleep wake-up.

**Wiring:**
```
Status LED             →    ESP32 Dev       →    GND
────────────────────────────────────────────────────
LED Anode (+)          →    GPIO 21         
LED Cathode (-)        →    Resistor (220Ω) →    GND
```

**Features:**
- Status LED only (no button functionality on ESP32 Dev)
- GPIO 21 is RTC-capable and can be used for deep sleep wake-up
- Distinct LED patterns for clear status and error indication

**LED Behavior (ESP32 Dev Headless):**
- **3x Very Fast Blink on Boot**: Three quick flashes immediately after power-on to indicate device start
- **Fast Blink (5Hz, 200ms)**: During startup and initialization (INITIALIZING, CONNECTING_WIFI states)
- **Slow Blink (1Hz, 1000ms)**: Config mode active - device waiting for configuration
- **Solid ON**: Device is ready to receive payments
- **200ms ON / 800ms OFF blink**: NFC payment pending – waiting for invoice settlement via WebSocket
- **2× Fast Blink (100ms ON/OFF) + Solid ON**: NFC payment confirmed – 2 quick confirmation flashes, then LED stays ON while relay fires
- **3× Fast Blink (100ms ON/OFF) + Solid ON**: NFC timeout (60s) or HTTP error – "NO LUCK" visual feedback, then returns to ready state
- **Very Fast Blink (10Hz, 50ms ON/OFF)**: Vending sensor blocking – one or more sensors are triggering a payment block (bin empty, product blocked, or stop active). WebSocket is disconnected.
- **Error Blink Patterns** (with 2 second pause between sequences):
  - **1 Blink** (500ms on, 500ms off): NO WIFI - WiFi connection lost or not established
  - **2 Blinks** (300ms on/off each): NO INTERNET - WiFi connected but no internet access
  - **3 Blinks** (250ms on/off each): NO SERVER - Internet OK but LNbits server unreachable
  - **4 Blinks** (200ms on/off each): NO WEBSOCKET - Server OK but WebSocket connection failed
- **OFF**: Help/Report modes, or deep sleep

**LED Behavior (T-Display-S3 with Display):**
- **ON**: Device is ready to receive payments (not in initialization, error, Config/Help/Report modes, or deep sleep)
- **OFF**: During startup, initialization, error states, Config/Help/Report modes, or deep sleep

### Headless Version: LED Status Diagnostics

The headless version (ESP32 Dev) uses the status LED to provide comprehensive visual feedback without a display. This allows for quick system diagnostics by counting LED blinks.

#### Network Error Detection Priority

Error patterns are displayed in priority order - the LED shows the **first unconfirmed network status**:
1. **WiFi** → 2. **Internet** → 3. **Server** → 4. **WebSocket**

For example: If WiFi is disconnected, the LED will show 1 blink (WiFi error) even if other services are also unavailable. Once WiFi is restored, the LED will show the next error in the chain (if any).

#### LED Pattern Reference Table

| Pattern | Timing | Status | Description |
|---------|--------|--------|-------------|
| **3 Fast Blinks** | 3x rapid flash on boot | **BOOT** | Device powered on, starting initialization |
| **Fast Continuous** | 200ms on/off (5Hz) | **INITIALIZING** | System startup, WiFi connecting |
| **Slow Continuous** | 1000ms on/off (1Hz) | **CONFIG MODE** | Configuration interface active, waiting for settings |
| **Solid ON** | Continuous light | **READY** | All systems operational, ready for payments |
| **Asymmetric Blink** | 200ms ON / 800ms OFF | **NFC PENDING** | NFC payment initiated, waiting for invoice settlement |
| **2× Fast Blink** | 100ms ON/OFF × 2, then solid | **NFC SUCCESS** | Payment confirmed – 2 quick flashes, relay fires, LED stays ON |
| **3× Fast Blink** | 100ms ON/OFF × 3, then solid | **NFC NO LUCK** | 60s timeout or HTTP error – returns to ready state |
| **Very Fast Continuous** | 50ms on/off (10Hz) | **SENSOR BLOCKING** | Vending sensor active – payments blocked, WebSocket disconnected |
| **1 Blink + Pause** | 500ms on/off, 2s pause | **NO WIFI** | WiFi connection lost or failed to connect |
| **2 Blinks + Pause** | 300ms on/off/on/off, 2s pause | **NO INTERNET** | WiFi connected, but no internet gateway access |
| **3 Blinks + Pause** | 250ms each, 2s pause | **NO SERVER** | Internet connected, LNbits server unreachable |
| **4 Blinks + Pause** | 200ms each, 2s pause | **NO WEBSOCKET** | Server reachable, WebSocket connection failed |
| **OFF** | No light | **INACTIVE** | Deep sleep, Help/Report modes, or system halted |

#### Troubleshooting with LED Patterns

**1 Blink (NO WIFI)**
- Check WiFi credentials in configuration
- Verify WiFi router is powered on and in range
- Check for MAC address filtering on router
- Confirm correct WiFi password

**2 Blinks (NO INTERNET)**
- WiFi credentials are correct, but router has no internet
- Check router's internet connection (modem/ISP)
- Verify router's WAN port is connected
- May occur during ISP outages

**3 Blinks (NO SERVER)**
- Internet is working, but LNbits server is down or unreachable
- Check LNbits server URL in configuration
- Verify LNbits server is running
- Check firewall rules (port 443/HTTPS)
- Confirm DNS resolution of server hostname

**4 Blinks (NO WEBSOCKET)**
- Server is reachable, but WebSocket connection or data validation failed
- **Most common cause:** Bitcoinswitch instance was deleted on LNbits server (HTTP 404)
  - Check if the configured device ID still exists in LNbits
  - Verify the switch configuration in LNbits admin panel
  - If instance was deleted, create a new one and update device configuration
- May occur briefly during LNbits service restart
- Check LNbits WebSocket configuration
- Verify API key/credentials are valid
- Should auto-recover when server completes startup or after creating new instance

**Recovery Behavior**
After network issues are resolved, the LED automatically transitions back to **Solid ON** (READY state). If the LED remains in an error state after confirmed repairs, perform a device reset by pressing the onboard reset button.


## Operation

### On-board Button - Reset
- **System reset**: Restarts the device completely

### On-board Button - HELP (GPIO 14)
- **Click once**: Open the Help page
- **Double-click**: Open the Report page

### On-board Button - NEXT (BOOT / GPIO 0)
- **General**: Wakes up from screensaver and deep sleep
- **1x click**: Display product page / Change page
- **Hold for more than 5 seconds**: Start Config mode
  - ⚠️ **Note**: Clicking again closes the Config page prematurely
- **Special function**: Hold down the BOOT button, press the reset button once, and then release the BOOT button → Activates reception mode for firmware updates

### Touch display (touch version only)
- **General**: Wakes up from screensaver (not compatible with deep sleep)
- **1x click/swipe**: Display product page / Change page

### Touch button (Red circle next to the touch field)
- **Click once**: Open the help page
- **Double-click**: Open the report page
- **Quadruple-click**: Open the Config page
  - ⚠️ **Note**: A delayed click on the display deactivates the Config page prematurely

### External LED-Button (if available)
- **LED Indicator**: Active when device is ready (no initialization, error, or special mode active)
- **General**: Wakes up from screensaver (not compatible with deep sleep)
- **Press once**: Display product page / Change page
- **Press and hold for at least 2 seconds**: Open the help page
- **Press 3x quickly**: Open the report page
- **Press briefly once, then hold for at least 3 seconds**: Activate Config mode
  - ⚠️ **Note**: Clicking again closes the Config page prematurely

### Startup & Initialization Sequence

The ZapBox features an optimized startup sequence with parallel connection establishment:

**Phase 1: Startup Screen (6 seconds)**
- Displays "ZAPBOX" branding with firmware version
- Shows "Powered by LNbits"
- WiFi connection starts in background during this phase

**Phase 2: Initialization Screen (up to 20 seconds)**
- Displays "ZAPBOX" with "Initialization in progress . . ."
- All connection tests run in parallel:
  1. WiFi connection (continues from Phase 1)
  2. Internet connectivity check (once WiFi connected)
  3. LNbits server reachability test (once Internet confirmed)
  4. WebSocket connection establishment (once Server confirmed)
- **Early Exit**: Screen switches to QR code as soon as all connections are successful
- **Maximum Time**: 25 seconds total (5s startup + 20s init) if connections take longer
- **Error Display**: After 25 seconds, shows first detected error if any connection failed

**Optimal Scenario**: ~10-15 seconds from power-on to QR code display
**Error Scenario**: 25 seconds → displays specific error screen

### Error Detection & Priority System

The ZapBox features a hierarchical error detection system with automatic diagnostics:

| Priority | Error Type | Abbreviation | Detection Method | Description |
|----------|-----------|--------------|------------------|-------------|
| 1 (Highest) | **NO WIFI** | NW | WiFi connection status | WiFi network not connected<br>-> Wifi data correct?<br>-> WiFi signal too weak? |
| 2 | **NO INTERNET** | NI | HTTP check to Google | Internet connectivity lost<br>-> Internet accessible? |
| 3 | **NO SERVER** | NS | TCP port 443 check | LNbits server unreachable<br>-> Server hardware down?<br>-> Device string correct?  |
| 4 (Lowest) | **NO WEBSOCKET** | NWS | WebSocket connection status | WebSocket protocol/handshake failure<br>-> LNbits down?<br>-> Device string correct?  |

**Error Detection Logic:**
- Each error level is only checked if all higher priority levels are OK
- Higher priority errors override lower priority error displays
- **WiFi down** → All other checks skipped, shows "NO WIFI"
- **WiFi OK, Internet down** → Server/WebSocket checks skipped, shows "NO INTERNET"
- **WiFi + Internet OK, Server down** → WebSocket check skipped, shows "NO SERVER"
- **WiFi + Internet + Server OK, WebSocket down** → Shows "NO WEBSOCKET"

**Monitoring Intervals:**
- **Internet Check**: Every 30 seconds via HTTP GET to `clients3.google.com/generate_204` (Google's connectivity service)
- **WiFi/Server/WebSocket Check**: Every 5 seconds with priority-based handling
- **WebSocket Ping**: Every 60 seconds (when connected) for connection quality monitoring

**Smart Recovery:**
- When WiFi reconnects, automatically checks Internet and Server status
- Prevents brief "Ready for Action" flash when higher-priority errors persist
- Automatically returns to correct error screen based on current system state

**Report Mode**: 
- **Press HELP button twice** in quick succession to display error counters (0-99) for all four error types with their occurrence counts
- **Press LED button four times** in quick succession (if external LED button is available)

**Common Wallet Error**: If a wallet scanning the QR code shows an error message "bitcoinswitch ... is disabled", this indicates either:
- The Bitcoin Switch was actively disabled in LNbits, or
- The handshake between the wallet and ZapBox failed

## Features

### Basic Configuration

Configuration is done via the [Web Installer](installer/index.html) with browser-based serial connection:

- WiFi SSID and password
- LNbits server WebSocket URL (supports both `ws://` and `wss://`)
- LNURL for payments
- Display orientation (horizontal/vertical)
- **Display Theme**: Multiple color combinations available (selectable in the web installer)

### Advanced Features

#### Multi-Channel-Control Mode (Touch Variant)
**Available on T-Display-S3 Touch variant only**

Control multiple relays with automatic product selection and label integration:

- **Single Mode** (default): Traditional single relay control on Pin 12
- **Duo Mode**: Two products on Pins 12 and 13
- **Quattro Mode**: Four products on Pins 12, 13, 10, and 11

**Features:**
- **Touch Navigation**: Swipe left/right (<-→) on product selection screen to choose product
- **Automatic LNURL Generation**: 
  - Each pin gets its own unique LNURL with Bech32 encoding
  - LUD17 format (LNURL as URL) for maximum compatibility
  - Encoded with HRP "lnurl" and XOR 1 checksum
- **Backend Product Labels**: 
  - Labels are fetched automatically from LNbits backend via `/api/v1/public/{deviceId}`
  - Labels are displayed on all QR screens (Normal, Special, and Multi-Channel-Control modes)
  - Multi-line display: Up to 3 words separated by spaces
  - Currency symbols automatically converted to text: €→EUR, $→USD, £→GBP, ¥→YEN, ₿→BTC, ₹→INR, ₽→RUB, ¢→ct
  - Third line uses smaller font for currency display
- **x-Second Timeout**: Product selection screen automatically shows after x seconds on QR screen
- **Loop Navigation**: Navigation wraps around (last→first, first→last)

**Configuration:**
Set Multi-Channel-Control Mode in Web Installer:
- `single` (default): Pin 12 only
- `duo`: Pins 12, 13
- `quattro`: Pins 12, 13, 10, 11
- `servo`: Pins 12 (relay 1), 13 (180° servo PWM), 10 (360° servo PWM), 11 (relay 2 / ambient)
- `one-for-all` *(default in Servo mode)*: Single QR on Pin 12 — all 4 channels fire concurrently on payment

**Use Cases**: Vending machines, multi-product payment terminals, flexible product offerings, servo-controlled dispensers and barriers

##### Special Function Channel 4 - Ambient Lighting Switch
**Available in Quattro Mode only (T-Display-S3)**

Channel 4 (GPIO 11) can be configured as an ambient lighting switch that synchronizes with the display backlight instead of functioning as a regular payment-controlled relay:

**Features:**
- **Backlight Synchronization**: GPIO 11 mirrors the state of the display backlight (GPIO 38)
  - HIGH when display is active and backlight is on
  - LOW when screensaver is active or device enters deep sleep
- **Reduced Product Count**: When enabled, only 3 products are shown (channels 1-3) instead of 4
- **Automatic Control**: No payment needed - GPIO 11 switches automatically based on display state
- **Use Cases**: 
  - Ambient LED strip lighting synchronized with display activity
  - Mood lighting that turns off during screensaver/sleep
  - Visual power state indicator
  - External backlight control for custom displays

**Configuration:**
Set in Web Installer under "Multi-Channel Mode - Quattro":
- **Special function Channel 4** dropdown:
  - `Off (default)`: GPIO 11 works as normal payment-controlled channel 4
  - `Ambient lighting switch`: GPIO 11 synchronized with display backlight

**Technical Details:**
- GPIO 11 is initialized after configuration is loaded
- State changes occur in:
  - `activateScreensaver()`: Sets GPIO 11 LOW
  - `deactivateScreensaver()`: Sets GPIO 11 HIGH
  - `prepareDeepSleep()`: Sets GPIO 11 LOW
- Initial state after boot: HIGH (display active by default)

**Note**: When ambient lighting mode is active, channel 4 cannot be used for payment-controlled switching. The device displays and accepts payments only for channels 1-3.

##### Servo Mode (4-Channel: 2 Relay + 2 Servo)
**Available on T-Display-S3 only**

Controls up to two servo motors via Bitcoin Lightning payment. Each servo is paired with a relay that can cut power between uses.

**Pin Assignment:**
| Pin | Function | Description |
|-----|----------|-------------|
| 12 | Relay 1 | Primary trigger — QR code shown for this channel |
| 13 | 180° Servo PWM | Sweeps Start→End, holds for action time, returns |
| 10 | 360° Servo PWM | Spins for configured duration on payment |
| 11 | Relay 2 / Ambient | Activates for action time duration |

**Activation Modes:**

**One for All (OFA) — Default**
- A single QR code (Pin 12) is shown to the customer
- On payment, all four channels activate simultaneously as concurrent FreeRTOS tasks:
  - **Pin 12** (Relay 1): fires normally per LNbits action time
  - **Pin 13** (180° Servo): sweeps to end angle, holds for the full action time, then returns to start
  - **Pin 10** (360° Servo): spins for the configured duration
  - **Pin 11** (Relay 2): activates for the full action time
- Ideal for vending machines and dispensers where one payment triggers the complete mechanism

**Independent Channels**
- Each channel has its own QR code and can be triggered separately
- Useful for multi-product setups where each item has a different price

**Servo Configuration (Web Installer):**
- **Start angle (°)**: Position the servo moves to at startup (0–180°)
- **End angle (°)**: Position the servo sweeps to on payment trigger (0–180°)
- **Sweep duration (ms)**: Time for one full sweep; `0` = native servo speed (max speed)
- **Return to start**: `Yes` — servo always returns after relay-off delay; `No` — toggle mode (alternates direction each trigger)

**Toggle Mode (Return = No):**
- First trigger: sweeps Start → End, stays at End
- Next trigger: sweeps End → Start, stays at Start
- Ideal for latches, barriers, or dispensers where the servo should stay in position

**Servo 2 optional:**
- If all Servo 2 values are zero, only Servo 1 (Pin 12/13) is active
- Pin 11 remains available as a regular relay channel (Channel 4)

**Important Notes:**
- Requires external 5V power supply for the servo (MG996R draws up to 2.5A peak)
- Special Mode (blink/pulse/strobe) is automatically bypassed in Servo mode — pulsing the relay would interfere with servo timing
- GPIO 10 and 13 are LEDC-capable on the ESP32-S3 (T-Display-S3 only); not available on headless ESP32 Dev

#### BTC-Ticker with Currency Display
**Available on all variants**

Real-time Bitcoin price and block height display with configurable visibility modes:

**Features:**
- **Bitcoin Price**: Live price in configurable currency (e.g., USD, EUR, GBP)
- **Block Height**: Current blockchain block height from mempool.space
- **Bitcoin Logo**: 64x64 pixel logo displayed on ticker screen
- **Currency Selection**: ISO code input (max. 3 characters, automatically uppercase)
- **Auto-Refresh**: Updates automatically after WiFi/Internet recovery
- **Touch Support**: Touch display to show/hide ticker in always and selecting modes
- **Three Display Modes**:
  - **OFF**: No ticker display
    - Duo/Quattro: Shows product selection screen instead
    - Single: Shows only QR code (no ticker, no product selection)
  - **ON - always**: Ticker overlay with on-demand QR display
    - Single: Starts with ticker, touch shows QR for 20s, then returns to ticker
    - Duo/Quattro: Ticker overlays, navigate with NEXT/swipe shows products temporarily, returns to ticker after 20s
  - **ON - when selecting**: Ticker appears only on demand
    - Single: Touch/NEXT shows ticker for 10s, then returns to QR
    - Duo/Quattro: Product selection → products → ticker on touch/swipe (10s timeout)

**Configuration:**
Set BTC-Ticker mode and currency in Web Installer:
- Mode: `off`, `always`, or `selecting`
- Currency: Any 3-letter ISO code (e.g., `USD`, `EUR`, `GBP`, `JPY`, `CHF`)

**Data Sources:**
- Price API: CoinGecko (supports 50+ currencies)
- Block Height: mempool.space

**Use Cases**: Bitcoin payment terminals, price information displays, educational demonstrations

#### Special Modes
Control relay switching patterns beyond simple on/off:
- **Standard**: Simple on/off (default)
- **Blink**: 1 Hz, 1:1 duty cycle
- **Pulse**: 2 Hz, 1:4 duty cycle (short pulses)
- **Strobe**: 5 Hz, 1:1 duty cycle (fast blinking)
- **Custom**: Set your own frequency (0.1-10 Hz) and duty cycle ratio (0.1-10)

**Use Cases**: LED effects, motor speed control, warning signals, custom patterns

#### Threshold Mode
Monitor a wallet balance and trigger the relay when a threshold is reached:
- Configure wallet invoice/read key
- Set threshold amount in satoshi
- Define GPIO pin and control duration
- Use static LNURL or Lightning Address for payments
- Payments accumulate in the wallet until threshold is reached

**Use Cases**: Crowdfunding triggers, donation goals, pay-per-use with accumulated balance

---

#### NFC Payment (Bolt Card / NTAG21x / LNURL Tags)

> **Optional feature** — activated via build flag `ENABLE_NFC=1` (default: enabled on T-Display-S3).

The ZapBox supports tap-to-pay via NFC with two card types:
- **Bolt Cards** (NTAG424 DNA): Authenticated LNURLW read via SUN message
- **NTAG21x (213/215/216) / LNURL Tags**: Plain NDEF text record containing an `lnurlw://` URL

A customer simply holds their card or tag near the PN532 reader to trigger a payment.

**Hardware Requirements**:
- PN532 NFC module wired on the shared I2C bus (SDA=GPIO18, SCL=GPIO17)
- IRQ line connected to GPIO1 (`PIN_NFC_IRQ`)
- Pull-up resistor on IRQ (internal `INPUT_PULLUP` used)

**Payment Flow**:
```
[Bolt Card / NTAG21x]  →  tap on PN532 reader
    ↓
PN532 detects ISO14443A card (IRQ LOW on GPIO1)
    ↓
FreeRTOS Task (Core 0) reads card:
  • NTAG424 DNA: Authenticated file read (SUN message)
  • NTAG21x (213/215/216): NDEF text record parse
    ↓
ZapBox validates "lnurlw://" prefix in returned data
    ↓
Display shows "PENDING NFC" screen
    ↓
ZapBox sends WebSocket event to zapbox_extension:
  { "event": "lnurlw", "lnurlw": "lnurlw://...", "pin": <activePin> }
    ↓
zapbox_extension resolves LNURLW → Lightning invoice → payment detected
    ↓
zapbox_extension sends back WS event → ZapBox activates relay / channel
    ↓
Display shows "ACTION TIME" → "THANK YOU" → returns to QR screen
```

**NFC Timeout & NO LUCK**:
- After a card tap, the device enters a **pending** state while waiting for payment confirmation
  - **T-Display-S3**: Shows a **"PENDING NFC"** screen
  - **Headless (ESP32 Dev)**: LED blinks **200ms ON / 800ms OFF**
- If no payment is confirmed within **60 seconds**, the device signals **"NO LUCK"**:
  - **T-Display-S3**: Switches to a **"NO LUCK"** screen (shown for 5 seconds, then returns to QR screen)
  - **Headless**: LED does **3× fast blinks** (100ms ON/OFF), then returns to solid ON (ready)
- This prevents the device from being stuck in a pending state indefinitely
- HTTP errors during the LNURLW request do **not** immediately trigger NO LUCK — the server may still confirm the payment via WebSocket within the timeout period
- On **successful payment**, the headless version shows **2× fast confirmation blinks** (100ms ON/OFF) before the relay fires

**Card Removal Detection**:
- After a successful card read, the NFC task waits for the card to be physically removed
- Detection requires **2 consecutive absent polls** (~0.8 seconds) to prevent false triggers
- This prevents **double-trigger** issues where a single tap would fire two payment requests

**Implementation Notes**:
- Uses [Adafruit-PN532-NTAG424](https://github.com/bitcoin-ring/Adafruit-PN532-NTAG424) library
- FreeRTOS task runs on Core 0 with priority 1 (stack 8 KB)
- IRQ-based detection — no I²C polling, does not interfere with touch sensor on shared bus
- The active channel (`pin`) is derived from `multiChannelConfig.currentProduct`
- Cross-core communication via `volatile` flags in GlobalState.h (Core 0 NFC task ↔ Core 1 main loop)

**Activate in `platformio.ini`**:
```ini
build_flags =
  ...
  -DENABLE_NFC=1
  -DENABLE_NFC_TEST=0   ; set to 1 for hardware test (no server needed)
```

---

#### NFC Card Emulation (LNURLp via NFC)

> **Optional feature** — activated via build flag `ENABLE_NFC=1`. Requires a PN532 NFC module.

The ZapBox can act as an **NFC tag** so that smartphones can read the LNURLp payment link by simply tapping their phone on the PN532 module — no QR code scanning needed.

The PN532 switches from Reader Mode (reading Bolt Cards) to **Target Mode** (emulating an ISO 14443-4 Type 4 Tag). The phone's NFC reader detects the ZapBox as a standard NDEF tag containing a `lightning:LNURL...` URI.

**How it Works**:
```
Smartphone (Initiator)          ZapBox + PN532 (Target)
─────────────────────           ──────────────────────────
NFC field ON          ────►     PN532 Target Mode (passive, no field)
                                IRQ fires → phone detected
ISO 14443-4 activation ◄──►     RATS/ATS exchange (handled by PN532)
SELECT NDEF App       ────►     Response: OK (9000)
SELECT CC file        ────►     Response: Capability Container
READ CC               ────►     Response: 15 bytes (NDEF file mapping)
SELECT NDEF file      ────►     Response: OK (9000)
READ BINARY (chunks)  ────►     Response: NDEF URI record
                                  "lightning:LNURL1DP68GURN..."
Phone opens wallet    ◄────     Payment flow starts automatically
```

**Key Features**:
- **Zero interaction required** — customer just taps their phone
- **Automatic NDEF update** — the LNURL payload updates whenever the QR code changes (product selection, channel switch)
- **ISO 14443-4 / ISO-DEP** compliant — works with all modern Android and iOS NFC readers
- **Coexists with Reader Mode** — NFC mode is configurable per device (reader, emulation, or both)
- **No additional hardware** — uses the same PN532 module and wiring as Bolt Card reading
- **Raw I2C communication** — bypasses the Adafruit library for precise timing control required by ISO-DEP frame deadlines

**NDEF Payload**:
```
NDEF URI Record:
  TNF:     0x01 (Well-known)
  Type:    "U" (URI)
  Prefix:  0x00 (no prefix)
  Payload: "lightning:LNURL1DP68GURN8GHJ7V33..."
```

**Supported Phones**:
- Android: Any phone with NFC (Phoenix, Wallet of Satoshi, Zeus, etc.)
- iOS: iPhone 7 and later (NFC tag reading via NDEF)

**NFC Mode Configuration** (Web Installer):
- `reader` — Bolt Card / NTAG21x reading only (default)
- `emulation` — Card Emulation only (LNURLp via NFC)
- `both` — Alternates between Reader and Emulation modes

**Implementation Notes**:
- Uses PN532 `TgInitAsTarget` command with SAK=0x20 (ISO-DEP only)
- IRQ-based phone detection — no I2C polling during idle wait
- APDU exchange handles the full NFC Forum Type 4 Tag command set (SELECT, READ BINARY)
- FreeRTOS task on Core 0, priority 1, 8192 byte stack
- NDEF payload supports URIs up to ~900 bytes (sufficient for all LNURL encodings)

---

**Hardware Test Mode (`ENABLE_NFC_TEST=1`)**:

To verify the hardware without a running bitcoinswitch_extension server:

1. Set both flags: `-DENABLE_NFC=1` and `-DENABLE_NFC_TEST=1`
2. Flash and open Serial Monitor (115200 baud)
3. Hold a Bolt Card near the PN532 reader
4. The display shows **"NFC OK!"** and a preview of the LNURLW
5. The full LNURLW string is printed to Serial — no WebSocket connection required

This allows testing the complete NFC read path (PN532 init → NTAG424 read → LNURLW decode) independently of the payment backend.

---

#### Screensaver & Deep Sleep Function
Automatic power-saving modes that activate after a configurable timeout:

**Power Consumption Comparison**:

| Mode | Backlight | Display | CPU | RAM | WiFi | WebSocket | Payments | Power | Savings | Wake-up |
|------|-----------|---------|-----|-----|------|-----------|----------|-------|---------|---------|
| **Normal** | ON | Active | Running | Active | Active | Active | ✅ Yes | ~150-250mA | 0% | - |
| **Screensaver** | OFF | Active | Running | Active | Active | Active | ✅ Yes | ~40-60mA | ~80-90% | Instant |
| **Light Sleep** | OFF | Active | Paused | Retained | Reconnect | Reconnect | ❌ No* | ~0.8-2mA | ~99% | ~3-5s |
| **Deep Sleep (Freeze)** | OFF | Active | OFF | RTC only | Reconnect | Reconnect | ❌ No* | ~0.01-0.15mA | ~99.9% | ~3-5s |

*Deep Sleep / Light Sleep requires wake-up (button press) and WiFi reconnection before payments can be received

**Wake-up Methods**:
- **Screensaver**: Touch display (Touch version) or press any button → Instant wake-up
- **Light Sleep**: Press BOOT, HELP or LED button → Device restarts and reconnects (~3-5s)
- **Deep Sleep (Freeze)**: Press BOOT or HELP button only (touch and LED button disabled for maximum power savings)

**Mode Recommendations**:

- **Screensaver (backlight off)**: ⭐ **Best for payment terminals** - Instant wake-up, 80-90% power saving, payments always work
  - Use when device should respond immediately to payments
  - Good for public terminals with frequent use
  - Battery operation: ~7-10 days with 10000mAh battery
  
- **Light Sleep**: ⭐ **Best for devices with external LED button** - 99% power saving, all buttons can wake the device
  - LED button (GPIO 44) can wake the device — not possible with freeze mode
  - WiFi reconnects after wake-up (~3-5 seconds)
  - NO payments received during sleep
  - Battery operation: months with 10000mAh battery

- **Deep Sleep (freeze)**: ⭐ **Best for long-term installations** - 99.9% power saving, maximum battery life
  - WiFi reconnects after wake-up (~3-5 seconds)
  - NO payments received during sleep
  - Press button to wake and device will restart
  - ⚠️ LED button (GPIO 44) **cannot** wake from freeze — GPIO 44 is not RTC-capable
  - Battery operation: 7.5-114 years(!) with 10000mAh battery
  - Ideal for devices used rarely or for maximum energy savings

**Configuration**:

- **Screensaver Options**:
  - **OFF**: No power saving (default)
  - **Backlight Off**: Display backlight turns off - recommended option

- **Deep Sleep Options**:
  - **OFF**: No deep sleep (default)
  - **Freeze**: Deep sleep mode - Maximum power saving, NO payments during sleep
  - **Light Sleep**: Light sleep mode - All buttons can wake, LED button supported

- **Combined Mode**: Screensaver and Deep Sleep can be used together — screensaver activates first (backlight off, payments still work), then deep sleep kicks in later for maximum power saving
- **Activation Timers**: Separate configurable timeouts for screensaver (default 5 min) and deep sleep (default 30 min), range 1-120 minutes each
- **Wake-up**: Press BOOT button or IO14 button to wake from sleep (Light Sleep also supports LED button)
- **Payment Processing**: Only Screensaver mode can receive payments during power saving

**Technical Notes**:
- **Screensaver - Backlight Power**: The T-Display-S3's backlight consumes most display power (~150-200mA). Turning it off saves 80-90% while keeping the display controller active for instant wake-up. CPU continues running, payments work normally.
- **Light Sleep - CPU Paused**: CPU is paused but RAM is retained. Any GPIO can wake the device, including GPIO 44 (LED button). Device restarts after wake-up for clean reinitialization.
- **Deep Sleep - Complete Shutdown**: Only RTC memory active, device performs full restart on wake-up (~3-5s), requires complete WiFi reconnection. NO payment processing during sleep. Maximum battery life. Only RTC-capable GPIOs (0, 14) can wake the device.

**Use Cases**: Energy saving for installations, battery operation, reducing device heat, extending display lifespan in always-on scenarios

## Web Installer

The ZapBox includes a browser-based Web Installer for easy firmware updates and configuration:

1. **Flash Firmware**: Select version and flash via USB (Chrome/Edge required)
2. **Configure Device**: Set WiFi, LNbits connection, display settings, and advanced features
3. **Serial Console**: Debug and monitor device in real-time
4. **Automatic Config Mode Detection**: Press BOOT button for 5 seconds to enter config mode

Access the installer: [https://installer.zapbox.space/](https://installer.zapbox.space/)<br>
Access the headless installer: [https://installer.zapbox.space/headless/](https://installer.zapbox.space/headless/)

## PlatformIO Project

This project is configured for PlatformIO and based on the Arduino framework for ESP32-S3.

### Required Libraries

- ArduinoJson
- OneButton
- WebSockets
- TFT_eSPI
- QRCode

### Project Structure

```
ZapBox/
├── src/                           # Main application source code
│   ├── main.cpp                   # Entry point and main loop
│   ├── API.cpp/h                  # LNbits API integration (labels, BTC ticker)
│   ├── Display.cpp/h              # Display rendering and theme management
│   ├── DisplayStubs.cpp           # Display stubs for headless mode
│   ├── GlobalState.cpp/h          # Global application state management
│   ├── DeviceState.h              # Device state definitions
│   ├── Input.cpp/h                # Input handling (buttons, touch)
│   ├── Navigation.cpp/h           # Navigation logic and screen management
│   ├── Network.cpp/h              # WiFi and network connectivity
│   ├── Payment.cpp/h              # Payment processing, LNURL/Bech32 encoding, relay control
│   ├── UI.cpp/h                   # User interface components and rendering
│   ├── Utils.cpp/h                # Utility functions (string, math, helpers)
│   ├── NFCBoltCard.cpp/h          # Bolt Card (NTAG424 DNA) authentication and LNURLW
│   ├── NFCCardEmulation.cpp/h     # NFC Card Emulation (PN532 Target Mode, LNURLp via NFC)
│   ├── NFCPN532.cpp/h             # PN532 NFC reader driver (I2C, IRQ-based)
│   ├── TouchCST816S.cpp/h         # Touch display support (CST816S)
│   ├── SerialConfig.cpp/h         # Serial configuration interface
│   ├── ServoControl.cpp/h         # Servo motor control (ESP32Servo, LEDC PWM)
│   └── PinConfig.h                # Hardware pin definitions and GPIO mapping
├── include/                       # Additional headers
│   ├── Log.h                      # Logging utilities
│   └── README                     # Include documentation
├── lib/                           # External library configurations
│   ├── README                     # Library documentation
│   └── TFT_eSPI_Setup/            # TFT_eSPI display driver configuration
├── installer/                     # Web-based firmware installer & configurator
│   ├── index.html                 # Console interface and configuration UI
│   ├── extensions.json            # Extension version manifest
│   ├── extensions-how-to-release.md # Extension release instructions
│   ├── assets/                    # Installer CSS, JavaScript, images
│   ├── firmware/                  # Firmware binaries and release manifests
│   └── headless/                  # Headless device firmware
├── assets/
│   ├── electric/                  # Electrical schematics (Inkscape)
│   │   ├── e926834-Compact/       # Prototype compact
│   │   ├── e928304-Compact/       # Prototype 2 compact
│   │   ├── e928556-Compact/       # Sample device compact
│   │   ├── e931557-Duo/           # First Duo variant
│   │   ├── e932547-Quattro/       # First Quattro variant
│   │   ├── e932714-Duo/           # Duo update
│   │   ├── e935776-Headless/      # Headless variant
│   │   ├── e936954-Quattro/       # Quattro update (button cable & light barrier)
│   │   ├── e937540-Duo/           # Duo update
│   │   ├── e937544-USB-Power-Hub/ # USB Power Hub
│   │   ├── e938714-ZapOMat/       # ZapOMat design
│   │   ├── e938889-Headless/      # Headless update with ZapBox picture
│   │   ├── e938897-Compact/       # Compact update with ZapBox picture
│   │   ├── e939042-Compact/       # Compact with NFC cap
│   │   └── e939705-ZapOMat/       # ZapOMat update
│   ├── housing/                   # 3D models and housing (FreeCAD)
│   │   ├── b926837-Compact/       # Prototype compact
│   │   ├── b928260-Compact/       # Prototype 2 compact
│   │   ├── b928555-Compact/       # Sample device compact
│   │   ├── b930595-Compact/       # Optimization, separate label
│   │   ├── b931760-Duo/           # Prototype Duo
│   │   ├── b932506-Compact/       # Adapter system, 90° front
│   │   ├── b932595-Duo&Quattro/   # Duo & Quattro, 90° and 35° fronts
│   │   ├── b932788-IlluminatedSign/ # LED sign for demo/testing
│   │   ├── b935750-Headless/      # Headless variant
│   │   ├── b937454-USB-Power-Hub/ # USB Power Hub
│   │   ├── b939002-Compact/       # Compact with NFC cap
│   │   ├── b939704-ZapOMat/       # ZapOMat housing
│   │   ├── b940298-Duo/           # Duo update
│   │   └── fonts/                 # Fonts for housing labels
│   ├── operating-instructions/    # User manuals (DE/EN)
│   │   ├── Compact-oi940284-de.md # ZapBox Compact manual (German)
│   │   ├── Compact-oi940284-en.md # ZapBox Compact manual (English)
│   │   ├── Duo-oi940285-de.md     # ZapBox Duo manual (German)
│   │   ├── Duo-oi940285-en.md     # ZapBox Duo manual (English)
│   │   ├── pic-Compact/           # Images for Compact manual
│   │   ├── pic-Duo/               # Images for Duo manual
│   │   └── archive/               # Older manual versions
│   ├── white-paper/               # Technical documentation
│   ├── Pinout-T-DISPLAY-S3.jpg    # T-Display-S3 pinout reference
│   ├── Pinout-T-DISPLAY-S3-TOUCH.png # T-Display-S3 Touch pinout reference
│   └── lightning-address.png      # Lightning address QR code
├── temp/                          # Temporary files
├── platformio.ini                 # PlatformIO build configuration
├── partitions_4mb.csv             # ESP32 partition table (4 MB devices)
├── partitions_16mb.csv            # ESP32 partition table (16 MB devices)
├── FIRMWARE.md                    # Firmware development and release documentation
├── LICENSE                        # Open source license
├── README.md                      # Main project documentation
└── .gitignore                     # Git ignore rules
```

## Compatibility

- **LNbits**: Compatible with v1.4.x or higher
- **Bitcoin Switch Extension**: Compatible with v1.2.1 ohr higher
- **ESP32-S3**: Optimized for LilyGo T-Display-S3
- **ESP32 classic**: A standard ESP32 can also be used. For installation in the headless case, an unsoldered 30-pin board is recommended.

## Versioning

Software versioning, see Releases.
Electrical design and housing variants, see table.

### Housing / 3D modeling (FreeCAD)

| Version | Type | Comment |
|---------|------|---------|
| b926837 | Compact | Prototyp, uses e926834 |
| b928260 | Compact | Prototyp 2, uses e928304 |
| b928555 | Compact | Sample device, uses e928556 |
| b930595 | Compact | Optimization, separate label |
| b931760 | Duo | Prototyp Duo with two front panels, 90 and 35 degrees  |
| b932506 | Compact | Add adapter system, 90-degree front, change USB-C position  |
| b932595 | Duo & Quattro | Prototyp Quattro and update Duo, 90 and 35 degrees front |
| b932788 | Illuminated Sign | Prototyp ZapBox LED sign for demonstration and testing purposes |
| b935750 | Headless | Prototyp Headless - ZapBox without display |
| b937454 | USB-Power-Hub | Prototyp USB-Power-Hub - Just for voltage distribution |
| b939002 | Compact | Compact 35° now with NFC cap |
| b939704 | ZapOMat | ZapOMat No.1 |
| b940298 | Duo | Update & NFC lid |
| b943400 | Headless | Headless with NFC |
| b943614 | Servo | The first one with servo control |
| b944177 | Headless | Mounting plate with snap-fit connection |
| b944666 | Headless Servo | First powerfull Headless Servo |

-> Find all versions here: [./assets/housing)](https://github.com/AxelHamburch/ZapBox/tree/main/assets/housing)

### Electrical layout / circuit diagram (Inkscape)

| Version | Type | Comment |
|---------|------|---------|
| e926834 | Compact | Prototype |
| e928304 | Compact | Prototype 2 |
| e928556 | Compact | Sample device |
| e931557 | Duo | First Duo |
| e932547 | Quattro | First Quattro |
| e932714 | Duo | Duo update |
| e935776 | Headless | First Headless |
| e932547 | Quattro | Add update button cable & IR light barrier |
| e937540 | Duo | Duo Update |
| e937544 | USB-Power-Hub | First USB-Power-Hub |
| e938714 | ZapOMat | Firest ZapOMat design |
| e938889 | Headless | Update Headless with ZapBox picture |
| e938897 | Compact | Update Compact with ZapBox picture |
| e939705 | ZapOMat | ZapOMat No.1 | 
| e940540 | Headless | Update | 
| e943674 | Servo | Start the Servo Story | 
| e944644 | Headless Servo | First powerfull Headless Servo | 

-> Find all versions here: [./assets/electric)](https://github.com/AxelHamburch/ZapBox/tree/main/assets/electric)

## Acknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and uses parts from Ben's [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).

A big thank you goes to [Ben Arc](https://njump.to/nprofile1qqsvrlrhw86l5sv06wkyjgs6rrcekskvk7nx8k50qn9m7mqgeqxjpvgpzamhxue69uhhyetvv9ujumn0wd68ytnzv9hxgtctcf224) and the entire LNbits team for their incredible work.

Parts of this project were developed with the assistance of **GitHub Copilot** powered by **Claude Sonnet** (Anthropic) as an AI coding assistant.

## Support

This is a free and open source project. Support is welcome. Making Bitcoin Lightning technology ⚡ accessible to everyone — worldwide. 🌍

<table>
<tr>
<td>
<img src="assets/lightning-address.png" width="90" alt="Lightning Address QR Code">
</td>
<td>
Lightning⚡Address<br>
axelhamburch@ereignishorizont.xyz
</td>
</tr>
</table>

---

**Lightning ZapBox** - Compact, simple, Bitcoin-powered! ⚡
