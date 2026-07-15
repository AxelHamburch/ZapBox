# ZapBox Touch 3.5" (JC3248W535C)

ESP32-S3 with a 3.5" capacitive touch display (480 × 320). The largest ZapBox — built for kiosk terminals, market stalls and vending installations where the customer interacts with the screen.

**Firmware suffix:** `t` (e.g. `v957859t`) · **Web Installer:** [installer.zapbox.space/touch3.5/](https://installer.zapbox.space/touch3.5/)

---

## Table of Contents

- [Hardware](#hardware)
- [GPIO Mapping](#gpio-mapping)
- [Channels (CH01–CH06)](#channels-ch01ch06)
- [Battery Gauge](#battery-gauge)
- [Vending Sensors](#vending-sensors)
- [Mini-PoS Mode](#mini-pos-mode)
- [Multi-Channel Mode](#multi-channel-mode)
- [Numerical Product Selection](#numerical-product-selection)
- [Identity🫆Login](#identitylogin)
- [Operation](#operation)
- [Shared Features](#shared-features)

---

## Hardware

| | |
|---|---|
| **Microcontroller** | ESP32-S3-WROOM-1, dual-core Xtensa LX7 @ 240 MHz |
| **Display** | 3.5" IPS, 480 × 320, AXS15231B driver, QSPI interface |
| **Touch** | AXS15231B capacitive multi-touch (internal I²C, SDA=4 / SCL=8) |
| **Memory** | 16 MB Flash, 8 MB OPI PSRAM |
| **Battery** | JST 1.25 2P connector, on-board charger + protection, physical battery switch |
| **Power draw** | ~200–350 mA |
| **Channels** | 6 flex channels (relay / servo / sensor / ambient light) |

**I²C addresses:** PN532 `0x24` · NT3H2111 `0x55` · PCF8574 `0x20`
(The AXS15231B touch sits on its own internal bus and does not share the external one.)

---

## GPIO Mapping

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **Display (QSPI, internal — not on the breakout header)** |
| 45 | LCD QSPI CS | Output | - | Display chip select |
| 47 | LCD QSPI CLK | Output | - | Display clock |
| 21 | LCD QSPI D0 | Output | - | Display data 0 |
| 48 | LCD QSPI D1 | Output | - | Display data 1 |
| 40 | LCD QSPI D2 | Output | - | Display data 2 |
| 39 | LCD QSPI D3 | Output | - | Display data 3 |
| 1 | LCD Backlight | Output | HIGH=ON | Display brightness (GPIO 21 is QSPI data, *not* backlight) |
| **Touch (AXS15231B, internal)** |
| 4 | I²C SDA | I²C | - | Internal bus — no external GPIO required |
| 8 | I²C SCL | I²C | - | Internal bus — no external GPIO required |
| **I²C Bus (external)** |
| 17 | I²C SCL | I²C | - | Shared: PN532 + NT3H2111 + PCF8574 |
| 18 | I²C SDA | I²C | - | Shared: PN532 + NT3H2111 + PCF8574 |
| 9 | NFC IRQ | Input | Pull-up | PN532 interrupt (card detection, active LOW) |
| **Flex Channels** |
| 14 | CH01 | Output | - | Primary channel — relay *(default)* / servo. Special Mode applies here only |
| 15 | CH02 | Output | - | Relay / servo / ambient light |
| 16 | CH03 | Output | - | Relay / servo / ambient light |
| 5 | CH04 | Output / Input / analog | Pull-up (sensor) | Relay / servo / **sensor** / ambient light — 🔋 **while off, this pin measures the battery** |
| 6 | CH05 | Output *or* Input | Pull-up (sensor) | Relay / servo / **sensor** / ambient light |
| 7 | CH06 | Output *or* Input | Pull-up (sensor) | Relay / servo / **sensor** / ambient light |
| **LED Button** |
| 43 | LED Button (LED) / **TX** | Output | HIGH=ON | Board pin labeled **TX** — illuminated button LED (3.3 V) |
| 44 | LED Button (SW) / **RX** | Input | Pull-up | Board pin labeled **RX** — button switch (active LOW); Light-Sleep wake source |
| **NFC Tag 2** |
| 46 | FD — NT3H2111 | Input | INPUT_PULLUP | ℹ️ Strapping pin — Field Detection, see note below |

> **ℹ️ GPIO 46 — Field Detection, a strapping pin with low practical risk**
>
> GPIO 46 is permanently configured as `INPUT_PULLUP` and used exclusively as the FD input from the NT3H2111. No Web Installer configuration required.
>
> On the ESP32-S3, GPIO 46 controls **ROM serial output during boot**:
> - HIGH at boot (default) → ROM log printed to UART
> - LOW at boot → ROM log suppressed (boot proceeds normally — no download-mode risk, no bricking)
>
> This has **no effect on normal firmware operation**. A 10 kΩ series resistor between the NT3H2111 FD pin and GPIO 46 is sufficient protection. The only real consequence of a LOW at boot is that ROM boot messages are silenced on the serial monitor.

**Reserved — not available as channels:** 17/18 (I²C), 9 (PN532 IRQ), 46 (NT3H2111 FD).

---

## Channels (CH01–CH06)

Six freely configurable channels. Each channel's function is set independently in the Web Installer, so one device can mix dispensers, relays and ambient lighting.

| Channel | GPIO | Selectable functions |
|---------|------|----------------------|
| CH01 | 14 | Relay *(default)* · Servo 180° · Servo 360° — primary / Special-Mode channel |
| CH02–CH03 | 15, 16 | Off · Relay · Servo 180°/360° · Ambient Light |
| CH04 | 5 | Off · Relay · Servo 180°/360° · **Sensor** (stop / blockage / level) · Ambient Light — 🔋 but **while off, this pin measures the [battery](#battery-gauge)** |
| CH05 | 6 | Off · Relay · Servo 180°/360° · **Sensor** (stop / blockage / level) · Ambient Light |
| CH06 | 7 | Off · Relay · Servo 180°/360° · **Sensor** (stop / blockage / level) · Ambient Light |

**Why the primary is GPIO 14:** GPIO 14/15/16 sit on ADC2, which the WiFi driver claims — they can never serve as analog inputs on this device, only as digital outputs or PWM (servo), so they lead as CH01–CH03. The three **ADC1-capable** pins (5, 6, 7) follow as CH04–CH06; all three are **sensor-capable**, and GPIO 5 (CH04) additionally carries the battery divider.

- **Payment channels:** every channel set to *relay* or *servo* becomes its own payment channel with a unique LNURL/QR code and its own amount and duration from the LNbits switch entry. Channels set to *ambient-light*, *sensor* or *off* are not counted as payment channels.
- **CH01** is always active as the primary channel and is the only one that supports the **Special Modes** (blink / pulse / strobe); additional channels switch in standard on/off mode.

### Per-channel servo parameters

Any channel set to **Servo 180°** or **Servo 360°** gets its own parameter box directly below that channel in the Web Installer, so every dispenser or barrier is tuned independently:

| Servo type | Parameters | Behaviour |
|------------|------------|-----------|
| **180°** (positional) | Start angle (°) · End angle (°) · Sweep duration (ms) | Sweeps Start→End on payment, holds for the action time, then returns to Start. `0 ms` = native (max) speed. |
| **360°** (continuous) | Speed (0–180) · Spin duration (ms) | Spins at the set speed (`90` = stop, `<90` = CCW, `>90` = CW). `0 ms` = spins until the action time ends. |

### Activation Options (One for All)

- **Off** *(default)* — each channel is triggered separately by its own QR/payment.
- **One for All** — a single payment on **CH01** fires *all* relay/servo channels at once (one QR code). Each channel runs for its own LNbits-configured duration (fallback: CH01's duration). Ambient-light and sensor channels are unaffected. Mutually exclusive with Numerical Product Selection.

> ⚠️ **Breaking change.** The channel numbering was re-ordered so the primary channel (CH01) is now **GPIO 14** and CH01–CH06 map to GPIO 14/15/16/5/6/7. Each physical function stayed on its GPIO — only the CHxx label moved (e.g. the battery/sensor pin GPIO 5 is now CH04, GPIO 7 is now CH06). The wiring does not change, but devices flashed with an older firmware must update the pin numbers in their LNbits switch entries to match the channel they use (e.g. the primary relay is now pin `14`, not pin `6`). A firmware-only update leaves the device silent — payments arrive, but nothing switches.

---

## Battery Gauge

The JC3248W535C has a **JST connector for a single-cell LiPo** with an on-board charging circuit. The battery rail is wired to **GPIO 5** through a divider (33 kΩ / 100 kΩ, per the vendor schematic), so the pack voltage can be read with the ADC. GPIO 5 is **ADC1_CH4** — and only ADC1 works while WiFi is active, since ADC2 (GPIO 14/15/16) is claimed by the WiFi driver.

The charge level (0–100 %) is shown in the **[Mini-PoS entry screen](#mini-pos-mode)**, in both orientations: top-right of the screen in portrait, and top-right of the left panel (next to the numpad divider) in landscape.

### GPIO 5 (CH04) has two mutually exclusive roles

| GPIO 5 used as | Consequence |
|----------------|-------------|
| **Battery ADC** (CH04 = `off`, the default) | Battery charge level is measured and displayed. |
| **Flex channel CH04** (relay / servo / sensor / ambient) | The channel works as configured; the battery display disappears, because a driven output overrides the high-impedance divider. |

No configuration switch is needed — the firmware derives this from the CH04 mode.

### Calibration

The reading is *not* the textbook divider ratio. The divider has a ~25 kΩ source impedance and the module has **no bypass capacitor** at the pin, so the ADC's sample-and-hold pulls the node down while measuring. The resulting error is linear and stable, so it is calibrated out in software (`Battery.cpp`) rather than fixed in hardware:

```
V_BAT[mV] = 1.533 × ADC[mV] + 356
```

Fitted over two full discharge runs (3000 mAh and 1000 mAh cells) across the whole usable range, 3.47–4.13 V; every point lands within ±40 mV.

The percentage curve maps the voltage **under load** (display + WiFi, ~250–300 mA) — that is the only condition in which the device ever measures itself, and a loaded cell sits well below its resting voltage. 4.0 V under load is a nearly full cell. The board browns out just under 3.2 V, which is where the curve reaches 0 %.

### No battery / USB

Whether USB is attached **cannot** be detected: the divider hangs on the charger's BAT node, so with a cell connected the pin shows the cell voltage either way.

A *railed* reading (~3107 mV) does mean something though — with no cell to hold the node down, the charger pushes it past the ADC's full scale. That is used to detect a **missing battery** (or a battery switch left off), and the gauge then shows nothing rather than inventing a number.

> **Debugging note:** probing GPIO 5 with a multimeter loads the high-impedance node and makes the displayed percentage sag for as long as the probe is attached; it creeps back afterwards. The measurement disturbs the measurement — that is expected, not a fault.

---

## Vending Sensors

Available on **CH04 (GPIO 5)**, **CH05 (GPIO 6)** and **CH06 (GPIO 7)** — the three ADC1-capable pins. Same three modes as the T-Display-S3 light barrier. All are digital inputs, active LOW (`INPUT_PULLUP`).

| Mode | Behaviour |
|------|-----------|
| **Stop the advance** | Ends the running relay/servo action as soon as the sensor triggers (earliest 2 s after the action started). |
| **Monitoring product blockage** | After a payment, a still-blocked outlet shows *PRODUCT BLOCKED* and holds further payments until the path is clear. |
| **Level monitoring** | An empty supply bin shows *SUPPLY BIN IS EMPTY* and blocks payments until it is restocked. |

Sensors are digital inputs — electrically any GPIO would do. CH04/CH05/CH06 are offered because they are the three ADC1-capable pins, which keeps an *analog* sensor possible later without moving the connector. Up to three sensors can run at once.

> ⚠️ A sensor on **CH04** claims GPIO 5 as a digital input and therefore **disables the [battery gauge](#battery-gauge)**.

---

## Mini-PoS Mode

**Requires the [zapbox_extension](https://github.com/AxelHamburch/zapbox_extension) v2.3.0+ on the LNbits server.**

Turns the ZapBox into a small point-of-sale terminal: instead of a fixed QR code, the customer-facing display shows an **amount entry screen**. The operator types an amount, presses **INVOICE**, and the ZapBox requests a Lightning invoice from the LNbits server and shows it as a QR code. After payment, **CH01 (GPIO 14)** switches — like in single-channel mode.

### Payment flow

1. Enter the amount on the touch numpad (e.g. `5` → automatically normalized to `5.00 EUR`)
2. Press **INVOICE** — the ZapBox requests a BOLT11 invoice via the zapbox_extension
3. The invoice is shown as QR code; lines 1+2 of the label come from the LNbits switch entry for CH01, line 3 shows the amount
4. The customer pays by:
   - **scanning the QR code** with any Lightning wallet
   - **tapping their phone** on the NFC Tag 2 (NT3H2111) — the tag carries the invoice
   - **tapping a Bolt Card** on the PN532 reader — the card pays the pending invoice (PIN protection supported)
5. On settlement the server pushes the trigger over WebSocket: CH01 switches with the duration configured for pin 14 in LNbits (fallback: 3000 ms), the display shows **PAID** for 3 seconds, then returns to the empty entry screen

### Entry screen

- Numpad with decimal point (the `.` key is disabled when *Decimal separator: NO* is configured)
- Amounts up to 7 characters (`xxxx.xx` or `xxxxxxx`)
- 🔋 **Battery percentage** top-right (only when a battery is connected and CH04 is off)
- **INVOICE** — request the invoice for the entered amount
- **LAST PAY** — recalls the amount of the last settled payment; shown orange/locked for 5 seconds, then stays in the field and can be confirmed or edited
- **CANCEL** (bottom-right on the QR screen) — aborts the pending invoice; a touch anywhere else does nothing
- Unpaid invoices expire after `INVOICE_TIMEOUT` (default 3 minutes, build flag) and the device returns to the entry screen

### BTC ticker as screensaver

With *BTC-Ticker Mode: ON - always*, the ticker acts as a screensaver: a single touch on the ticker opens the amount entry screen; after `PRODUCT_TIMEOUT` (default 30 s) of inactivity on the entry screen the device returns to the ticker. With ticker mode *OFF* or *when selecting*, the entry screen is shown permanently.

### NFC Tag idle content

While no invoice is pending, the NFC Tag 2 carries `https://zapbox.space` — a curious phone tap on the idle device opens the project website. As soon as an invoice is created, the tag carries the invoice; after payment/cancel/timeout it returns to the URL.

### Configuration (Web Installer → ZapBox Mode → Mini-PoS)

| Field | Description |
|-------|-------------|
| Currency (ISO Code) for Payments | e.g. `EUR`, `USD`, `GBP` — or `Sat` for satoshi (default: `EUR`) |
| Decimal separator | `YES` = two decimal places (e.g. `0.50 EUR`), `NO` = whole numbers (e.g. `1000 Sat`) |
| Wallet Invoice Key | the LNbits wallet "Invoice/read key" (32 characters) of the wallet that receives the payments |

The *Device settings string* must be configured as usual — it identifies the LNbits server and the ZapBox device entry (used for label fetch, WebSocket push and Bolt Card payments).

**Activation Options** are available here too: with **One for All**, a paid invoice additionally triggers the channels CH02–CH06 that are configured as relay/servo (configure the channel functions in Multi-channel mode first — the values are kept when switching back to Mini-PoS).

**Use cases:** market stalls, flea markets, tip boxes, club bars, small shops — anywhere a fixed-price switch is not enough and a full PoS is too much.

---

## Multi-Channel Mode

Up to 6 independent payment channels, each with its own QR code, amount and duration from LNbits. See [Channels](#channels-ch01ch06) for the pin map and per-channel options.

Customers pick a product by **swiping** the selection screen, or by typing its number with [Numerical Product Selection](#numerical-product-selection).

**Configuration (Web Installer → ZapBox Mode → Multi-channel):** set each channel's function in the CH01–CH06 dropdowns; the servo parameter box appears automatically when a channel is set to Servo 180°/360°. *Activation Options* and *Numerical Product Selection* live in the same panel.

---

## Numerical Product Selection

**Available in Multi-channel mode.**

Instead of swiping through the products one by one, the customer selects a product by typing its **GPIO number** on a touch keypad and gets the matching QR code. The fixed binding to the GPIO number guarantees a unique assignment and maximum flexibility — every relay/servo channel that is configured as a switch in the LNbits extension is directly addressable, including the **I/O expander virtual pins 200–207**.

**Flow:**

1. Main screen is the *Select your product* screen (or the BTC ticker with *BTC-Ticker Mode: ON - always*) — any touch opens the **product selection panel**
2. Type the GPIO number of the product (e.g. `6`, `7`, `14` or `200`); `<` deletes the last digit, the small **CANCEL** button returns to the main screen
3. Press the green **OK** key — the ZapBox validates the number:
   - the pin must be configured as a **relay/servo channel** on the device (CH01–CH06 or I/O expander enabled), **and**
   - LNbits must have a switch entry for this pin (fetched from the server)
   - unknown numbers show **"Product not available"**
4. The product QR code is shown with the LNbits label; the content is also served via NFC: the **NFC Tag 2** carries the LNURL for phone taps and a **Bolt Card** tap on the PN532 pays this exact product (PIN protection supported)
5. **CANCEL** (bottom-right) returns to the keypad; after `PRODUCT_TIMEOUT` of inactivity the device falls back to the main screen
6. After payment the relay/servo switches with the LNbits duration and the device returns to the main screen

While no product QR is shown, the NFC Tag 2 carries `https://zapbox.space` and Bolt Card taps are ignored. A pin triggered by the server (paid QR code from elsewhere) always switches, regardless of the current screen.

**Configuration:** *Web Installer → ZapBox Mode → Multi-channel → Numerical Product Selection → `Yes`*. Cannot be combined with *One for All* (the installer enforces this).

---

## Identity🫆Login

Both authentication methods are supported: **LNURL-auth** (wallet login via QR) and **NTAG 424 DNA** (NFC tap — Bolt Card / Bolt Ring), plus an optional **4-digit PIN pad** after the tap.

**Configuration:** *Web Installer → ZapBox Mode → Identity🫆Login*. The triggered pin defaults to CH01 (GPIO 14).

→ Mechanism, teach mode and security: **[docs/identity.md](identity.md)**

---

## Operation

### Touch display

- **General:** wakes from screensaver (not compatible with deep sleep)
- **1× tap / swipe:** show product page / change page
- **5 rapid taps anywhere + 2 s hold:** enter Config mode

### External LED Button (optional, GPIO 43/44)

- **LED:** on when the device is ready (no initialization, error or special mode active)
- **Press once:** show product page / change page
- **Hold ≥ 2 s:** open the Help page
- **Press 3× quickly:** open the Report page
- **Press briefly once, then hold ≥ 3 s:** enter Config mode

Wiring and button details: [docs/common-features.md → External LED Button](common-features.md#external-led-button)

### Screensaver & Deep Sleep

Deep-sleep wake-up is only possible via the **BOOT button (GPIO 0)** — the AXS15231B touch controller's INT pin is not routed to a GPIO on this module, so touch cannot wake the device.

→ Modes, power figures and configuration: [docs/common-features.md → Screensaver & Deep Sleep](common-features.md#screensaver--deep-sleep)

---

## Shared Features

These work the same way as on the other variants:

| Feature | Documentation |
|---------|---------------|
| NFC (Bolt Card, card emulation, NT3H2111 phone tap) | [docs/nfc.md](nfc.md) |
| Identity🫆Login | [docs/identity.md](identity.md) |
| BTC ticker, Special Modes, Threshold Mode | [docs/common-features.md](common-features.md) |
| I/O Expander (PCF8574, virtual pins 200–207) | [docs/common-features.md](common-features.md#io-expander--pcf8574) |
| Screensaver & Deep Sleep | [docs/common-features.md](common-features.md#screensaver--deep-sleep) |
| Startup sequence & error diagnostics | [docs/common-features.md](common-features.md#startup--error-detection) |
