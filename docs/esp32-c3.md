# ZapBox Compact Headless (ESP32-C3-21-1)

The smallest and most power-efficient ZapBox. Single-core RISC-V, no display, one relay plus two flex channels — built for tight spaces and battery-powered installations.

**Firmware suffix:** `c` (e.g. `v957859c`) · **Web Installer:** [installer.zapbox.space/c3/](https://installer.zapbox.space/c3/)

> ℹ️ The C3 firmware is **not built for every release** — it is only published when explicitly requested. Check the installer dropdown for the latest available version.

---

## Table of Contents

- [Hardware](#hardware)
- [GPIO Mapping](#gpio-mapping)
- [Channels](#channels)
- [Flex Channel Modes](#flex-channel-modes)
- [Shared Features](#shared-features)

---

## Hardware

| | |
|---|---|
| **Microcontroller** | ESP32-C3-WROOM-02 — single-core RISC-V @ 160 MHz |
| **Display** | none — status LED only |
| **Memory** | 4 MB Flash, 400 KB SRAM |
| **Power draw** | ~80–120 mA — the lowest of all variants |
| **Channels** | 1 relay + 2 flex channels |
| **USB** | Native USB CDC/JTAG (GPIO 19 D− / GPIO 20 D+) — no external UART chip |
| **Configuration** | Web Installer (serial) |

**Advantages:** lowest cost, smallest PCB footprint, efficient single-core processor, native Bluetooth support (not currently used).

**Use cases:** extremely space-constrained installations (embedded relays, pole mounts), battery-powered applications, high-volume cost-sensitive deployments, IoT/automation integrations.

---

## GPIO Mapping

| GPIO | Function | Type | Direction | Description |
|------|----------|------|-----------|-------------|
| **User Input** |
| 9 | BOOT Button | Input | Pull-up | ⚠️ On the C3 the BOOT button is **IO9**, not IO0. Hold LOW during reset → download mode |
| **LEDs & Status** |
| 5 | Status LED | Output | HIGH=ON | Solid = ready · slow blink = config mode · fast blink = error / payment |
| **Relay & Flex Channels** |
| 4 | Relay (CH01) | Output | - | Primary relay coil driver (HFD3/5 — 1 A 30 VDC / 0.5 A 125 VAC) |
| 6 | Flex CH01 | Output *or* Input | Pull-up (sensor) | Relay / servo / sensor — see [Flex Channel Modes](#flex-channel-modes) |
| 7 | Flex CH02 | Output *or* Input | Pull-up (sensor) | Relay / servo / sensor — see [Flex Channel Modes](#flex-channel-modes) |
| **I²C (optional)** |
| 20 | I²C SDA | I²C | - | Board header pin labelled **RXD** |
| 21 | I²C SCL | I²C | - | Board header pin labelled **TXD** |
| 10 | NFC IRQ | Input | - | PN532 interrupt (active LOW) |
| **Free** |
| 0, 1, 2, 3, 18 | unassigned | — | — | Available for future use |

> ⚠️ **GPIO 11–17 must never be configured as outputs on the C3** — GPIO 15 is the SPI flash write-protect pin, and driving these pins corrupts the flash SPI and causes an immediate panic/reboot.

**I²C note:** with `ARDUINO_USB_CDC_ON_BOOT=1` the serial console runs over native USB, so UART0 is idle and GPIO 20/21 are free to serve as the I²C bus.

---

## Channels

| Channel | GPIO | Note |
|---------|------|------|
| CH01 | 4 | Primary relay — the only LNbits payment channel |
| Flex CH01 | 6 | Secondary actuator or sensor |
| Flex CH02 | 7 | Secondary actuator or sensor |

The two flex channels are **not independent payment channels**. In actor mode they always fire **together with GPIO 4**.

---

## Flex Channel Modes

Each flex channel (GPIO 6 and GPIO 7) is configured independently in the Web Installer:

| Mode | Behaviour |
|------|-----------|
| `relay` | Secondary relay output — fires together with GPIO 4 |
| `servo180` | 180° positional servo |
| `servo360` | 360° continuous rotation servo |
| `yes` | **Sensor** — stops the relay action when triggered (INPUT_PULLUP, active LOW) |
| `monitor` | **Sensor** — blocks the next payment until the path is cleared |
| `level` | **Sensor** — blocks payments when the supply bin is empty (HIGH = empty) |

---

## Shared Features

These work the same way as on the other variants:

| Feature | Documentation |
|---------|---------------|
| NFC (Bolt Card, card emulation, NT3H2111 phone tap) | [docs/nfc.md](nfc.md) |
| BTC ticker, Special Modes, Threshold Mode | [docs/common-features.md](common-features.md) |
| Startup sequence & error diagnostics | [docs/common-features.md](common-features.md#startup--error-detection) |
