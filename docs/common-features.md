# Common Features

Features that work the same way on every ZapBox variant. Anything board-specific — GPIO numbers, channel counts, button behaviour — lives on the variant page:

[T-Display-S3](t-display-s3.md) · [Touch 3.5"](touch-3.5.md) · [Headless ESP32](headless-esp32.md) · [ESP32-C3](esp32-c3.md)

---

## Table of Contents

- [Basic Configuration](#basic-configuration)
- [Special Modes](#special-modes)
- [BTC Ticker](#btc-ticker)
- [Threshold Mode](#threshold-mode)
- [I/O Expander — PCF8574](#io-expander--pcf8574)
- [External LED Button](#external-led-button)
- [Screensaver & Deep Sleep](#screensaver--deep-sleep)
- [Startup & Error Detection](#startup--error-detection)

---

## Basic Configuration

Everything is configured through the [Web Installer](https://installer.zapbox.space/) over a browser-based serial connection (Chrome or Edge required):

- WiFi SSID and password
- LNbits server URL (both `ws://` and `wss://` are supported)
- LNURL for payments
- Display orientation (horizontal / vertical) — *display variants*
- Display theme — several colour combinations — *display variants*

**Entering config mode:** hold the BOOT button for 5 seconds (or use the button/touch gesture documented on your variant page).

---

## Special Modes

Control the relay switching pattern beyond simple on/off. **Applies to CH01 only** — additional channels always use standard on/off.

| Mode | Pattern |
|------|---------|
| **Standard** *(default)* | Simple on/off |
| **Blink** | 1 Hz, 1:1 duty cycle |
| **Pulse** | 2 Hz, 1:4 duty cycle (short pulses) |
| **Strobe** | 5 Hz, 1:1 duty cycle (fast blinking) |
| **Custom** | Your own frequency (0.1–10 Hz) and duty cycle ratio (0.1–10) |

**Use cases:** LED effects, motor speed control, warning signals.

> Special Mode is automatically **bypassed in Servo mode** — pulsing the relay would interfere with servo timing.

---

## BTC Ticker

Live Bitcoin price and block height on the display. *Display variants only.*

**Three display modes:**

| Mode | Behaviour |
|------|-----------|
| **OFF** | No ticker. Multi-channel shows the product selection screen; single mode shows only the QR code |
| **ON — always** | The ticker is the main screen. A touch shows the QR for ~20 s, then returns to the ticker |
| **ON — when selecting** | The ticker appears only on demand — touch shows it for ~10 s, then back to the QR |

**Configuration:** mode (`off` / `always` / `selecting`) and currency — any 3-letter ISO code (`USD`, `EUR`, `GBP`, `JPY`, `CHF`, …).

**Data sources:** price from CoinGecko (50+ currencies), block height from mempool.space. The ticker refreshes automatically after WiFi/internet recovery.

---

## Threshold Mode

Monitor a wallet balance and trigger the relay only when a **threshold** is reached. Payments accumulate in the wallet until the goal is met.

**Configuration:**
- Wallet invoice/read key
- Threshold amount in satoshi
- GPIO pin and switching duration
- A static LNURL or Lightning Address for payments

**Use cases:** crowdfunding triggers, donation goals, pay-per-use with an accumulated balance.

---

## I/O Expander — PCF8574

Adds **8 additional relay/sensor channels** (virtual pins 200–207) over the existing I²C bus. Supported on the T-Display-S3 and the headless ESP32 Dev.

This is the recommended route for the **T-Display-S3**, which has no free GPIOs left. The PCF8574 sits on the same I²C bus as the PN532 — no additional wiring to the ESP32 is needed.

### Virtual pin mapping

| Virtual Pin (LNbits) | PCF8574 Port | Config Key | Default |
|----------------------|--------------|------------|---------|
| 200 | P0 | ch05 | off |
| 201 | P1 | ch06 | off |
| 202 | P2 | ch07 | off |
| 203 | P3 | ch08 | off |
| 204 | P4 | ch09 | off |
| 205 | P5 | ch10 | off |
| 206 | P6 | ch11 | off |
| 207 | P7 | ch12 | off |

### Wiring

```
PCF8574         →    ZapBox              →    Relay Module
────────────────────────────────────────────────────────────
VCC             →    3.3V
GND             →    GND
SDA             →    GPIO 18 (shared I²C)
SCL             →    GPIO 17 (shared I²C)
A0, A1, A2      →    GND                      (address = 0x20)
P0 … P7         →                        →    IN1 … IN8
```

> **⚠️ The PCF8574 outputs are active-LOW.** Use relay modules that trigger on LOW (most opto-coupled relay boards do). On startup all pins are set HIGH — all relays off.

### Channel modes

| Mode | Behaviour | Screen |
|------|-----------|--------|
| `off` | Channel disabled (pin stays HIGH) | — |
| `relay` | Standard relay output — active for the payment duration | — |
| `sensor-stop` | Stops an active relay action immediately when triggered (LOW) | — |
| `sensor-monitor` | Blocks the next payment while the sensor is LOW | "PRODUCT BLOCKED" |
| `sensor-level` | Inverted logic: LOW = supply OK, HIGH = bin empty → blocks payments | "SUPPLY BIN EMPTY" |

**I²C address:** `0x20` (A0/A1/A2 tied to GND). The expander is auto-detected at startup; if none is found, the feature is silently disabled.

---

## External LED Button

An optional illuminated push button. *Display variants only* (the headless variants have a status LED but no button).

**GPIO 43** drives the LED (sources 3.3 V when the device is ready) and **GPIO 44** reads the button (pull-up, active LOW on press).

### Wiring

```
External LED Button    →    ZapBox      →    GND
──────────────────────────────────────────────────
LED Anode (+)          →    GPIO 43
LED Cathode (-)        →                →    GND
Button Terminal 1      →    GPIO 44
Button Terminal 2      →                →    GND
```

### Button functions

| Action | Result |
|--------|--------|
| **Single press** | Wake from screensaver / next product or QR screen |
| **Hold ≥ 2 s** | Open the Help page |
| **Triple-click** (within 2 s) | Open the Report page |
| **Double-click, hold the 2nd press ≥ 3 s** | Enter Config mode |
| **In Config mode** (after a 2 s guard time) | Press again to exit and restart |

50 ms hardware debounce. Works in all operation modes.

> **Note:** GPIO 43/44 are **not RTC-capable**. The LED button can wake the device from **Light Sleep**, but **not** from **Deep Sleep (Freeze)**.

---

## Screensaver & Deep Sleep

Automatic power-saving modes that activate after a configurable timeout.

| Mode | Backlight | CPU | Payments | Power | Savings | Wake-up |
|------|-----------|-----|----------|-------|---------|---------|
| **Normal** | ON | Running | ✅ Yes | ~150–250 mA | 0 % | — |
| **Screensaver** | OFF | Running | ✅ Yes | ~40–60 mA | ~80–90 % | Instant |
| **Light Sleep** | OFF | Paused | ❌ No | ~0.8–2 mA | ~99 % | ~3–5 s |
| **Deep Sleep (Freeze)** | OFF | OFF | ❌ No | ~0.01–0.15 mA | ~99.9 % | ~3–5 s |

### Which mode should I use?

**Screensaver (backlight off)** — ⭐ **best for payment terminals**
Instant wake-up, 80–90 % saving, and **payments still work** while it is active. The backlight is what consumes most of the display power, so switching it off gets almost all of the benefit while the device stays fully online.
*Battery: ~7–10 days with a 10 000 mAh pack.*

**Light Sleep** — ⭐ **best for devices with an external LED button**
99 % saving, and **any** button can wake the device, including the LED button (GPIO 44). WiFi reconnects on wake (~3–5 s). No payments during sleep.
*Battery: months with a 10 000 mAh pack.*

**Deep Sleep (Freeze)** — ⭐ **best for long-term installations**
Maximum saving. Only RTC-capable GPIOs can wake the device, so the **LED button cannot wake it** (GPIO 44 is not RTC-capable). No payments during sleep.
*Battery: years with a 10 000 mAh pack.*

### Wake-up sources

| Mode | Wakes on |
|------|----------|
| **Screensaver** | Touch, any button — instant |
| **Light Sleep** | BOOT, HELP or LED button — device restarts and reconnects |
| **Deep Sleep** | BOOT or HELP button only (RTC-capable pins) |

> On the **Touch 3.5"**, touch cannot wake the device from deep sleep at all — the touch controller's INT pin is not routed to a GPIO on that module. The BOOT button (GPIO 0) is the wake source.

### Configuration

- **Screensaver:** `OFF` *(default)* · `Backlight Off` *(recommended)*
- **Deep Sleep:** `OFF` *(default)* · `Freeze` · `Light Sleep`
- **Timers:** separate timeouts for screensaver (default 5 min) and deep sleep (default 30 min), 1–120 minutes each
- **Combined:** both can be used together — the screensaver kicks in first (payments still work), deep sleep follows later

---

## Startup & Error Detection

### Startup sequence

**Phase 1 — startup screen (~5 s):** branding and firmware version; WiFi connection starts in the background.

**Phase 2 — initialization (up to 20 s):** all connection tests run in parallel — WiFi → Internet → LNbits server → WebSocket. The device switches to the QR code **as soon as all connections succeed**.

**Typical:** ~10–15 seconds from power-on to QR code. **Worst case:** 25 seconds, then the first detected error is displayed.

### Error priority

Errors are detected hierarchically — each level is only checked when all higher levels are OK.

| Priority | Error | Abbr. | Detection | Likely cause |
|----------|-------|-------|-----------|--------------|
| 1 (highest) | **NO WIFI** | NW | WiFi connection status | Wrong credentials? Signal too weak? |
| 2 | **NO INTERNET** | NI | HTTP check | Router has no internet? |
| 3 | **NO SERVER** | NS | TCP port 443 check | LNbits server down? Device string wrong? |
| 4 (lowest) | **NO WEBSOCKET** | NWS | WebSocket handshake | LNbits down? Switch instance deleted? |

A higher-priority error always overrides the display of a lower one: if WiFi is down, the device shows "NO WIFI" and skips the other checks entirely.

**Monitoring intervals:** internet check every 30 s (HTTP GET to `clients3.google.com/generate_204`) · WiFi/server/WebSocket every 5 s · WebSocket ping every 60 s.

**Smart recovery:** when WiFi reconnects, internet and server status are re-checked immediately, and the device returns to the correct screen for its actual state.

### Report mode

Shows error counters (0–99) for all four error types with their occurrence counts.

- **Press the HELP button twice** in quick succession, or
- **Press the LED button three times** in quick succession (if an external LED button is fitted)

On the headless variants, the same information is expressed through [LED blink patterns](headless-esp32.md#led-status-diagnostics).

### A common wallet error

If a wallet scanning the QR code reports *"bitcoinswitch … is disabled"*, then either the switch was disabled in LNbits, or the handshake between wallet and ZapBox failed.
