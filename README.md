# Lightning ZapBox ⚡

Bitcoin Lightning-controlled USB power switch for LilyGo T-Display-S3

## What is the ZapBox?

The Lightning ZapBox is a compact device that controls a USB output via Bitcoin Lightning payment. Various 5V devices can be operated on the USB output, such as LED lamps, fans, or other USB-powered devices. It features multiple operation modes, customizable display themes, and advanced relay control patterns.

## How it Works

1. **QR Code Display**: The integrated display of the T-Display-S3 shows a QR code with the LNURL for scanning
2. **Lightning Payment**: After scanning and paying the invoice, the payment is sent to the LNbits server
3. **WebSocket Trigger**: The LNbits server sends a signal via WebSocket to the ESP32 microcontroller
4. **Relay Switching**: The ESP32 activates the relay, which turns on the USB output for a specified period (with optional special modes like blinking, pulsing, or strobing)
5. **Confirmation**: The display shows that the payment has been received and the relay has been switched

## Hardware

- **LilyGo T-Display-S3**: ESP32-S3 microcontroller with integrated 170x320 LCD display
  - Available in two versions: **Touch** (with CST816S touch controller) and **Non-Touch**
  - Software automatically detects touch capability at startup
- **Relay Module**: Switches the USB output
- **USB Output Socket**: Provides 5V for connected devices
- **Two Physical Buttons** (Non-Touch version): For navigation and access to features
- **Touch Display** (Touch version): Virtual touch button for Help/Report/Config modes
- **3-Position Switch**:
  - **Position 0**: Everything off
  - **Position 1**: Output permanently on (bypass mode)
  - **Position A**: Automatic mode - ESP32 active, waiting for Lightning payment

### Electrical Layout

See the complete wiring diagram: [E-Layout-ZapBox-Compact.png](assets/electric/E-Layout-ZapBox-Compact.png)

## Operation

### Non-Touch Version (Physical Buttons)

- **Left Button (BOOT)**: 
  - Short press = Report mode (shows error counters)
  - Hold 5 seconds = Configuration mode (serial config interface)
- **Right Button (HELP)**: Show help page with instructions

### Touch Version (Virtual Touch Button)

The Touch version features a **virtual touch button** in the bottom area of the display (works in all orientations):

- **1x Click** (wait 1 second): **Help Mode** - Shows 3 help screens with usage instructions
- **2x Click** (within 1 second): **Report Mode** - Shows error counters and diagnostics (2s first screen, 1s per additional)
- **3x Click** (within 1 second): Nothing (waiting for 4th click)
- **4x Click** (within 1 second): **Config Mode** - Serial configuration interface
  - **Timeout**: 3 minutes (180 seconds) of inactivity
  - **Early Exit**: Touch screen anywhere after 2 seconds to exit and restart

**Touch Button Features:**
- Located at hardware position Y > 305 (bottom 15 pixels)
- Works in both vertical and horizontal display orientations
- 100ms debounce prevents accidental double-clicks
- Independent from main display touch navigation
- Automatically disabled on non-touch hardware

**Touch Navigation:**
- **Swipe** or **Tap** on main display area to navigate between products (Multi-Channel-Control mode)
- Touch button area is excluded from product navigation to prevent conflicts

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
| 1 (Highest) | **NO WIFI** | NW | WiFi connection status | WiFi network not connected |
| 2 | **NO INTERNET** | NI | HTTP check to Google | Internet connectivity lost |
| 3 | **NO SERVER** | NS | TCP port 443 check | LNbits server unreachable |
| 4 (Lowest) | **NO WEBSOCKET** | NWS | WebSocket connection status | WebSocket protocol/handshake failure |

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

**Report Mode**: Press BOOT button to display error counters (0-99) for all four error types with their occurrence counts.

## Features

### Basic Configuration

Configuration is done via the [Web Installer](installer/index.html) with browser-based serial connection:

- WiFi SSID and password
- LNbits server WebSocket URL (supports both `ws://` and `wss://`)
- LNURL for payments
- Display orientation (horizontal/vertical)
- **Display Theme**: Choose from 16 color combinations including:
  - BLACK & WHITE, WHITE & BLACK
  - BLACK & RED, BLACK & OLIVE
  - WHITE & NAVY, WHITE & DARKCYAN
  - GREEN & RED, RED & GREEN
  - GREY & BLUE, ORANGE & BROWN, ORANGE & BLACK
  - BROWN & ORANGE, BROWN & YELLOW
  - MAROON & MAGENTA, DARKCYAN & CYAN
  - DARK GREY & LIGHT GREY
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
| **Light Sleep** | OFF | Active | Paused | Active | Reconnect | Reconnect | ❌ No* | ~0.8-3mA | ~98-99% | ~1-2s |
| **Deep Sleep** | OFF | Active | OFF | RTC only | Reconnect | Reconnect | ❌ No* | ~0.01-0.15mA | ~99.9% | ~3-5s |

*Light Sleep and Deep Sleep require wake-up (button press) and WiFi reconnection before payments can be received

**Wake-up Methods**:
- **Screensaver**: Touch display (Touch version) or press any button → Instant wake-up
- **Deep Sleep**: Press BOOT or HELP button only (touch disabled for maximum power savings)

**Mode Recommendations**:

