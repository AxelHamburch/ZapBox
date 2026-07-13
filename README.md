# Lightning - Zap⚡Box

> **Forked from [danielcharrua/satoffee](https://github.com/danielcharrua/satoffee)**
> Expanded with many new features and hardware for many new use cases.

**A Bitcoin Lightning-controlled switching unit.** Pay an invoice — a relay switches. That's the core idea, and everything else is built on it: vending machines, tip boxes, kiosk terminals, coffee machines, LED signs, servo dispensers.

Ease of use, verifiability and reliability are paramount — all in one package.

📖 Detailed descriptions, images and application examples: [zapbox.space](https://zapbox.space/) · [ereignishorizont.xyz](https://ereignishorizont.xyz/en/zapbox-en/) · [white paper](https://github.com/AxelHamburch/ZapBox/tree/main/assets/white-paper)

---

## Table of Contents

- [How it Works](#how-it-works)
- [Which Variant Do I Need?](#which-variant-do-i-need)
- [Feature Matrix](#feature-matrix)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [Development](#development)
- [Compatibility](#compatibility)
- [Acknowledgement](#acknowledgement)
- [Support](#support)

---

## How it Works

1. **QR code** — the display shows a QR code containing the LNURL *(display variants)*
2. **Lightning payment** — the customer scans and pays. Or they tap an **NFC Bolt Card** on the reader, or hold a **smartphone** near the NFC tag — all three routes work
3. **WebSocket trigger** — the LNbits server pushes the payment confirmation to the ESP32
4. **Relay switches** — the output is activated for the configured duration (optionally blinking, pulsing or strobing)
5. **Confirmation** — the display shows the payment was received *(display variants)*

The ZapBox never handles funds itself. It only listens for a confirmation from **your** LNbits server and switches a pin.

---

## Which Variant Do I Need?

Four hardware variants share the same firmware codebase. Pick by **how the customer interacts** with the device:

### 🖥️ [T-Display-S3](docs/t-display-s3.md) — the classic

Small integrated display (170 × 320), optional touch. The most widely deployed ZapBox and the best starting point for most builds.
**4 relay channels** · 2 buttons · optional servo mode · ~150–250 mA

*→ General retail, vending machines, coffee machines, tip boxes*

### 📱 [Touch 3.5"](docs/touch-3.5.md) — the big one

Large 3.5" capacitive touch display (480 × 320). The only variant with a **Mini-PoS mode** (customer-facing amount entry) and a **battery gauge**.
**6 flex channels** · vending sensors · LiPo battery support · ~200–350 mA

*→ Kiosk terminals, market stalls, flea markets, club bars, public vending installations*

### 💡 [Headless ESP32](docs/headless-esp32.md) — the workhorse

No display — status is signalled by a single LED with distinct blink patterns. Has by far the **most channels**.
**12 relay channels** · 2 vending sensors · ~100–150 mA

*→ Embedded and hidden installations, wall-mounted relay control, multi-channel dispensers*

### ⚡ [ESP32-C3](docs/esp32-c3.md) — the small one

Single-core RISC-V, the smallest footprint and the lowest power draw of all variants.
**1 relay + 2 flex channels** · ~80–120 mA

*→ Extremely tight spaces, battery-powered installs, cost-sensitive volume deployments*

---

## Feature Matrix

| | T-Display-S3 | Touch 3.5" | Headless ESP32 | ESP32-C3 |
|---|---|---|---|---|
| **Processor** | ESP32-S3 dual-core | ESP32-S3 dual-core | ESP32 dual-core | ESP32-C3 single-core RISC-V |
| **Display** | LCD 170×320 | Touch 480×320 | — | — |
| **Memory** | 16 MB / 8 MB PSRAM | 16 MB / 8 MB PSRAM | 4 MB / 512 KB | 4 MB / 400 KB |
| **Relay channels** | 4 | 6 | **12** | 1 (+2 flex) |
| **I/O Expander** (+8 channels) | ✅ | — | ✅ | — |
| **Servo mode** | ✅ | ✅ per channel | ✅ | ✅ |
| **Vending sensors** | 1 (light barrier) | 2 | 2 | 2 |
| **NFC** (Bolt Card, phone tap) | ✅ | ✅ | ✅ | ✅ |
| **Identity🫆Login** | LNURL-auth + NFC | LNURL-auth + NFC + PIN | NFC only | — |
| **Mini-PoS** (amount entry) | — | ✅ | — | — |
| **Battery gauge** | — | ✅ | — | — |
| **BTC ticker** | ✅ | ✅ | — | — |
| **Screensaver / Deep Sleep** | ✅ | ✅ | ✅ | ✅ |
| **Power draw** | ~150–250 mA | ~200–350 mA | ~100–150 mA | ~80–120 mA |
| **Firmware suffix** | *(none)* | `t` | `h` | `c` |

---

## Quick Start

1. **Build the hardware** — follow the [electrical layout](docs/hardware-assets.md#electrical-layouts) for your variant
2. **Set up LNbits** — you need a server with the [bitcoinswitch](https://github.com/lnbits/bitcoinswitch) or [zapbox](https://github.com/AxelHamburch/zapbox_extension) extension, and a switch entry per channel
3. **Flash and configure** — open the Web Installer in Chrome or Edge, flash the firmware, then enter WiFi and LNbits settings over the same serial connection

| Variant | Web Installer |
|---------|---------------|
| T-Display-S3 | [installer.zapbox.space](https://installer.zapbox.space/) |
| Touch 3.5" | [installer.zapbox.space/touch3.5/](https://installer.zapbox.space/touch3.5/) |
| Headless ESP32 | [installer.zapbox.space/headless/](https://installer.zapbox.space/headless/) |
| ESP32-C3 | [installer.zapbox.space/c3/](https://installer.zapbox.space/c3/) |

**Entering config mode on an already-flashed device:** hold the BOOT button for 5 seconds.

---

## Documentation

### Hardware variants — pin maps, channels, board-specific features

- 🖥️ **[T-Display-S3](docs/t-display-s3.md)** — GPIO map, 4 channels, servo mode, ambient lighting, light barrier
- 📱 **[Touch 3.5"](docs/touch-3.5.md)** — GPIO map, 6 flex channels, Mini-PoS, battery gauge, vending sensors
- 💡 **[Headless ESP32](docs/headless-esp32.md)** — GPIO map, 12 channels, LED diagnostics, vending sensors
- ⚡ **[ESP32-C3](docs/esp32-c3.md)** — GPIO map, relay + flex channels

### Features across all variants

- 🔧 **[Common Features](docs/common-features.md)** — special modes, BTC ticker, threshold mode, I/O expander, LED button, screensaver & deep sleep, startup & error diagnostics
- 📡 **[NFC](docs/nfc.md)** — Bolt Card payments, card emulation, NT3H2111 phone tap
- 🫆 **[Identity🫆Login](docs/identity.md)** — LNURL-auth and NTAG 424 DNA login without payment

### Hardware & documents

- 🛠️ **[Housings, Layouts & Manuals](docs/hardware-assets.md)** — 3D models, wiring diagrams, operating instructions, QA protocols
- 📦 **[Firmware Release Process](FIRMWARE.md)** — how releases are built

---

## Development

PlatformIO project, Arduino framework, ESP32.

```bash
pio run -e lilygo-t-display-s3    # T-Display-S3
pio run -e Touch3_5               # Touch 3.5"
pio run -e esp32dev               # Headless
pio run -e esp32-c3-21-1          # ESP32-C3
```

**Libraries:** ArduinoJson · OneButton · WebSockets · TFT_eSPI · QRCode

<details>
<summary><strong>Project structure</strong></summary>

```
ZapBox/
├── src/                           # Firmware source (C++)
│   ├── main.cpp                   # Entry point and main event loop
│   ├── API.cpp/h                  # LNbits API & Bitcoin ticker
│   ├── Battery.cpp/h              # Battery gauge (Touch 3.5")
│   ├── Display.cpp/h              # Display rendering (T-Display-S3)
│   ├── DisplayTouch.cpp           # Display rendering (Touch 3.5")
│   ├── DisplayStubs.cpp           # No-op display layer (headless)
│   ├── GlobalState.cpp/h          # Global state & configuration
│   ├── Input.cpp/h                # Button & touch input
│   ├── Navigation.cpp/h           # Screen navigation
│   ├── Network.cpp/h              # WiFi & WebSocket
│   ├── Payment.cpp/h              # Payment processing & LNURL encoding
│   ├── UI.cpp/h                   # UI components
│   ├── NFCPN532.cpp/h             # PN532 NFC reader driver
│   ├── NFCBoltCard.cpp/h          # Bolt Card LNURLW authentication
│   ├── NFCCardEmulation.cpp/h     # PN532 target mode
│   ├── NFCNT3H2111.cpp/h          # NT3H2111 NFC Tag 2 driver
│   ├── IOExpander.h               # PCF8574 driver (virtual pins 200–207)
│   ├── I2CBus.h                   # Shared I²C bus mutex
│   ├── ServoControl.cpp/h         # Servo motor control
│   ├── TouchCST816S.cpp/h         # Touch driver (T-Display-S3)
│   ├── TouchAXS15231B.cpp/h       # Touch driver (Touch 3.5")
│   ├── SerialConfig.cpp/h         # Serial configuration interface
│   └── PinConfig.h                # Hardware pin definitions (all boards)
├── docs/                          # Documentation (you are here)
├── installer/                     # Web installer & firmware binaries
├── assets/                        # Housings, layouts, manuals, QA
└── platformio.ini                 # Build configuration
```

</details>

---

## Compatibility

- **LNbits**: v1.4.x or higher
- **Bitcoin Switch Extension**: v1.2.1 or higher
- **ZapBox Extension**: required for NFC Bolt Card payments and Mini-PoS
- **ESP32 classic**: a standard ESP32 works for the headless build — an **unsoldered 30-pin board** is recommended for the headless housing

Software versioning follows the [Releases](https://github.com/AxelHamburch/ZapBox/releases). Housings (`b…`) and electrical layouts (`e…`) are versioned by Bitcoin block height — see [Hardware & Documents](docs/hardware-assets.md).

---

## Acknowledgement

This project is based on Daniel's [SATOFFEE](https://github.com/danielcharrua/satoffee) and uses parts from Ben's [bitcoinswitch](https://github.com/lnbits/bitcoinswitch).

A big thank you goes to [Ben Arc](https://njump.to/nprofile1qqsvrlrhw86l5sv06wkyjgs6rrcekskvk7nx8k50qn9m7mqgeqxjpvgpzamhxue69uhhyetvv9ujumn0wd68ytnzv9hxgtctcf224) and the entire LNbits team for their incredible work.

Parts of this project were developed with the assistance of AI coding assistants (**GitHub Copilot** and **Claude**).

---

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

**Lightning ZapBox** — Compact, simple, Bitcoin-powered! ⚡
