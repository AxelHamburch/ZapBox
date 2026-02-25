# Lightning ZapBox ⚡

Bitcoin Lightning-controlled USB power switch for LilyGo T-Display-S3 or standard ESP32

## What is the ZapBox?

The Lightning ZapBox is a compact device that controls a USB output via Bitcoin Lightning payment. Various 5V devices can be operated on the USB output, such as LED lamps, fans, or other USB-powered devices. It features multiple operation modes, customizable display themes, and advanced relay control patterns. 

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
2. **Lightning Payment**: After scanning and paying the invoice, the payment is sent to the LNbits server
3. **WebSocket Trigger**: The LNbits server sends a signal via WebSocket to the ESP32 microcontroller
4. **Relay Switching**: The ESP32 activates the relay, which turns on the USB output for a specified period (with optional special modes like blinking, pulsing, or strobing)
5. **Confirmation**: The display shows that the payment has been received and the relay has been switched *(T-Display-S3 only)*

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
- **PN532 NFC Reader** (Optional, in planning): For contactless NFC card/tag reading
  - Connected via I2C (shared bus with Touch controller)
  - Enables NFC-based payment triggers and card identification

### ESP32 Dev Module (Headless Version)

- **Microcontroller**: ESP32 (classic) without display
- **Memory**: 4MB Flash, 512KB SRAM
- **Operation**: Fully functional headless mode - all core features work via serial configuration
- **Status LED**: GPIO 21 with distinct blink patterns (3 fast boot blinks, fast blink during startup, slow blink in config mode, solid when ready, error blink patterns 1-4 for network issues)
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
| 12 | Relay 1 | Output | - | Single mode default, Duo/Quattro mode 1 |
| 13 | Relay 2 | Output | - | Duo/Quattro mode 2 |
| 10 | Relay 3 | Output | - | Quattro mode 3 |
| 11 | Relay 4 / Ambient | Output | - | Quattro mode 4, or ambient lighting (synced with backlight) |

**I2C Bus Addresses:**
- Touch CST816S/CST328: `0x15` or `0x5A`
- PN532 NFC Reader: `0x24`

#### ESP32 Dev Module GPIO Mapping (ENABLE_DISPLAY=0 - Headless)

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 0 | BOOT Button | Input | Pull-up | Wake from sleep / Config mode |
| 14 | HELP Button | Input | Pull-up | Help/Report mode |
| 2 | Light Barrier | Input | Pull-up | Optional NPN light barrier (not used in headless) |
| **LEDs & Status** |
| 21 | Status LED | Output | HIGH=ON | Status indication (RTC-capable) |
| **I2C (Optional)** |
| 17 | I2C SCL | I2C | - | Optional: NFC reader |
| 18 | I2C SDA | I2C | - | Optional: NFC reader |
| 1 | NFC IRQ | Input | - | Optional: PN532 interrupt |
| **Power & Control** |
| 15 | Power On | Output | - | Power control pin |
| **Relay Channels (Multi-Channel-Control)** |
| 12 | Relay 1 | Output | - | Single/Duo/Quattro mode 1 |
| 13 | Relay 2 | Output | - | Duo/Quattro mode 2 |
| 10 | Relay 3 | Output | - | Quattro mode 3 |
| 11 | Relay 4 | Output | - | Quattro mode 4 |

#### Key Differences Between Variants

| Feature | T-Display-S3 | ESP32 Dev |
|---------|--------------|-----------|
| Display | LCD (170x320) | None (Headless) |
| Touch | CST816S/CST328 | N/A |
| External LED Button | Supported (GPIO 43/44) | N/A |
| Light Barrier | Supported (GPIO 2) | Available but not typically used |
| Status Indication | Display + LED | LED only (GPIO 21 and onboard LED GPIO 2) |
| NFC Support | Yes (GPIO 1, 17, 18) | Yes (optional) |
| Power Consumption | ~150-250mA | Lower (no display overhead) |
| Configuration Method | Web Installer + Serial | Web Installer + Serial |
| Deep Sleep Wake | GPIO 0, 14 (not 43/44) | N/A |

### Vending Machine Light Barrier (Optional)

**Feature:** Optical item detection for vending machines via infrared light barrier.

