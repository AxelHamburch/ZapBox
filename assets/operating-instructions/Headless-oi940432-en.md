# ZapBox Headless – Operating Instructions

**Language:** English | **Version:** oi940432

---

## Table of Contents

1. [Overview](#overview)
2. [Views](#views)
3. [Connections](#connections)
4. [Controls](#controls)
5. [Setup and Commissioning](#setup-and-commissioning)
6. [Option: NFC Module](#option-nfc-module)
7. [Technical Specifications](#technical-specifications)
8. [Safety Instructions](#safety-instructions)
9. [Further Links](#further-links)

---

## Overview

The **ZapBox Headless** is an electronic switch for Bitcoin Lightning payments without a display. A single payment over the Lightning Network can switch an output — ideal for embedded applications, concealed installations, mechanical engineering, and anywhere a display is not required.

The operating state is indicated exclusively via a **Status LED**. A second **Action LED** signals the switching function.

### Standard Equipment

| Component | Description |
|---|---|
| Microcontroller | ESP32 Dev Module (no display) |
| Input | USB-C (Power IN) |
| Output | USB-C (Power OUT) |
| Status indicator | Status LED with blink patterns and Action LED as feedback |
| Control element | Micro push-button for BOOT (config mode) and Reset |
| Switch | 3-position slide switch (AUTO / OFF / ON) |
| Optional NFC module | For Bolt Cards (NTAG424 DNA) and standard NTAG213/215/216 (with LNURL-withdraw) |

---

## Views

<img src="pic-Headless/Headless-oi-01.webp" alt="Front view – top view" width="67%">

*Figure 1: Front view / Top view*

---

## Connections

### Input – USB-C Socket for Power Supply (5 V)

Supply the device via the **Power IN** connector using a USB-C cable with **5 V DC (max. 5 A)**.

> **Note:** The Power IN connector is not "intelligent". Some chargers or power modules with a USB-C output do not recognise the ZapBox and will not deliver current. In this case, use a **USB-A output** on the power supply or a different power source.

---

### Input – Micro-USB Socket on the Microcontroller (Data Access, Front)

To read data from or transfer data to the device, connect the ZapBox to a computer or laptop:

1. On the **front left side** there is a small panel to the left of the USB connectors. Open the panel by sliding it to the right using a **narrow screwdriver** inserted from below.
2. Connect a Micro-USB cable to the microcontroller.

<img src="pic-Headless/Headless-oi-02.webp" alt="Opening the panel for data connection" width="67%">

*Figure 2: Opening the panel for data connection*

<img src="pic-Headless/Headless-oi-03.webp" alt="Micro USB Port" width="67%">

*Figure 3: Micro USB Port*

> **Important:** The USB connector on the microcontroller itself is intended exclusively for flashing firmware and transferring configuration parameters. During the flashing process, no load must be connected or switched at the output, as this can cause malfunctions or **damage to the microcontroller**.
>
> It is therefore recommended that:
> - no load is connected to the output during the USB connection, or
> - the regular **Power IN** input is additionally connected to the same power supply. This ensures that the current for the power relay does not flow through the microcontroller and overload it.

---

### Output – USB-C Socket (Switched 5 V)

The USB socket is switched via a relay switching contact. The **total load** on the sockets should **not exceed 3 A**.

---

## Controls

The ZapBox Headless has **no display**. The operating state is indicated exclusively via the **Status LED** and **Action LED**.

### Status LED – Blink Patterns

| Pattern | Meaning |
|---|---|
| 3× short on startup | Boot completed |
| Fast blinking | Connecting / initialising |
| Slow blinking (1 Hz) | Config mode active |
| Steady light | Ready, waiting for payment |
| 200 ms on / 800 ms off | NFC payment pending |
| 2× short | Payment successful |
| 3× short | NFC timeout / error |
| 1× blink (500 ms on/off, 2 s pause) | Error pattern 1: No Wi-Fi |
| 2× blink (300 ms on/off, 2 s pause) | Error pattern 2: No internet |
| 3× blink (250 ms on/off, 2 s pause) | Error pattern 3: Server unreachable |
| 4× blink (200 ms on/off, 2 s pause) | Error pattern 4: WebSocket connection failed |

### Push-Buttons

| Function | Button |
|---|---|
| Enter config mode | Hold BOOT button for at least 5 seconds |
| Restart | Reset button |

### Triple Slide Switch (AUTO / OFF / ON)

| Position | Function |
|---|---|
| **A** (AUTO) | Automatic mode – Normal operation |
| **0** (OFF) | Power supply interrupted – Output OFF |
| **1** (ON) | Output (USB-C) permanently ON |

---

## Setup and Commissioning

The ZapBox is tested after production and shipped with the current firmware — however, it is not yet configured. As the software is actively being developed, it is recommended to flash the ZapBox with the latest firmware at the very beginning and then perform the configuration. A convenient **web installer** (headless version) is available for this purpose.

Here is a step-by-step guide for setup:

1. Connect the ZapBox to a computer via the Micro-USB port on the microcontroller using a cable.
2. Open a Chromium-based browser, such as Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera, or [Helium](https://helium.computer/).
3. Go to the web installer page **https://installer.zapbox.space/headless/**.
4. Flash the latest "Latest" version (Headless) as described in step 1 of the web installer.
5. After flashing, close the small window and go to step 3 – Load config values. There, click the `🔌 Connect` button.
6. You should now see `✅ Connected` and `✅ Config mode` in the green field. Config mode is active as soon as the **Status LED blinks slowly** (approx. 1 Hz). If not, check step 2 – Prepare connection.
7. The ZapBox requires three parameters: `WiFi SSID` / `WiFi password` / `Device settings string`. You get the Device Settings String from your LNbits wallet. To do this, add the **Bitcoin Switch** or **ZapBox** extension. The ZapBox extension also supports the NFC module; otherwise they are identical.
8. Once all three parameters have been entered, save them using the `🔥 Write Config` button and restart the ZapBox using the `🔁 Restart` button.

After initialisation, the Status LED will light up steadily — the ZapBox is ready for operation and waiting for the first payment.

In case of errors or issues, please refer to the "Error Detection & Report" and "Troubleshoot" sections further down on the web installer page.

---

## Option: NFC Module

Depending on the configuration, an NFC module may be installed on the **top side of the ZapBox**. Alternatively, the module is also available separately. The ZapBox currently supports the following card types:

- **Boltcards** (NTAG424)
- **LNURL-Withdraw** from NTAG21x (213 / 215 / 216)

The LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension) is required for this function to work. It must be supported by the LNbits server.

---

## Technical Specifications

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 5.0 A |
| Output current | max. 3.0 A (recommended) |
| Microcontroller | ESP32 Dev Module (WROOM-32) |
| Flash memory | 4 MB |
| SRAM | 512 KB |
| Display | None (Headless) |
| Status indicator | Status LED / Action LED |
| Communication | Wi-Fi (ESP32) |
| Relay channels | 1 – Expandable up to 12 (CH01–CH12) |
| Payment protocol | Bitcoin Lightning Network |

---

## Safety Instructions

- Only operate the device with the specified supply voltage.
- Do not exceed the maximum current ratings of the outputs.
- Do not perform any work on relay contacts under load.
- The device is not suitable for use in humid or wet environments.
- Keep out of reach of children.

---

## Further Links

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web installer, quick overview & troubleshooting | https://installer.zapbox.space/headless/ |
| Detailed documentation (parameters & functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub repository (software, schematics, 3D print files, operating instructions) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Subject to changes and errors. As of: 2026*
