# ZapBox Quattro – User Manual

**Language:** English | **Version:** oi940421

---

## Table of Contents

1. [Overview](#overview)
2. [Views](#views)
3. [Connections](#connections)
4. [Controls](#controls)
5. [Setup and Commissioning](#setup-and-commissioning)
6. [Option: NFC Module](#option-nfc-module)
7. [Technical Specifications](#technical-specifications)
8. [Safety Notes](#safety-notes)
9. [Further Links](#further-links)

---

## Overview

The **ZapBox Quattro** is an electronic switch for Bitcoin Lightning payments. A payment via the Lightning Network triggers an output – ideal for vending machines, presentations, event control, and many other applications.

### Standard Equipment

| Component | Description |
|---|---|
| Microcontroller | T-Display-S3 with 1.9" LCD display |
| Front panel | Available with 35° or 90° display front (90° also available for built-in installation) |
| Input | USB-C (Power IN) |
| Outputs CH1–CH4 | Four relay outputs, switching contacts (NO/COM/NC) externally accessible |
| Control element | LED button (full ZapBox functionality) |
| Switch 1 | 3-position slide switch (AUTO / OFF / ON) |
| Switch 2 | 2-position slide switch (Invert Output) |
| Option BTC Ticker | Activatable via the Web Installer |
| Option board buttons | Two on-board micro buttons |
| Option NFC module | For Bolt Cards (NTAG424 DNA) and standard NTAG213/215/216 (with LNURL-withdraw) |

---

## Views

<img src="pic-Quattro/Quattro-oi-01.webp" alt="Front view" width="67%">

*Figure 1: Front view*

<img src="pic-Quattro/Quattro-oi-02.webp" alt="Rear view" width="67%">

*Figure 2: Rear view*

<img src="pic-Quattro/Quattro-oi-04.webp" alt="Top view" width="67%">

*Figure 3: Top view*

---

## Connections

### Input – USB-C socket for power supply (5 V)

Supply the device via the **Power IN** connector with a USB-C cable providing **5 V DC (max. 5 A)**.

> **Note:** The Power IN connector is not "smart". Some chargers or power modules with USB-C output may not recognise the ZapBox and deliver no power. In this case, use a **USB-A output** of the power supply or a different power source.

---

### Input – USB-C socket on the microcontroller (data access, behind the right side panel)

To read from or write to the device, connect the ZapBox to a computer or laptop:

1. On the **right side of the front panel** there is a small, concealed flap.
2. Open the flap by sliding it to the right with a **narrow flat-head screwdriver** from below.
3. Connect a USB-C cable to the microcontroller port underneath.

<img src="pic-Quattro/Quattro-oi-03.webp" alt="Opening the panel and USB-C connector" width="67%">

*Figure: Opening the panel and USB-C connector for data*

> **Important:** The USB connector on the microcontroller is intended exclusively for flashing new firmware or transferring configuration parameters. If switching functions are triggered while flashing, **malfunctions or damage to the microcontroller** may occur.
>
> It is therefore recommended to:
> - Avoid triggering switching functions during connection, **or**
> - Additionally connect the regular **Power IN input** to the same power supply so that current for the power relay does not flow through the microcontroller and overload it.

---

### Outputs CH1–CH4

The Quattro has four relay outputs accessible from outside as switching contacts (NO/COM/NC). To reach the screws for the terminal connectors, a narrow cover must be pried off. Insert a small flat-head screwdriver into each of the two openings above the switching contacts and push the cover upward.

The relay contacts are rated for a **maximum current load of 10 A**.

---

## Controls

Depending on the version, the ZapBox features two small **on-board micro buttons** in addition to the **LED button**, both directly connected to the microcontroller. All functions are accessible via either the LED button or the micro buttons. The ZapBox also has a reset button on the bottom of the front panel and two slide switches on the side.

### Function Overview – Micro Buttons / LED Button

| Function | Micro button | LED button |
|---|---|---|
| Show help page | Press HELP once | Hold LED button for at least 2 seconds |
| Next page / product change | Press NEXT once | Press LED button briefly once |
| Show REPORT page | Press HELP twice | Press LED button 3× quickly |
| Enter config mode | Hold NEXT for at least 5 seconds | Press briefly once, then hold for at least 5 seconds |

---

## Setup and Commissioning

The ZapBox is tested after manufacturing and shipped with the current firmware – but it is not pre-configured. The software is actively developed, so it is recommended to flash the latest firmware right at the start and then perform the configuration. A convenient **Web Installer** is available for this purpose.

Step-by-step setup instructions:

1. Open the right side panel of the front panel as described above under "Input – USB-C socket on the microcontroller".
2. Connect the ZapBox to the USB-C port using a cable and plug it into a computer.
3. Open a Chromium-based browser, such as Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera, or [Helium](https://helium.computer/).
4. Navigate to the Web Installer page **https://installer.zapbox.space/**.
5. Flash the latest "Latest" version as described in step 1 of the Web Installer.
6. After flashing, close the small window and proceed to step 3 – Load config values. Click the `🔌 Connect` button.
7. You should now see `✅ Connected` and `✅ Config mode` in the green field, provided the ZapBox is currently in `SERIAL CONFIG MODE`. The display should confirm this. If not, check step 2 – Prepare connection.
8. The ZapBox requires three parameters: `WiFi SSID` / `WiFi password` / `Device settings string`. You obtain the Device Settings String from your LNbits wallet. Add the **Bitcoin Switch** or **ZapBox** extension. The ZapBox extension also supports the NFC module; otherwise they are identical.
9. Once all three parameters have been entered, save them using the `🔥 Write Config` button and restart the ZapBox using the `🔁 Restart` button.

That should be all. After initialisation, the ZapBox will display the product QR code and is ready for the first payment and subsequent action on the relay output.

For errors or faults, please refer to the "Error Detection & Report" and "Troubleshoot" sections further down on the Web Installer page.

---

## Option: NFC Module

Depending on the configuration, an NFC module may be installed on the **top of the ZapBox**. It is also available separately. The ZapBox currently supports the following card types:

- **Bolt Cards** (NTAG424)
- **LNURL-Withdraw** from NTAG21x (213 / 215 / 216)

The LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension) is required for this feature. It must be supported by the LNbits server.

---

## Technical Specifications

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 5.0 A |
| Output current | max. 3.0 A (recommended) |
| Display | 1.9" LCD (T-Display-S3) |
| Communication | Wi-Fi (ESP32-S3) |
| Payment protocol | Bitcoin Lightning Network |

---

## Safety Notes

- Operate the device only with the specified supply voltage.
- Do not exceed the maximum current ratings of the outputs.
- Do not work on relay contacts under load.
- The device is not suitable for use in humid or wet environments.
- Keep out of reach of children.

---

## Further Links

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web Installer, quick overview & troubleshooting | https://installer.zapbox.space/ |
| Detailed documentation (parameters & functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub repository (software, PCB layouts, 3D print files, manuals) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Subject to change without notice. As of: 2026*