**Use Cases:** This allows automated systems to detect the end of the conveying cycle, as the product has fallen through the light barrier. The conveying cycle is thus terminated prematurely.

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

### Relay Control Pins

**Multi-Channel Configuration:**
- **Single Mode (default)**: Pin 12 only
- **Duo Mode**: Pins 12 and 13
- **Quattro Mode**: Pins 12, 13, 10, and 11
  - **Special Option**: Pin 11 can be configured as ambient lighting switch (syncs with display backlight)

**Output Type:** Digital GPIO outputs (HIGH = relay activated)
**Max Current per Pin:** ~40mA (requires external relay driver for high-power loads)
**Typical Usage:** Relay driver IC (ULN2003/ULN2803) or MOSFET for external circuits

### NFC Reader Setup (Optional, in planning)

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
- **Display Theme**: Choose from 16 color combinations including:
  - BLACK & WHITE, BLACK & DARKCYAN
  - BLACK & RED, BLACK & OLIVE
  - WHITE & NAVY, WHITE & DARKCYAN
  - GREEN & RED, RED & GREEN
  - GREY & BLUE, ORANGE & BROWN, ORANGE & BLACK
  - BROWN & ORANGE, BROWN & YELLOW
  - MAROON & MAGENTA, DARKCYAN & CYAN
  - BLACK & DARK GREY
  - BLACK & LIGHT GREY

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

**Use Cases**: Vending machines, multi-product payment terminals, flexible product offerings

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

#### Screensaver & Deep Sleep Function
Automatic power-saving modes that activate after a configurable timeout:

**Power Consumption Comparison**:

| Mode | Backlight | Display | CPU | RAM | WiFi | WebSocket | Payments | Power | Savings | Wake-up |
|------|-----------|---------|-----|-----|------|-----------|----------|-------|---------|---------|
| **Normal** | ON | Active | Running | Active | Active | Active | ✅ Yes | ~150-250mA | 0% | - |
| **Screensaver** | OFF | Active | Running | Active | Active | Active | ✅ Yes | ~40-60mA | ~80-90% | Instant |
| **Deep Sleep** | OFF | Active | OFF | RTC only | Reconnect | Reconnect | ❌ No* | ~0.01-0.15mA | ~99.9% | ~3-5s |

*Deep Sleep requires wake-up (button press) and WiFi reconnection before payments can be received

**Wake-up Methods**:
- **Screensaver**: Touch display (Touch version) or press any button → Instant wake-up
- **Deep Sleep**: Press BOOT or HELP button only (touch disabled for maximum power savings)

**Mode Recommendations**:

- **Screensaver (backlight off)**: ⭐ **Best for payment terminals** - Instant wake-up, 80-90% power saving, payments always work
  - Use when device should respond immediately to payments
  - Good for public terminals with frequent use
  - Battery operation: ~7-10 days with 10000mAh battery
  
- **Deep Sleep (freeze)**: ⭐ **Best for long-term installations** - 99.9% power saving, maximum battery life
  - WiFi reconnects after wake-up (~3-5 seconds)
  - NO payments received during sleep
  - Press button to wake and device will restart
  - Battery operation: 7.5-114 years(!) with 10000mAh battery
  - Ideal for devices used rarely or for maximum energy savings

**Configuration**:

- **Screensaver Options**:
  - **OFF**: No power saving (default)
  - **Backlight Off**: Display backlight turns off - recommended option

- **Deep Sleep Options**:
  - **OFF**: No deep sleep (default)
  - **Freeze**: Deep sleep mode - Maximum power saving, NO payments during sleep

- **Mutual Exclusion**: Only one mode can be active at a time (Screensaver or Deep Sleep)
- **Activation Time**: Configurable timeout (1-120 minutes)
- **Wake-up**: Press BOOT button or IO14 button to wake from sleep
- **Payment Processing**: Only Screensaver mode can receive payments during power saving

