# ZapBox Headless (ESP32 Dev Module)

Classic ESP32 without a display. Status is signalled entirely through a single LED — which makes the [LED pattern reference](#led-status-diagnostics) the most important part of this page.

The headless variant has the **most relay channels of any ZapBox: 12**.

**Firmware suffix:** `h` (e.g. `v957859h`) · **Web Installer:** [installer.zapbox.space/headless/](https://installer.zapbox.space/headless/)

---

## Table of Contents

- [Hardware](#hardware)
- [GPIO Mapping](#gpio-mapping)
- [Relay Channels (12)](#relay-channels-12)
- [ZapBox Mode (CH01)](#zapbox-mode-ch01)
- [Vending Sensors](#vending-sensors)
- [LED Status Diagnostics](#led-status-diagnostics)
- [Troubleshooting with LED Patterns](#troubleshooting-with-led-patterns)
- [Identity🫆Login](#identitylogin)
- [Shared Features](#shared-features)

---

## Hardware

| | |
|---|---|
| **Microcontroller** | ESP32 (classic), dual-core |
| **Display** | none — status LED only |
| **Memory** | 4 MB Flash, 512 KB SRAM |
| **Power draw** | ~100–150 mA |
| **Channels** | 12 relay channels (+ 8 more via [I/O Expander](common-features.md#io-expander--pcf8574)) |
| **Configuration** | Web Installer (serial) |

**Advantages:** lower cost, smaller footprint, lower power consumption.
**Use cases:** embedded installations, wall-mounted relay control, hidden installations.

> **Hardware tip:** for installation in the headless housing, an **unsoldered 30-pin board** is recommended.

---

## GPIO Mapping

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 0 | BOOT Button | Input | Pull-up | Wake from sleep / Config mode |
| 2 | Onboard LED | Output | HIGH=ON | Additional status LED |
| **LEDs & Status** |
| 21 | Status LED | Output | HIGH=ON | Main status indication (RTC-capable) |
| **I²C (optional)** |
| 17 | I²C SCL | I²C | - | NFC reader, NT3H2111, PCF8574 |
| 18 | I²C SDA | I²C | - | NFC reader, NT3H2111, PCF8574 |
| 4 | NFC IRQ | Input | - | PN532 interrupt — ⚠️ **not** GPIO 1 (that is UART0 TX on the classic ESP32) |
| 34 | FD — NT3H2111 | Input only | — | Field Detection. GPIO 34 is **input-only with no internal pull-up** — an external 10 kΩ pull-up to 3.3 V is required for the open-drain FD signal |
| **Power & Control** |
| 15 | Power On | Output | - | Power control pin |
| **Relay Channels** |
| 12 | Relay 1 (CH01) | Output | - | Primary channel |
| 13 | Relay 2 (CH02) | Output | - | |
| 14 | Relay 3 (CH03) | Output | - | ⚠️ NOT GPIO 10 — internal flash on the WROOM-32! |
| 16 | Relay 4 (CH04) | Output | - | ⚠️ NOT GPIO 11 — internal flash on the WROOM-32! |
| 19 | Relay 5 (CH05) | Output | - | |
| 22 | Relay 6 (CH06) | Output | - | ⚠️ reserved when [Sensor 1](#vending-sensors) is active |
| 23 | Relay 7 (CH07) | Output | - | ⚠️ reserved when [Sensor 2](#vending-sensors) is active |
| 25 | Relay 8 (CH08) | Output | - | |
| 26 | Relay 9 (CH09) | Output | - | |
| 27 | Relay 10 (CH10) | Output | - | |
| 32 | Relay 11 (CH11) | Output | - | RTC-capable |
| 33 | Relay 12 (CH12) | Output | - | RTC-capable |

> ⚠️ **GPIO 6–11 are connected to the internal SPI flash on the ESP32-WROOM-32 and must NEVER be used as outputs.**

**Output type:** digital GPIO outputs (HIGH = relay activated) · **Max current per pin:** ~40 mA — use a relay driver IC (ULN2003/ULN2803) or MOSFET for real loads.

---

## Relay Channels (12)

> The headless version does **not** use Single/Duo/Quattro modes for channel selection. The active GPIO is determined directly by the switch configuration in LNbits — simply assign the desired GPIO pin to each switch in the LNbits extension.

| Channel | GPIO | Note |
|---------|------|------|
| CH01 | 12 | Default / primary (or servo, see [ZapBox Mode](#zapbox-mode-ch01)) |
| CH02 | 13 | |
| CH03 | 14 | ⚠️ GPIO 10 = internal flash on the WROOM-32! |
| CH04 | 16 | ⚠️ GPIO 11 = internal flash on the WROOM-32! |
| CH05 | 19 | |
| CH06 | 22 | ⚠️ reserved when Sensor 1 or Relay Output is active |
| CH07 | 23 | ⚠️ reserved when Sensor 2 or Relay Output is active |
| CH08 | 25 | |
| CH09 | 26 | |
| CH10 | 27 | |
| CH11 | 32 | RTC-capable |
| CH12 | 33 | RTC-capable |

---

## ZapBox Mode (CH01)

Pin 12 can operate in one of three modes:

- **Relay** *(default)* — standard digital relay output (HIGH/LOW)
- **180° Servo** — positional servo on Pin 12, sweeps between configurable start and end angles
- **360° Servo** — continuous rotation servo on Pin 12, runs at a configurable speed for a set duration

Servo parameters (angle start/end, speed, duration) are configured in the Web Installer. When servo mode is active, Pin 12 is reserved for the servo and skipped in the relay channel list.

---

## Vending Sensors

Two independent sensor inputs on **GPIO 22** and **GPIO 23**.

**Four operating modes per sensor:**

| Mode | Behaviour |
|------|-----------|
| **Stop the advance** (`yes`) | Stops the relay action when the sensor detects a product (LOW). Minimum 2 s action time before the sensor can trigger. |
| **Monitoring product blockage** (`monitor`) | Continuously monitors whether the product exit is blocked (LOW = blocked). Blocks further payments until cleared. |
| **Level monitoring** (`level`) | Monitors the supply bin fill level. Sensor HIGH (no product) = bin empty → payments blocked until restocked. |
| **Relay output** (`relay`) | Configures the GPIO as an **additional relay output** that switches in parallel with Pin 12 (CH01). Works with all ZapBox Modes and Special Modes — useful for extra relays, indicator lights or secondary actuators. |

**Payment blocking behaviour:** when a sensor condition blocks payments, the LED blinks **very fast (10 Hz)**, the WebSocket connection is disconnected (so the LNbits server rejects static QR payments), and NFC Bolt Card taps are blocked. Once cleared, the WebSocket auto-reconnects and normal operation resumes.

> ⚠️ When a sensor or relay-output function is active on GPIO 22 or 23, those GPIOs are **no longer available as relay channels** (CH06/CH07).

---

## LED Status Diagnostics

The headless version uses the status LED for all feedback. The device can be fully diagnosed by counting blinks.

### Network error priority

Error patterns follow a strict priority — the LED shows the **first unconfirmed network status**:

**1. WiFi → 2. Internet → 3. Server → 4. WebSocket**

If WiFi is down, the LED shows 1 blink even when other services are also unavailable. Once WiFi is restored, the LED moves on to the next error in the chain (if any).

### LED pattern reference

| Pattern | Timing | Status | Description |
|---------|--------|--------|-------------|
| **3 fast blinks** | 3× rapid flash on boot | **BOOT / NFC REJECTED** | Device powered on — also: Identity mode NFC card not recognised or rejected (tagid 404); returns to solid ON |
| **Fast continuous** | 200 ms on/off (5 Hz) | **INITIALIZING** | System startup, WiFi connecting |
| **Slow continuous** | 1000 ms on/off (1 Hz) | **CONFIG MODE** | Configuration interface active, waiting for settings |
| **Solid ON** | Continuous | **READY** | All systems operational, ready for payments |
| **Brief OFF (300 ms)** | 300 ms OFF, then solid again | **ACTION START** | Relay/servo fired — the onboard LED (GPIO 2) dips briefly to confirm |
| **Asymmetric blink** | 200 ms ON / 800 ms OFF | **NFC PENDING** | NFC payment initiated, waiting for invoice settlement |
| **2× fast blink** | 100 ms ON/OFF ×2, then solid | **NFC SUCCESS** | Payment confirmed — 2 quick flashes, relay fires, LED stays ON |
| **3× fast blink** | 100 ms ON/OFF ×3, then solid | **NFC NO LUCK** | 60 s timeout or HTTP error — returns to ready state |
| **Very fast continuous** | 50 ms on/off (10 Hz) | **SENSOR BLOCKING** | Vending sensor active — payments blocked, WebSocket disconnected |
| **Double-pulse** | 150 ms ON / 100 ms OFF / 150 ms ON / 1.5 s pause | **IDENTITY TEACH** | Teach mode active — waiting for a card or wallet to enrol |
| **6× rapid flash** | 50 ms ON/OFF ×6 (600 ms) | **IDENTITY ENROLLED** | NFC card or wallet successfully enrolled |
| **1 blink + pause** | 500 ms on/off, 2 s pause | **NO WIFI** | WiFi connection lost or failed to connect |
| **2 blinks + pause** | 300 ms each, 2 s pause | **NO INTERNET** | WiFi connected, but no internet access |
| **3 blinks + pause** | 250 ms each, 2 s pause | **NO SERVER** | Internet OK, LNbits server unreachable |
| **4 blinks + pause** | 200 ms each, 2 s pause | **NO WEBSOCKET** | Server reachable, WebSocket connection failed |
| **OFF** | No light | **INACTIVE** | Deep sleep or system halted |

### Wiring the status LED

```
Status LED             →    ESP32 Dev       →    GND
────────────────────────────────────────────────────
LED Anode (+)          →    GPIO 21
LED Cathode (-)        →    Resistor (220Ω) →    GND
```

GPIO 21 is RTC-capable and can therefore be used as a deep-sleep wake source. There is no button functionality on the headless variant — status LED only.

---

## Troubleshooting with LED Patterns

### 1 blink — NO WIFI
- Check the WiFi credentials in the configuration
- Verify the router is powered on and in range
- Check for MAC address filtering on the router
- Confirm the WiFi password

### 2 blinks — NO INTERNET
- The WiFi credentials are correct, but the router has no internet
- Check the router's internet connection (modem / ISP)
- Verify the router's WAN port is connected
- May simply be an ISP outage

### 3 blinks — NO SERVER
- Internet works, but the LNbits server is down or unreachable
- Check the LNbits server URL in the configuration
- Verify the LNbits server is running
- Check firewall rules (port 443 / HTTPS)
- Confirm DNS resolution of the server hostname

### 4 blinks — NO WEBSOCKET
- The server is reachable, but the WebSocket connection or data validation failed
- **Most common cause:** the switch instance was **deleted** on the LNbits server (HTTP 404)
  - Check whether the configured device ID still exists in LNbits
  - Verify the switch configuration in the LNbits admin panel
  - If the instance was deleted, create a new one and update the device configuration
- May also occur briefly during an LNbits service restart
- Should auto-recover once the server finishes starting up

### Recovery
After network issues are resolved, the LED returns to **solid ON** (READY) automatically. If it stays in an error state after confirmed repairs, press the reset button.

---

## Identity🫆Login

**NTAG 424 DNA (NFC tap) only** — LNURL-auth is not available, because there is no screen to show the login QR code.

The teach mode is started via a **6-digit PIN set in the Web Installer** (one-shot: the PIN is erased from flash as soon as teach mode starts). Teach status is signalled by the LED (double-pulse = teach active, 6× rapid flash = enrolled, 3× fast blink = card rejected).

**Configuration:** *Web Installer → ZapBox Mode → Identity🫆Login*. The triggered GPIO (relay / 180° servo / 360° servo) is Pin 12.

→ Mechanism, teach mode and security: **[docs/identity.md](identity.md)**

---

## Shared Features

These work the same way as on the other variants:

| Feature | Documentation |
|---------|---------------|
| NFC (Bolt Card, card emulation, NT3H2111 phone tap) | [docs/nfc.md](nfc.md) |
| Identity🫆Login | [docs/identity.md](identity.md) |
| BTC ticker, Special Modes, Threshold Mode | [docs/common-features.md](common-features.md) |
| I/O Expander (PCF8574, virtual pins 200–207) | [docs/common-features.md](common-features.md#io-expander--pcf8574) |
| Startup sequence & error diagnostics | [docs/common-features.md](common-features.md#startup--error-detection) |
