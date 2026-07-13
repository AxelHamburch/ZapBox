# ZapBox T-Display-S3 (LilyGo)

ESP32-S3 with an integrated 170 × 320 LCD. The classic ZapBox — the most widely deployed variant, available with and without touch.

**Firmware suffix:** *(none)* (e.g. `v957859`) · **Web Installer:** [installer.zapbox.space](https://installer.zapbox.space/)

---

## Table of Contents

- [Hardware](#hardware)
- [GPIO Mapping](#gpio-mapping)
- [Relay Channels](#relay-channels)
- [Multi-Channel Mode](#multi-channel-mode)
- [Ambient Lighting (Channel 4)](#ambient-lighting-channel-4)
- [Servo Mode](#servo-mode)
- [Vending Machine Light Barrier](#vending-machine-light-barrier)
- [Identity🫆Login](#identitylogin)
- [Operation](#operation)
- [Shared Features](#shared-features)

---

## Hardware

| | |
|---|---|
| **Microcontroller** | ESP32-S3, dual-core |
| **Display** | 170 × 320 ST7789 TFT (8-bit parallel interface) |
| **Touch** | CST816S / CST328 — **optional**; the firmware auto-detects touch capability at startup |
| **Memory** | 16 MB Flash, 8 MB PSRAM |
| **Power draw** | ~150–250 mA |
| **Channels** | 4 relay channels (+ 8 more via [I/O Expander](common-features.md#io-expander--pcf8574)) |
| **Buttons** | 2 physical buttons (BOOT / HELP) + optional external LED button |

**I²C addresses:** Touch CST816S/CST328 `0x15` or `0x5A` · PN532 `0x24` · NT3H2111 `0x55` · PCF8574 `0x20`

---

## GPIO Mapping

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 0 | BOOT Button | Input | Pull-up | Left physical button — Config mode trigger (5 s hold) |
| 14 | HELP Button | Input | Pull-up | Right physical button — Help / Report mode |
| 2 | Light Barrier | Input | Pull-up | NPN vending machine light barrier (active LOW) |
| 4 | Battery Voltage | ADC Input | - | Battery voltage monitoring |
| **Display Control** |
| 38 | LCD Backlight | Output | HIGH=ON | Display brightness (PWM capable) |
| 5 | LCD RES | Output | - | Display reset |
| 6 | LCD CS | Output | - | Display chip select |
| 7 | LCD DC | Output | - | Display data/command |
| 8 | LCD WR | Output | - | Display write |
| 9 | LCD RD | Output | - | Display read |
| 39–42, 45–48 | LCD Data (D0–D7) | Output | - | 8-bit parallel display data bus |
| **Touch & NFC** |
| 17 | I²C SCL | I²C | - | Shared: Touch + NFC |
| 18 | I²C SDA | I²C | - | Shared: Touch + NFC |
| 16 | Touch INT | Input | - | Touch controller interrupt |
| 21 | Touch RES | Output | - | Touch controller reset |
| 1 | NFC IRQ | Input | - | PN532 interrupt (card detection) |
| **Power & External Controls** |
| 15 | Power On | Output | - | Power control pin |
| 43 | LED Button (LED) | Output | HIGH=ON | External illuminated button LED (3.3 V) |
| 44 | LED Button (SW) | Input | Pull-up | External button switch (active LOW) |
| **Relay Channels** |
| 12 | Relay 1 (CH01) | Output | - | Single mode default; Duo/Quattro/Servo mode 1 |
| 13 | Relay 2 (CH02) / Servo 1 | Output | - | Duo/Quattro mode 2; Servo mode PWM output |
| 10 | Relay 3 (CH03) / Servo 2 | Output | - | Quattro mode 3; Servo mode PWM output |
| 11 | Relay 4 (CH04) / Ambient | Output | - | Quattro mode 4; Servo mode relay 2; or ambient lighting |
| **Fixed-Function Expansion** |
| 3 | FD — NT3H2111 | Input | INPUT_PULLUP | ℹ️ Strapping pin — Field Detection, see note below |

> **ℹ️ GPIO 3 — Field Detection, a strapping pin with low practical risk**
>
> GPIO 3 is permanently configured as `INPUT_PULLUP` and used exclusively as the FD input from the NT3H2111. No Web Installer configuration required.
>
> On the ESP32-S3, GPIO 3 controls the **JTAG signal source**:
> - HIGH at boot (default) → JTAG routed through the GPIO matrix (GPIO 39–42)
> - LOW at boot → JTAG routed through the USB Serial/JTAG controller
>
> This has **no effect on normal firmware operation**. There is no download-mode risk and no bricking scenario. A 10 kΩ series resistor between the NT3H2111 FD pin and GPIO 3 is sufficient protection. The only real consequence of a LOW at boot is that an attached JTAG debugger may need to connect via USB instead of GPIO 39–42.

> ⚠️ **All GPIOs are allocated.** GPIO 3 is the only physically unconnected pin, but as a strapping pin it is unsuitable for sensors that can be LOW at power-on. There is **no free GPIO** for additional sensors — use the [I/O Expander](common-features.md#io-expander--pcf8574) instead.

**Output type:** digital GPIO outputs (HIGH = relay activated) · **Max current per pin:** ~40 mA — use a relay driver IC (ULN2003/ULN2803) or MOSFET for real loads.

---

## Relay Channels

| Channel | GPIO | Note |
|---------|------|------|
| CH01 | 12 | Single mode default; primary / Special-Mode channel |
| CH02 | 13 | Servo 1 PWM in Servo mode |
| CH03 | 10 | Servo 2 PWM in Servo mode |
| CH04 | 11 | Relay 2 in Servo mode; or [ambient lighting](#ambient-lighting-channel-4) |

Eight further channels (virtual pins 200–207) can be added via the [PCF8574 I/O Expander](common-features.md#io-expander--pcf8574) — the recommended route, since no free GPIOs remain.

---

## Multi-Channel Mode

**Touch variant only** (product selection needs swipe navigation).

Control multiple relays with automatic product selection and label integration:

- **Single Mode** *(default)* — one relay on Pin 12
- **Duo Mode** — two products on Pins 12 and 13
- **Quattro Mode** — four products on Pins 12, 13, 10 and 11
- **Servo Mode** — see [Servo Mode](#servo-mode)

**Features:**

- **Touch navigation** — swipe left/right on the product selection screen to choose a product
- **Automatic LNURL generation** — each pin gets its own unique LNURL (Bech32, LUD17 format, HRP `lnurl`, XOR 1 checksum)
- **Backend product labels** — fetched automatically from LNbits via `/api/v1/public/{deviceId}` and shown on all QR screens
  - Up to 3 words per label, split across lines
  - Currency symbols converted to text: €→EUR, $→USD, £→GBP, ¥→YEN, ₿→BTC, ₹→INR, ₽→RUB, ¢→ct
  - Third line uses a smaller font for the currency
- **Timeout** — the product selection screen reappears automatically after a few seconds on a QR screen
- **Loop navigation** — wraps around (last → first, first → last)

**Configuration (Web Installer):** `single` · `duo` · `quattro` · `servo` · `one-for-all` *(default in Servo mode)*

**Use cases:** vending machines, multi-product payment terminals, servo-controlled dispensers and barriers.

---

## Ambient Lighting (Channel 4)

**Available in Quattro Mode only.**

Channel 4 (GPIO 11) can be configured as an ambient lighting switch that **synchronizes with the display backlight** instead of acting as a payment-controlled relay.

- **Backlight synchronization:** GPIO 11 mirrors the state of the display backlight (GPIO 38) — HIGH when the display is active, LOW during screensaver or deep sleep
- **Reduced product count:** only 3 products are shown (channels 1–3) instead of 4
- **No payment needed:** GPIO 11 switches automatically based on display state
- **Initial state after boot:** HIGH (display active by default)

**Configuration:** *Web Installer → Multi-Channel Mode – Quattro → Special function Channel 4* → `Ambient lighting switch`

**Use cases:** ambient LED strip lighting synchronized with display activity, mood lighting that turns off during screensaver, a visual power-state indicator.

> **Note:** with ambient lighting active, channel 4 cannot be used for payment-controlled switching.

---

## Servo Mode

Controls up to two servo motors via Lightning payment. Each servo is paired with a relay that can cut power between uses.

| Pin | Function | Description |
|-----|----------|-------------|
| 12 | Relay 1 | Primary trigger — QR code shown for this channel |
| 13 | 180° Servo PWM | Sweeps Start→End, holds for the action time, returns |
| 10 | 360° Servo PWM | Spins for the configured duration on payment |
| 11 | Relay 2 / Ambient | Activates for the action time duration |

### Activation modes

**One for All (OFA)** — *default*
- A single QR code (Pin 12) is shown to the customer
- On payment, all four channels activate simultaneously as concurrent FreeRTOS tasks
- Ideal for vending machines and dispensers where one payment triggers the complete mechanism

**Independent channels**
- Each channel has its own QR code and can be triggered separately
- Useful for multi-product setups where each item has a different price

### Servo configuration (Web Installer)

| Parameter | Description |
|-----------|-------------|
| **Start angle (°)** | Position the servo moves to at startup (0–180°) |
| **End angle (°)** | Position the servo sweeps to on payment trigger (0–180°) |
| **Sweep duration (ms)** | Time for one full sweep; `0` = native servo speed (max) |
| **Return to start** | `Yes` — servo always returns after the relay-off delay · `No` — toggle mode |

**Toggle mode (`Return = No`):** first trigger sweeps Start → End and stays there; the next trigger sweeps back. Ideal for latches, barriers or dispensers where the servo should hold its position.

**Servo 2 is optional:** if all Servo 2 values are zero, only Servo 1 (Pin 12/13) is active and Pin 11 remains available as a regular relay channel.

> ⚠️ **Important:**
> - Requires an external 5 V supply for the servo (an MG996R draws up to 2.5 A peak)
> - Special Mode (blink/pulse/strobe) is automatically bypassed in Servo mode — pulsing the relay would interfere with servo timing
> - GPIO 10 and 13 are LEDC-capable on the ESP32-S3; the headless ESP32 Dev cannot do this

---

## Vending Machine Light Barrier

Optical item detection via an infrared light barrier on **GPIO 2**.

**Two operating modes:**

- **Stop the advance** (`yes`) — the conveying cycle ends early as soon as the product falls through the light barrier
- **Monitoring product blockage** (`monitor`) — after each payment the device checks whether the product exit is clear. If the barrier is still active, the display shows *PRODUCT BLOCKED — Remove the product* and all further payments are locked (NFC taps and QR payments refused) until the path is physically cleared

**Hardware:** NPN phototransistor light barrier module (3-wire, active LOW)

| Property | Value |
|---|---|
| Pin | GPIO 2 |
| Input type | Digital input with internal pull-up |
| Active state | LOW (barrier broken / item detected) |
| Inactive state | HIGH (barrier intact / no item) |

**Wiring:**

```
Light Barrier Module    →    T-Display-S3    →    GND
────────────────────────────────────────────────────
+5V / +3.3V            →    Power supply
GND                    →                     →    GND
Signal (NPN output)    →    GPIO 2
```

---

## Identity🫆Login

Both authentication methods are supported: **LNURL-auth** (wallet login via QR) and **NTAG 424 DNA** (NFC tap — Bolt Card / Bolt Ring).

**Configuration:** *Web Installer → ZapBox Mode → Identity🫆Login*. The triggered pin defaults to CH01 (GPIO 12).

→ Mechanism, teach mode and security: **[docs/identity.md](identity.md)**

---

## Operation

### On-board button — RESET
- Restarts the device completely

### On-board button — HELP (GPIO 14)
- **Click once:** open the Help page
- **Double-click:** open the Report page

### On-board button — NEXT (BOOT / GPIO 0)
- **General:** wakes from screensaver and deep sleep
- **1× click:** show product page / change page
- **Hold > 5 s:** enter Config mode (clicking again closes it prematurely)
- **Firmware update:** hold BOOT, press RESET once, release BOOT → download mode

### Touch display *(touch version only)*
- **General:** wakes from screensaver (not from deep sleep)
- **1× tap / swipe:** show product page / change page

### Touch button *(red circle next to the touch field)*
- **Click once:** open the Help page
- **Double-click:** open the Report page
- **Quadruple-click:** open the Config page

### External LED button *(optional, GPIO 43/44)*
- **LED:** on when the device is ready
- **Press once:** show product page / change page
- **Hold ≥ 2 s:** open the Help page
- **Press 3× quickly:** open the Report page
- **Press briefly once, then hold ≥ 3 s:** enter Config mode

> **Note:** GPIO 43/44 are not RTC-capable — the LED button can wake the device from **Light Sleep** but not from **Deep Sleep (Freeze)**.

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