**Technical Notes**:
- **Screensaver - Backlight Power**: The T-Display-S3's backlight consumes most display power (~150-200mA). Turning it off saves 80-90% while keeping the display controller active for instant wake-up. CPU continues running, payments work normally.
- **Deep Sleep - Complete Shutdown**: Only RTC memory active, device performs full restart on wake-up (~3-5s), requires complete WiFi reconnection. NO payment processing during sleep. Maximum battery life.

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
│   ├── API.cpp/h                  # LNbits Bitcoin Switch API integration
│   ├── Display.cpp/h              # Display rendering and theme management
│   ├── DisplayStubs.cpp           # Display stubs for headless mode
│   ├── GlobalState.cpp/h          # Global application state management
│   ├── DeviceState.h              # Device state definitions
│   ├── Input.cpp/h                # Input handling (buttons, touch)
│   ├── Navigation.cpp/h           # Navigation logic and screen management
│   ├── Network.cpp/h              # WiFi and network connectivity
│   ├── Payment.cpp/h              # Payment processing and relay control
│   ├── UI.cpp/h                   # User interface components and rendering
│   ├── Utils.cpp/h                # Utility functions (string, math, helpers)
│   ├── NFCPN532.cpp/h             # NFC reader support (PN532)
│   ├── TouchCST816S.cpp/h         # Touch display support (CST816S)
│   ├── SerialConfig.cpp/h         # Serial configuration interface
│   └── PinConfig.h                # Hardware pin definitions and GPIO mapping
├── include/                       # Additional headers
│   ├── Log.h                      # Logging utilities
│   └── README                     # Include documentation
├── lib/                           # External library configurations
│   ├── README                     # Library documentation
│   └── TFT_eSPI_Setup/            # TFT_eSPI display driver configuration
├── installer/                     # Web-based firmware installer & configurator
│   ├── index.html                 # Console interface and configuration UI
│   ├── how-to-upload.md           # Installation instructions
│   ├── assets/                    # Installer CSS, JavaScript, images
│   ├── firmware/                  # Firmware binaries and release manifests
│   ├── headless/                  # Headless device firmware
│   └── templates/                 # HTML templates
├── assets/
│   ├── electric/                  # Electrical schematics (Inkscape)
│   │   ├── e926834-Compact/       # Prototype compact
│   │   ├── e928304-Compact/       # Prototype 2 compact
│   │   ├── e928556-Compact/       # Sample device compact
│   │   ├── e931557-Duo/           # First Duo variant
│   │   ├── e932547-Quattro/       # First Quattro variant
│   │   ├── e932714-Duo/           # Duo update
│   │   ├── e935776-Headless/      # Headless variant
│   │   ├── e937540-Duo/           # Duo update
│   │   └── e937544-USB-Power-Hub/ # USB Power Hub
│   ├── housing/                   # 3D models and housing (FreeCAD)
│   │   ├── b926837-Compact/       # Prototype compact
│   │   ├── b928260-Compact/       # Prototype 2 compact
│   │   ├── b931760-Duo/           # Prototype Duo
│   │   ├── b932595-Quattro/       # Prototype Quattro
│   │   ├── b935750-Headless/      # Headless variant
│   │   └── b937544-USB-Power-Hub/ # USB Power Hub
│   ├── white-paper/               # Technical documentation
│   └── lightning-address.png      # Lightning address QR code
├── platformio.ini                 # PlatformIO build configuration
├── partitions_4mb.csv             # ESP32 partition table (4 MB devices)
├── partitions_16mb.csv            # ESP32 partition table (16 MB devices)
├── FIRMWARE.md                    # Firmware development and release documentation
├── LICENSE                        # Open source license
├── README.md                       # Main project documentation
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
| b935750 | Headless |  Prototyp Headless - ZapBox without display |
| b937454 | USB-Power-Hub |  Prototyp USB-Power-Hub - Just for voltage distribution |

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

-> Find all versions here: [./assets/electric)](https://github.com/AxelHamburch/ZapBox/tree/main/assets/electric)

## Acknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and uses parts from Ben's [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).

A big thank you goes to [Ben Arc](https://njump.to/nprofile1qqsvrlrhw86l5sv06wkyjgs6rrcekskvk7nx8k50qn9m7mqgeqxjpvgpzamhxue69uhhyetvv9ujumn0wd68ytnzv9hxgtctcf224) and the entire LNbits team for their incredible work.

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
