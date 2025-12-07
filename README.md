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

#### Threshold Mode
Monitor a wallet balance and trigger the relay when a threshold is reached:
- Configure wallet invoice/read key
- Set threshold amount in satoshi
- Define GPIO pin and control duration
- Use static LNURL or Lightning Address for payments
- Payments accumulate in the wallet until threshold is reached

**Use Cases**: Crowdfunding triggers, donation goals, pay-per-use with accumulated balance

#### Special Modes
Control relay switching patterns beyond simple on/off:
- **Standard**: Simple on/off (default)
- **Blink**: 1 Hz, 1:1 duty cycle
- **Pulse**: 2 Hz, 1:4 duty cycle (short pulses)
- **Strobe**: 5 Hz, 1:1 duty cycle (fast blinking)
- **Custom**: Set your own frequency (0.1-10 Hz) and duty cycle ratio (0.1-10)

**Use Cases**: LED effects, motor speed control, warning signals, custom patterns

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