- **Screensaver (backlight off)**: ⭐ **Best for payment terminals** - Instant wake-up, 80-90% power saving, payments always work
  - Use when device should respond immediately to payments
  - Good for public terminals with frequent use
  - Battery operation: ~7-10 days with 10000mAh battery
  
- **Light Sleep**: ⭐ **Best for button-activated devices** - 98-99% power saving, fast wake-up, good battery life
  - WiFi reconnects quickly after wake-up (~1-2 seconds)
  - NO payments received during sleep (CPU is paused)
  - Press button to wake, then ready for payments quickly
  - Battery operation: ~83-139 days with 10000mAh battery
  
- **Deep Sleep (freeze)**: ⭐ **Best for long-term installations** - 99.9% power saving, maximum battery life
  - WiFi reconnects after wake-up (~3-5 seconds)
  - NO payments received during sleep
  - Press button to wake, slower reconnect than light sleep
  - Battery operation: 7.5-114 years(!) with 10000mAh battery
  - Ideal for devices used rarely

**Configuration**:

- **Screensaver Options**:
  - **OFF**: No power saving (default)
  - **Backlight Off**: Display backlight turns off - recommended option

- **Deep Sleep Options**:
  - **OFF**: No deep sleep (default)
  - **Light**: Light sleep mode - CPU pauses, faster wake-up, NO payments during sleep
  - **Freeze**: Deep sleep mode - Maximum power saving, slowest wake-up, NO payments during sleep

- **Mutual Exclusion**: Only one mode can be active at a time (Screensaver or Deep Sleep)
- **Activation Time**: Configurable timeout (1-120 minutes)
- **Wake-up**: Press BOOT button or IO14 button to wake from sleep
- **Payment Processing**: Only Screensaver mode can receive payments during power saving

**Technical Notes**:
- **Screensaver - Backlight Power**: The T-Display-S3's backlight consumes most display power (~150-200mA). Turning it off saves 80-90% while keeping the display controller active for instant wake-up. CPU continues running, payments work normally.
- **Light Sleep - CPU Pause**: CPU is paused to save power (~0.8-3mA total), WiFi disconnects but RAM is retained. Wake-up is fast (~1-2s) because no full reboot needed. NO payment processing during sleep.
- **Deep Sleep - Complete Shutdown**: Only RTC memory active, device performs full restart on wake-up (~3-5s), requires complete WiFi reconnection. NO payment processing during sleep. Maximum battery life.

**Use Cases**: Energy saving for installations, battery operation, reducing device heat, extending display lifespan in always-on scenarios

## Web Installer

The ZapBox includes a browser-based Web Installer for easy firmware updates and configuration:

1. **Flash Firmware**: Select version and flash via USB (Chrome/Edge required)
2. **Configure Device**: Set WiFi, LNbits connection, display settings, and advanced features
3. **Serial Console**: Debug and monitor device in real-time
4. **Automatic Config Mode Detection**: Press BOOT button for 5 seconds to enter config mode

Access the installer: [https://ereignishorizont.xyz/ZapBox/](https://ereignishorizont.xyz/ZapBox/)

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
├── src/
│   ├── main.cpp           # Main application logic
│   ├── Display.cpp/h      # Display functions and themes
│   ├── SerialConfig.cpp/h # Serial configuration interface
│   └── PinConfig.h        # Hardware pin definitions
├── installer/
│   ├── index.html         # Web-based installer and configuration tool
│   ├── assets/            # Web installer assets (CSS, JS, images)
│   └── firmware/          # Firmware versions with binaries and manifests
├── assets/
│   ├── electric/          # Electrical layouts and circuit diagrams
│   ├── housing/           # 3D models and FreeCAD files
│   └── lightning-address.png
├── lib/                   # TFT_eSPI configuration
├── include/               # Additional headers
├── platformio.ini         # PlatformIO configuration
├── partitions_16mb.csv    # ESP32 partition table
├── FIRMWARE.md            # Firmware release process documentation
├── RELEASE.md             # GitHub release process documentation
└── TODO.md                # Development roadmap
```

## Compatibility

- **LNbits**: Compatible with v0.12.12+ and v1.3.1+ (supports both `ws://` and `wss://`)
- **Bitcoin Switch Extension**: Compatible with v1.1.2+
- **ESP32-S3**: Optimized for LilyGo T-Display-S3

## Acknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and uses parts from Ben's [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).

A big thank you goes to [Ben Arc](https://njump.to/nprofile1qqsvrlrhw86l5sv06wkyjgs6rrcekskvk7nx8k50qn9m7mqgeqxjpvgpzamhxue69uhhyetvv9ujumn0wd68ytnzv9hxgtctcf224) and the entire LNbits team for their incredible work.

## Versioning

Software versioning, see Releases.
Electrical design and housing variants, see table.

### Housing / 3D modeling (FreeCAD)

| Version | Type | Comment |
|---------|------|---------|
| b926837 | compact | Prototyp, uses e926834 |
| b928260 | compact | Prototyp 2, uses e928304 |
| b928555 | compact | Sample device, uses e928556 |

### Electrical layout / circuit diagram (Inkscape)

| Version | Type | Comment |
|---------|------|---------|
| e926834 | compact | Prototype |
| e928304 | compact | Prototype 2 |
| e928556 | compact | Sample device |

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
