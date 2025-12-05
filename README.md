# Lightning ZapBox ⚡

Bitcoin Lightning-controlled USB power switch for LilyGo T-Display-S3

## What is the ZapBox?

The Lightning ZapBox is a compact device that controls a USB output via Bitcoin Lightning payment. Various 5V devices can be operated on the USB output, such as LED lamps, fans, or other USB-powered devices.

## How it Works

1. **QR Code Display**: The integrated display of the T-Display-S3 shows a QR code with the LNURL for scanning
2. **Lightning Payment**: After scanning and paying the invoice, the payment is sent to the LNbits server
3. **WebSocket Trigger**: The LNbits server sends a signal via WebSocket to the ESP32 microcontroller
4. **Relay Switching**: The ESP32 activates the relay, which turns on the USB output for a specified period
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

## Operation

- **Left Button**: Short press = Report mode, hold 3 seconds = Configuration mode
- **Right Button**: Show help page

## Configuration

Configuration is done via serial port or the web interface in the `installer` folder:

- WiFi SSID and password
- LNbits server WebSocket URL
- LNURL for payments
- Display orientation (horizontal/vertical)

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
platformio run --target upload
```

## Aknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and it uses parts from [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).


---

**Lightning ZapBox** - Compact, simple, Bitcoin-powered! ⚡
