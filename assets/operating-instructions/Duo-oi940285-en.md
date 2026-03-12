# ZapBox Duo – User Manual

**Language:** English | **Version:** oi940285

---

## Table of Contents

1. [Overview](#overview)
2. [Views](#views)
3. [Connections](#connections)
4. [Controls](#controls)
5. [Setup and Commissioning](#setup-and-commissioning)
6. [NFC Module (optional)](#nfc-module-optional)
7. [Technical Specifications](#technical-specifications)
8. [Safety Instructions](#safety-instructions)
9. [Further Links](#further-links)

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

## Connections

### Input – USB-C socket for power supply (5V)

Power the device via the **Power IN** connector using a USB-C cable with **5 V DC (max. 5 A)**.

> **Note:** The Power IN connector is not "intelligent". Some chargers or power modules with a USB-C output do not recognise the ZapBox and will not supply power. In this case, use a **USB-A output** from your power supply or a different power source.

---

### Input – USB-C socket on the microcontroller (data access, behind right side panel)

To read data from or transfer data to the device, connect the ZapBox to a computer or laptop:

1. On the **right side of the front panel** there is a small, concealed flap.
2. Open the flap by sliding it to the right from below using a **narrow screwdriver**.
3. Connect a USB-C cable to the microcontroller port underneath.

<img src="pic-Duo/Duo-oi-03.webp" alt="Opening the panel and USB-C connection" width="67%">

*Figure: Opening the panel and USB-C connection for data*

> **Important:** The USB connector on the microcontroller is intended exclusively for flashing new firmware or transferring configuration parameters. If switching functions are triggered while flashing, **malfunctions or damage to the microcontroller** may occur.
>
> It is therefore recommended to either:
> - Avoid triggering switching functions during the connection, **or**
> - additionally connect the regular **Power IN** input to the same power supply, so that the current for the power relay does not flow through the microcontroller and overload it.

### Output

#### Channel 1 (CH1) – 30 A Power Relay

The power relay on channel 1 exposes the **COM / NO / NC** contacts externally. These are covered with a **protective cap** from the factory.

- **Remove cap:** Simply pull upwards.
- **Attach cap:** First place the long side on top, then press the short angle downwards until the cap clicks into place.

The contacts of the power relay are rated for a **maximum current of 30 A**.

On the **top of the ZapBox** there is a small transparent opening. An LED behind it indicates the **ON status of the power relay**.

#### Channel 2 (CH2) – Dual USB Socket (USB-A / USB-C)

The dual USB socket is switched via a relay contact (CH2). The **total load** across the sockets should **not exceed 3 A**.

---

## Controls

Depending on the version, the ZapBox features two small **on-board micro buttons** connected directly to the microcontroller, in addition to the **LED button**. All functions are accessible via both the LED button and the micro buttons. In addition, the ZapBox has a reset button on the underside of the front panel and two slide switches on the side.

### Function Overview – Micro Buttons / LED Button

| Function | Micro Button | LED Button |
|---|---|---|
| Display help page | Press HELP 1× | Hold LED button for at least 2 seconds |
| Next page / product change | Press NEXT 1× | Press LED button 1× briefly |
| Display REPORT page | Press HELP 2× | Press LED button 3× in quick succession |
| Enter config mode | Hold NEXT for at least 5 seconds | Press 1× briefly, then hold for at least 5 seconds |

### Function Overview – Slide Switches

The ZapBox Duo has two slide switches.

#### Switch 1 – Three-position slide switch (AUTO / OFF / ON)

| Position | Function |
|---|---|
| **A** (AUTO) | Automatic mode – normal operation |
| **0** (OFF) | Power supply interrupted – output OFF |
| **1** (ON) | CH2 output (dual USB A/C) permanently ON (with Switch 2 – Inverse = OFF) |

#### Switch 2 – Two-position slide switch (standard / inverted)

| Position | Function |
|---|---|
| **Std.** (Standard) | The output is de-energised at rest (0 V). After switching, 5 V is applied to the USB sockets. |
| **Inv.** (Inverse) | The output is at 5 V at rest. After switching, the output changes to 0 V (inverted switching). |

> **Note:** If Switch 2 is set to **Inv.** and Switch 1 is in **position 1**, the output is **OFF** – contrary to normal operation.

---

## Setup and Commissioning

The ZapBox is tested after production and shipped with the current firmware – however, it is not yet configured. The software is actively being developed, so it is recommended to flash the ZapBox with the latest firmware right from the start and then perform the configuration. A convenient **Web Installer** is available for this.

Here is a step-by-step guide for setup:

1. Open the right side panel of the front panel as described above under "Input – USB-C socket on the microcontroller".
2. Connect the ZapBox via the USB-C port with a cable to a computer.
3. Open a Chromium-based browser, for example Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera, or [Helium](https://helium.computer/).
4. Navigate to the Web Installer page **https://installer.zapbox.space/**.
5. Flash the latest "Latest" version as described in step 1 of the Web Installer.
6. After flashing, close the small window and go to step 3 – Load config values. Select the `🔌 Connect` button.
7. You should now see `✅ Connected` and `✅ Config mode` in the green field, provided the ZapBox is now in `SERIAL CONFIG MODE`. The display should show this. If not, check step 2 – Prepare connection.
8. The ZapBox requires three parameters: `WiFi SSID` / `WiFi password` / `Device settings string`. You receive the Device Settings String from your LNbits wallet. To do so, add the **Bitcoin Switch** or **ZapBox** extension. The ZapBox extension also supports the NFC module; otherwise they are identical.
9. Once all three parameters have been entered, save them using the `🔥 Write Config` button and restart the ZapBox using the `🔁 Restart` button.

That should be all. After initialisation, the ZapBox will display the QR code of the product and is ready for the first payment and subsequent action on the USB output.

In case of errors or faults, please refer to the "Error Detection & Report" and "Troubleshoot" sections further down on the Web Installer page.

---

## NFC Module (optional)

Depending on the configuration, an NFC module is built into the **top of the ZapBox**. It supports the following card types:

- **Bolt Cards** (NTAG424)
- **LNURL-Withdraw** via NTAG21x (213 / 215 / 216)

The LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension) is required for this feature. It must be supported by the LNbits server.

---

## Technical Specifications

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 5.0 A |
| CH1 switching capacity | max. 30 A |
| CH2 output current | max. 3.0 A (recommended/total) |
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
| GitHub repository (software, schematics, 3D print files, manuals) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |

---

*Subject to changes and errors. As of: 2026*
