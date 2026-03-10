# ZapBox Compact – Operating Instructions

**Language:** English | **Version:** OI940117

---

## Overview

The **ZapBox Compact** is an electronic switch for Bitcoin Lightning payments. A payment via the Lightning Network triggers an output — ideal for vending machines, presentations, event control, and many other applications.

### Base Equipment

| Component | Description |
|---|---|
| Microcontroller | T-Display-S3 with 1.9" LCD display |
| Front panel | Available with 35° or 90° display front (90° also available for panel mounting) |
| Input | USB-C (Power IN) |
| Output | USB-A socket |
| Controls | Two on-board micro buttons |
| Switch 1 | 3-position slide switch (AUTO / OFF / ON) |
| Switch 2 | 2-position slide switch (Invert Output) |
| Option BTC Ticker | Activatable via the Web Installer |
| Option NFC Module | For Bolt Cards (NTAG424 DNA) and standard NTAG213/215/216 (with LNURL-withdraw) |

---

## Views

<img src="pic-Compact/Compact-oi-01.webp" alt="Front view" width="67%">

*Figure 1: Front view*

<img src="pic-Compact/Compact-oi-02.webp" alt="Rear view" width="67%">

*Figure 2: Rear view*

---

## Getting Started

### Power Supply

Supply the device via the **Power IN** connector using a USB-C cable with **5 V DC (max. 5 A)**.

> **Note:** The Power IN connector is not "intelligent". Some chargers or power modules with a USB-C output do not recognise the ZapBox and will not supply power. In this case, use a **USB-A output** of the power supply or a different power source.

### First Test (pre-configured device)

If the device is already configured, you can immediately perform a first test:

1. Press **NEXT** to cycle through the display pages.
2. Scan the displayed **QR code** with a Lightning wallet.
3. Pay the invoice.
4. The relay should audibly switch — the switching operation was successful.

---

## Setup

### Web Installer

For initial setup and firmware updates, the **Web Installer** is available:

**https://installer.zapbox.space/**

The complete procedure is described there. It is recommended to always flash the **"Latest" firmware** to keep the ZapBox up to date.

### USB Data Access (hidden cover)

To read or transfer data from the device, connect the ZapBox to a computer or laptop:

1. On the **right side of the front panel** there is a small, concealed cover.
2. Open the cover by sliding it to the right using a **narrow screwdriver** from below.
3. Connect a USB-C cable to the microcontroller connector underneath.

<img src="pic-Compact/Compact-oi-03.webp" alt="Opening the panel and USB-C connector" width="67%">

*Figure: Opening the panel and USB-C connector for data*

> **Important note:** The USB connector directly on the microcontroller is used exclusively for flashing new firmware or transferring configuration parameters. If switching functions are triggered simultaneously during flashing, **malfunctions or damage to the microcontroller** may occur.
>
> It is therefore recommended to:
> - Not trigger any switching functions during flashing, **or**
> - additionally connect the regular **Power IN input** to the same power supply, so that the current for the power relay does not flow through the microcontroller and overload it.

---

## Slide Switches

The ZapBox Compact has two slide switches.

### Switch 1 – Triple Slide Switch (AUTO / OFF / ON)

| Position | Function |
|---|---|
| **A** (AUTO) | Automatic mode – normal operation |
| **0** (OFF) | Power supply interrupted – output OFF |
| **1** (ON) | Output permanently ON |

### Switch 2 – Dual Slide Switch (Invert / Normal)

| Position | Function |
|---|---|
| **Normal** | The output is de-energised at rest (0 V). After switching, 5 V is present at the USB sockets. |
| **Inv.** (Invert) | The output is at 5 V at rest. After switching, the output changes to 0 V (inverted switching). |

> **Note:** If the dual switch is set to **Inv.** and the triple switch is in **position 1**, the output is — unlike in normal operation — **OFF** instead of ON.

---

## Output: USB-A Socket

The USB socket is switched via a relay contact. The **total load** on the sockets should **not exceed 3 A**.

---

## NFC Module (optional)

Depending on the configuration, an NFC module is installed on the **top of the ZapBox**. It supports the following card types:

- **Bolt Cards** (NTAG424)
- **LNURL-Withdraw** from NTAG21x (213 / 215 / 216)

---

## Controls

The ZapBox has two small **on-board micro buttons** connected directly to the microcontroller. All functions are accessible via the micro buttons. The ZapBox also has a Reset button on the underside.

### Function Overview

| Function | Micro Button |
|---|---|
| Show help page | Press HELP once |
| Next page / product change | Press NEXT once |
| Show REPORT page | Press HELP twice |
| Enter config mode | Hold NEXT for at least 5 seconds |

---

## Technical Specifications

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 3.5 A |
| Output power | max. 3.0 A |
| Display | 1.9" LCD (T-Display-S3) |
| Communication | Wi-Fi (ESP32-S3) |
| Payment protocol | Bitcoin Lightning Network |

---

## Safety Instructions

- Operate the device only with the specified supply voltage.
- Do not exceed the maximum current ratings of the outputs.
- Do not work on relay contacts under load.
- The device is not suitable for use in humid or wet environments.
- Keep out of reach of children.

---

---

## Further Links

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web Installer, quick overview & troubleshooting | https://installer.zapbox.space/ |
| Detailed documentation (parameters & functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub repository (software, PCB layouts, 3D print files) | https://github.com/AxelHamburch/ZapBox |

---

*Subject to changes and errors. As of: 2026*
