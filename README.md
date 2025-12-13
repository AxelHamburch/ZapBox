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
- **Relay Module**: Switches the USB output
- **USB Output Socket**: Provides 5V for connected devices
- **Two Buttons**: For navigation and access to the help page
- **3-Position Switch**:
  - **Position 0**: Everything off
  - **Position 1**: Output permanently on (bypass mode)
  - **Position A**: Automatic mode - ESP32 active, waiting for Lightning payment

### Electrical Layout

See the complete wiring diagram: [E-Layout-ZapBox-Compact.png](assets/electric-layout/E-Layout-ZapBox-Compact.png)

## Operation

- **Left Button (BOOT)**: 
  - Short press = Report mode (shows error counters)
  - Hold 5 seconds = Configuration mode (serial config interface)
- **Right Button (HELP)**: Show help page with instructions

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

### Advanced Features

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
| **Light Sleep** | OFF | Active | Paused | Active | Active | Active | ✅ Yes | ~3-5mA | ~97-99% | ~3-5ms |
| **Deep Sleep** | OFF | Active | OFF | RTC only | Reconnect | Reconnect | ❌ No* | ~0.01-0.15mA | ~99.9% | ~200-500ms |

*Deep Sleep requires wake-up and WiFi reconnection before payments can be received

**Mode Recommendations**:

- **Screensaver (backlight off)**: ⭐ **Best for active terminals** - Instant wake-up, 80-90% power saving, payments always work
  - Use when device should respond immediately to payments
  - Good for public terminals with frequent use
  
- **Light Sleep**: ⭐ **Best for semi-active terminals** - 97-99% power saving, payments work during sleep, fast wake-up
  - WiFi stays connected, WebSocket active
  - Payments trigger automatic wake-up
  - Battery operation: 2-3 weeks instead of 2-3 days
  - USB terminal stays connected (may disconnect on some systems)
  
- **Deep Sleep (freeze)**: ⭐ **Best for long-term installations** - 99.9% power saving, maximum battery life
  - WiFi disconnects during sleep
  - No payments received until wake-up
  - Battery operation: 3-6 months
  - Ideal for button-activated use cases

**Configuration**:

- **Screensaver Options**:
  - **OFF**: No power saving (default)
  - **Backlight Off**: Display backlight turns off - recommended option

- **Deep Sleep Options**:
  - **OFF**: No deep sleep (default)
  - **Light**: Light sleep mode - CPU pauses, WiFi active, payments work
  - **Freeze**: Deep sleep mode - Maximum power saving, WiFi disconnects

- **Mutual Exclusion**: Only one mode can be active at a time (Screensaver or Deep Sleep)
- **Activation Time**: Configurable timeout (1-120 minutes)
- **Wake-up**: Press BOOT button or IO14 button to deactivate and resume normal operation
- **Automatic Wake-up**: In Light Sleep mode, incoming payments automatically wake the device

**Technical Notes**:
- **Screensaver - Backlight Power**: The T-Display-S3's backlight consumes most display power (~150-200mA). Turning it off saves 80-90% while keeping the display controller active for instant wake-up.
- **Light Sleep - WiFi Modem**: WiFi stays in modem sleep mode (~3-5mA total), WebSocket connection remains active, CPU wakes on network events or GPIO interrupts.
- **Deep Sleep - Complete Shutdown**: Only RTC memory active, device restarts on wake-up, requires WiFi reconnection (~200-500ms boot time).
- **USB Serial**: In Light Sleep, USB connection may disconnect on some systems. Terminal will reconnect automatically after wake-up.

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

### Build and Upload

```bash
# Compile and upload to device
platformio run --target upload

# Monitor serial output
platformio device monitor
```

### Project Structure

```
ZapBox/
├── src/
│   ├── main.cpp           # Main application logic
│   ├── Display.cpp/h      # Display functions and themes
│   ├── SerialConfig.cpp/h # Serial configuration interface
│   └── PinConfig.h        # Hardware pin definitions
├── installer/             # Web-based installer and configuration tool
├── assets/
│   └── electric-layout/   # Wiring diagrams and schematics
└── platformio.ini         # PlatformIO configuration
```

## Compatibility

- **LNbits**: Compatible with v0.12.12+ and v1.3.1+ (supports both `ws://` and `wss://`)
- **Bitcoin Switch Extension**: Compatible with v1.1.2+
- **ESP32-S3**: Optimized for LilyGo T-Display-S3

## Acknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and uses parts from [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).

---

**Lightning ZapBox** - Compact, simple, Bitcoin-powered! ⚡
