# ZapBox Duo – User Manual

**Language:** English | **Version:** oi940285

---

## Table of Contents

1. [Overview](#overview)
2. [Views](#views)
3. [Commissioning](#commissioning)
4. [Setup](#setup)
5. [Slide Switches](#slide-switches)
6. [Outputs](#outputs)
7. [NFC Module (optional)](#nfc-module-optional)
8. [Controls](#controls)
9. [Technical Specifications](#technical-specifications)
10. [Safety Instructions](#safety-instructions)
11. [Further Links](#further-links)

---

## Overview

The **ZapBox Duo** is an electronic switch for Bitcoin Lightning payments. A payment via the Lightning Network triggers two independent switched outputs – ideal for vending machines, presentations, event control, and many other applications.

### Basic Equipment

| Component | Description |
|---|---|
| Microcontroller | T-Display-S3 with 1.9" LCD display |
| Front panel | Available with 35° or 90° display front (90° also available for panel mounting) |
| Input | USB-C (Power IN) |
| Output Channel 1 (CH1) | 30 A power relay with external terminals |
| Output Channel 2 (CH2) | Dual socket with USB-A and USB-C |
| Control | LED button (full ZapBox functionality) |
| Switch 1 | 3-position slide switch (AUTO / OFF / ON) |
| Switch 2 | 2-position slide switch (Invert Output) |
| Option BTC Ticker | Activatable via the Web Installer |
| Option Board Buttons | Two on-board micro buttons |
| Option NFC Module | For Bolt Cards (NTAG424 DNA) and standard NTAG213/215/216 (with LNURL-withdraw) |

---

## Views

<img src="pic-Duo/Duo-oi-01.webp" alt="Front view" width="67%">

*Figure 1: Front view*

<img src="pic-Duo/Duo-oi-02.webp" alt="Rear view" width="67%">

*Figure 2: Rear view*

---

## Commissioning

### Power Supply

Power the device via the **Power IN** connector using a USB-C cable with **5 V DC (max. 5 A)**.

> **Note:** The Power IN connector is not "intelligent". Some chargers or power modules with a USB-C output do not recognise the ZapBox and will not supply power. In this case, use a **USB-A output** from your power supply or a different power source.

### First Test (pre-configured device)

If the device is already configured, you can immediately run a first test:

1. Press the **LED button** to switch between display pages.
2. Scan the displayed **QR code** with a Lightning wallet.
3. Pay the invoice.
4. The relay should switch audibly – the switching operation was successful.

---

## Setup

### Web Installer

The **Web Installer** is available for initial setup and firmware updates:

**https://installer.zapbox.space/**

The complete procedure is described there. It is recommended to always flash the **"Latest" firmware** to keep the ZapBox up to date.

### USB Data Access (concealed flap)

To read data from or transfer data to the device, connect the ZapBox to a computer or laptop:

1. On the **right side of the front panel** there is a small, concealed flap.
2. Open the flap by sliding it to the right from below using a **narrow screwdriver**.
3. Connect a USB-C cable to the microcontroller port underneath.

<img src="pic-Duo/Duo-oi-03.webp" alt="Opening the panel and USB-C connection" width="67%">

*Figure: Opening the panel and USB-C connection for data*

> **Important:** The USB connector on the microcontroller is intended exclusively for flashing new firmware or transferring configuration parameters. If switching functions are triggered while flashing, **malfunctions or damage to the microcontroller** may occur.
>
> It is therefore strongly recommended to either:
> - Avoid triggering switching functions while flashing, **or**
> - additionally connect the regular **Power IN** input to the same power supply, so that the current for the power relay does not flow through the microcontroller and overload it.

---

## Slide Switches

The ZapBox Duo has two slide switches.

### Switch 1 – Three-position slide switch (AUTO / OFF / ON)

| Position | Function |
|---|---|
| **A** (AUTO) | Automatic mode – normal operation |
| **0** (OFF) | Power supply interrupted – output OFF |
| **1** (ON) | CH2 output (dual USB A/C) permanently ON |

### Switch 2 – Two-position slide switch (Invert / Normal)

| Position | Function |
|---|---|
| **Normal** | CH2 is de-energised at rest (0 V). After switching, 5 V is applied to the USB sockets. |
| **Inv.** (Invert) | CH2 is at 5 V at rest. After switching, the output changes to 0 V (inverted switching). |

> **Note:** If Switch 2 is set to **Inv.** and Switch 1 is in **position 1**, the output is **OFF** – contrary to normal operation.

---

## Outputs

### Channel 1 (CH1) – 30 A Power Relay

The power relay on channel 1 exposes the **COM / NO / NC** contacts externally. These are covered with a **protective cap** from the factory.

- **Remove cap:** Simply pull upwards.
- **Attach cap:** First place the long side on top, then press the short angle downwards until the cap clicks into place.

The contacts of the power relay are rated for a **maximum current of 30 A**.

On the **top of the ZapBox** there is a small transparent opening. An LED behind it indicates the **ON status of the power relay**.

### Channel 2 (CH2) – Dual USB Socket (USB-A / USB-C)

The dual USB socket is switched via a relay contact (CH2). The **total load** across the sockets should **not exceed 3 A**.

---

## NFC Module (optional)

Depending on the configuration, an NFC module is built into the **top of the ZapBox**. It supports the following card types:

- **Bolt Cards** (NTAG424)
- **LNURL-Withdraw** via NTAG21x (213 / 215 / 216)

---

## Controls

Depending on the version, the ZapBox features two small **on-board micro buttons** connected directly to the microcontroller, in addition to the LED button. All functions are accessible via both the LED button and the micro buttons. The ZapBox also has a reset button on the underside.

### Function Overview

| Function | Micro Button | LED Button |
|---|---|---|
| Display help page | Press HELP 1× | Hold LED button for at least 2 seconds |
| Next page / product change | Press NEXT 1× | Press LED button 1× briefly |
| Display REPORT page | Press HELP 2× | Press LED button 3× in quick succession |
| Enter config mode | Hold NEXT for at least 5 seconds | Press 1× briefly, then hold for at least 5 seconds |

---

## Technical Specifications

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 3.5 A |
| CH1 switching capacity | max. 30 A |
| CH2 output current | max. 3.0 A (total) |
| Display | 1.9" LCD (T-Display-S3) |
| Communication | Wi-Fi (ESP32-S3) |
| Payment protocol | Bitcoin Lightning Network |

---

## Safety Instructions

- Operate the device only with the specified supply voltage.
- Do not exceed the maximum current ratings of the outputs.
- Do not perform any work on the relay contacts under load.
- The device is not suitable for use in humid or wet environments.
- Keep out of reach of children.

---

## Further Links

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web Installer, quick overview & troubleshooting | https://installer.zapbox.space/ |
| Detailed documentation (parameters & functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub repository (software, schematics, 3D print files) | https://github.com/AxelHamburch/ZapBox |

---

*Subject to changes and errors. As of: 2026*
