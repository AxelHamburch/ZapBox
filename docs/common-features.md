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
- [I/O Expander — PCF8575](#io-expander--pcf8575)
- [I/O Expander — MCP23017](#io-expander--mcp23017)
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

### Trigger level

The relay polarity is selectable in the web installer (**PCF8574 — Relay Trigger Level**):

| Setting | Behaviour | Idle state at boot |
|---------|-----------|--------------------|
| `Low-Level Trigger` *(default)* | LOW switches the relay **on** | all ports HIGH |
| `High-Level Trigger` *(not recommended)* | HIGH switches the relay **on** | all ports LOW |

> **💡 Relay flicker at power-up.** With a low-level trigger board the relays may click
> briefly when the device is switched on. Between the expander powering up and the firmware
> initialising it, the I²C lines can glitch and leave the ports LOW — which a low-level board
> reads as "on". This window exists before any firmware runs and cannot be closed in software.
> Wiring a **high-level trigger** board and selecting `High-Level Trigger` avoids it entirely,
> because LOW then means "off".

> **⚠️ High-Level Trigger is not recommended.** The PCF8574/PCF8575 ports are
> *quasi-bidirectional*: they sink around 25 mA towards GND, but in the HIGH state they only
> source about **100 µA** from a weak internal pull-up. That is far below what the opto-coupler
> input of a relay board draws (≈ 4 mA), so the level collapses as soon as the load is
> connected — measured on hardware: **0 V → 0.8 V** under load with High-Level, versus a solid
> **3.7 V → 0.3 V** with Low-Level. The relay then does not switch at all. The same weak pull-up
> also makes the LEDs on an 8-channel board glow faintly in the idle state.
>
> Use **Low-Level Trigger** and accept the brief power-up click, or add an external pull-up
> (4.7 k–10 k to the relay board's VCC) to suppress the glow. High-Level is only usable for
> loads that draw practically no current. If you want both trigger levels to work properly,
> use an [MCP23017](#io-expander--mcp23017) instead — it has push-pull outputs.

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

## I/O Expander — PCF8575

Adds **16 additional relay channels** (virtual pins 300–315) over the same I²C bus. Supported on the **Touch 3.5"** (JC3248W535C). Can run alongside a PCF8574 and an MCP23017, giving 8 + 16 + 16 = 40 expander channels in total.

### Virtual pin mapping

| Virtual Pin | Port | | Virtual Pin | Port |
|-------------|------|---|-------------|------|
| 300 | P00 | | 308 | P10 |
| 301 | P01 | | 309 | P11 |
| 302 | P02 | | 310 | P12 |
| 303 | P03 | | 311 | P13 |
| 304 | P04 | | 312 | P14 |
| 305 | P05 | | 313 | P15 |
| 306 | P06 | | 314 | P16 |
| 307 | P07 | | 315 | P17 |

### I²C address — mind the notation

> **⚠️ PCF8574 and PCF8575 share the same `0x20`–`0x27` address range.** With A0/A1/A2 tied
> to GND both chips answer on `0x20`, so a PCF8575 must be moved off that address whenever a
> PCF8574 is also in use.

ZapBox expects the PCF8575 on **`0x21`** — that is **A0 to VDD**, A1/A2 to GND.

Many datasheets and wikis list these addresses in **8-bit notation including the R/W bit**
(`0x40`, `0x42`, `0x44` … in steps of two). Arduino and `Wire` use the **7-bit** form, which
is simply half of that value:

| 8-bit (datasheet) | 7-bit (`Wire`) | A2 | A1 | A0 | |
|---|---|---|---|---|---|
| 0x40 | `0x20` | LOW | LOW | LOW | ⚠️ used by PCF8574 |
| **0x42** | **`0x21`** | LOW | LOW | **HIGH** | ✅ ZapBox default |
| 0x44 | `0x22` | LOW | HIGH | LOW | ⚠️ used by MCP23017 |
| 0x46 | `0x23` | LOW | HIGH | HIGH | free |
| 0x48 | `0x24` | HIGH | LOW | LOW | ⚠️ used by PN532 |
| 0x4A–0x4E | `0x25`–`0x27` | | | | free |

If the address pins are left floating, the chip answers on a **random address that changes
between scans** — a reliable symptom of unbridged A0/A1/A2.

### Wiring

```
PCF8575         →    ZapBox              →    Relay Module
────────────────────────────────────────────────────────────
VCC             →    3.3V                     (not 5 V!)
GND             →    GND
SDA             →    GPIO 18 (shared I²C)
SCL             →    GPIO 17 (shared I²C)
A0              →    VDD                      (address = 0x21)
A1, A2          →    GND
P00 … P17       →                        →    IN1 … IN16
INT             →    not connected            (inputs not implemented yet)
```

### Trigger level

Same options as the PCF8574 — selectable in the installer as **PCF8575 — Relay Trigger Level**,
with the same power-up flicker consideration described above. The **High-Level Trigger is not recommended** here either — the PCF8575 pull-up is even weaker than the PCF8574 one (measured idle level 2.25 V versus 2.56 V with the same relay board), so the LED glow is more pronounced.

### Selecting these channels

CH300–CH315 are reachable via the **numerical product selection** (keypad), a Bolt Card tap on
the displayed product QR, or a direct payment/trigger from LNbits. The classic product browsing
navigation only covers products 1–12 and does not reach them.

### Inputs (not implemented)

The PCF8575 can also read inputs. This is not implemented yet. When it is, it should use the
chip's **`INT` line** wired to a free ESP32 GPIO: the PCF8575 pulls `INT` low whenever an input
level changes, so the firmware can react to an interrupt instead of polling the chip
continuously over I²C.

---

## I/O Expander — MCP23017

Adds **16 additional relay channels** (virtual pins 400–415) over the same I²C bus. Supported on
the **Touch 3.5"** (JC3248W535C). Can run alongside a PCF8574 *and* a PCF8575, giving
8 + 16 + 16 = 40 expander channels in total.

> **💡 This is the electrically better expander.** Unlike the PCF857x, the MCP23017 has true
> **push-pull** outputs that sink *and* source 25 mA. Both trigger levels therefore work under
> load, the idle LEDs on a relay board do not glow, and there is no power-up relay flicker —
> after reset every port is a high-impedance input until the firmware configures it. See the
> trigger-level warnings in the PCF8574/PCF8575 sections for what this fixes.

### Virtual pin mapping

| Virtual Pin | Port | | Virtual Pin | Port |
|-------------|------|---|-------------|------|
| 400 | PA0 | | 408 | PB0 |
| 401 | PA1 | | 409 | PB1 |
| 402 | PA2 | | 410 | PB2 |
| 403 | PA3 | | 411 | PB3 |
| 404 | PA4 | | 412 | PB4 |
| 405 | PA5 | | 413 | PB5 |
| 406 | PA6 | | 414 | PB6 |
| 407 | PA7 | | 415 | PB7 |

Port names are the ones silkscreened on the breakout board; the datasheet calls the same pins
`GPA0`–`GPA7` and `GPB0`–`GPB7`.

> **Note on the byte order.** The MCP23017 library writes the **high** byte of its 16-bit API to
> port A, which would put channel CH400 on `PB0`. The driver calls `reverse16ByteOrder(true)` to
> get the mapping above. If you ever swap the library out, this is the first thing to re-check.

### I²C address

The firmware expects the MCP23017 on **`0x22`** (7-bit) = **`0x44`** in the 8-bit notation used
by many datasheets — **A1 to VCC, A0 and A2 to GND**. On the usual breakout boards these are
solder jumpers, not header pins — see [Setting the address on the breakout
board](#setting-the-address-on-the-breakout-board).

> **⚠️ The default address collides.** With A0/A1/A2 all grounded the MCP23017 answers on
> `0x20`, which is the PCF8574's address. All three chip families share the `0x20`–`0x27`
> range, and the **PN532 NFC reader occupies `0x24`** on the same bus. The address pins must
> therefore be wired deliberately.

Occupied addresses on the shared bus:

| Address | Device |
|---------|--------|
| `0x15` | CST816S touch controller |
| `0x20` | PCF8574 |
| `0x21` | PCF8575 |
| `0x22` | **MCP23017** |
| `0x23` | free |
| `0x24` | PN532 NFC reader |
| `0x25`–`0x27` | free |
| `0x55` | NT3H2111 |

`initIOExpanderMCP()` refuses to bring the chip up if its address collides with an active
PCF8574, an active PCF8575, or the PN532, rather than producing random relay behaviour.

> **⚠️ Never leave an address input open.** A floating pin makes the chip answer on a
> **different address on every bus scan** — the same symptom the PCF8575 shows. Also check
> `RST`: it is **active LOW** and must sit HIGH, otherwise the chip stays in reset and never
> appears on the bus.

### Setting the address on the breakout board

The common green MCP23017 breakouts do not bring A0/A1/A2 out on the header — they carry three
**solder jumpers** on the board, next to a `GND` / `VCC` label:

```
        GND   VCC
   A2  [ ■ ]  [   ]
   A1  [ ■ ]  [   ]     ← move this link to VCC for 0x22
   A0  [ ■ ]  [   ]
```

In the factory state all three links sit on the `GND` side, which is address `0x20` — the
PCF8574's address. For ZapBox, move **A1** to the `VCC` side: desolder the 0 Ω link (or clear
the pad with solder wick) and bridge the VCC pad instead. A0 and A2 stay on GND.

The relay ports are silkscreened as `PA0`–`PA7` and `PB0`–`PB7` (see the virtual pin mapping
above for which channel is which).

### Wiring

```
MCP23017        →    ZapBox              →    Relay Module
────────────────────────────────────────────────────────────
VCC             →    3.3V
GND             →    GND
SDA             →    GPIO 18 (shared I²C)
SCL             →    GPIO 17 (shared I²C)
A1 jumper       →    VCC side                 (address = 0x22)
A0, A2 jumpers  →    GND side (factory state)
RST             →    must sit HIGH            (active LOW — must not float)
PA0 … PB7       →                        →    IN1 … IN16
INTA, INTB      →    not connected            (inputs not implemented yet)
```

### Trigger level

Selectable in the installer as **MCP23017 — Relay Trigger Level**, same two options as the PCF
expanders:

| Setting | Behaviour | Idle state after init |
|---------|-----------|-----------------------|
| `Low-Level Trigger` *(default)* | LOW switches the relay **on** | all ports HIGH |
| `High-Level Trigger` | HIGH switches the relay **on** | all ports LOW |

Here **both settings are electrically fine**, because the outputs are push-pull. The
initialisation writes the idle value into the output latch *before* switching the ports from
input to output, so an active-HIGH relay board never sees a start-up pulse.

### Selecting these channels

CH400–CH415 are reachable via the **numerical product selection** (keypad), a Bolt Card tap on
the displayed product QR, or a direct payment/trigger from LNbits. The classic product browsing
navigation only covers products 1–12 and does not reach them.

### Inputs (not implemented)

The MCP23017 can also read inputs, with per-pin interrupt-on-change on the `INTA`/`INTB` lines.
Not implemented yet.

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
