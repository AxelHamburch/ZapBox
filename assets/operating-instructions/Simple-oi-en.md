# ZapBox Simple – Operating Instructions

**Language:** English | **Version:** oi967281

---

## Table of Contents

1. [Overview](#overview)
2. [Views](#views)
3. [Connections](#connections)
4. [Controls](#controls)
5. [Setup and Commissioning](#setup-and-commissioning)
6. [Technical Data](#technical-data)
7. [Safety Instructions](#safety-instructions)
8. [Further Links](#further-links)

---

## Overview

The **ZapBox Compact** is an electronic switch for Bitcoin Lightning payments. A payment via the Lightning Network can be used to switch an output – ideal for vending machines, presentations, event control, and many other applications.

### Basic Equipment

| Component | Description |
|---|---|
| Microcontroller | T-Display-S3 with 1.9" LCD display |
| Front panel | 35° display |
| Input | Dual USB-A and USB-C |
| Output | Dual USB-A and USB-C |
| Control | Two on-board micro switches |
| Option BTC ticker | Activatable via the Web Installer |

---

## Views

<img src="pics/pic-Simple/01.webp" alt="Front view" width="67%">

*Figure 1: Front view*

<img src="pics/pic-Simple/02.webp" alt="Rear view" width="67%">

*Figure 2: Rear view*

---

## Connections

### Input - Dual USB-A and USB-C socket for power supply (5V)

Power the device via the **Power IN** connector using a USB-C/A cable with **5 V DC (max. 5 A)**.

> **Note:** The USB power connector does not support automatic USB-C power negotiation (no USB-C Power Delivery). Some USB-C chargers or power modules therefore do not recognize the ZapBox as a load and will not supply power. In this case, use a **USB-A output** of the power supply or an alternative 5 V power source. The maximum current must not exceed 3 A.

---

### Input - USB-C socket on the microcontroller (data access, behind the right side panel)

To read or transfer data from the device, connect the ZapBox to a computer or laptop:

1. On the **right side of the front panel** there is a small, concealed flap.
2. Open the flap by pushing it to the right from below using a **narrow screwdriver**.
3. Connect a USB-C cable to the microcontroller's connector underneath.

<img src="pics/pic-Simple/03.webp" alt="Opening the panel and USB-C connector" width="67%">

*Figure: Opening the panel and USB-C connector for data*

> **Important note:** The USB connector directly on the microcontroller is intended exclusively for flashing the firmware and transferring configuration parameters. During the flashing process, no load may be connected or switched at the output, as this can lead to malfunctions or **damage to the microcontroller**.
>
> It is therefore recommended to either:
> - not connect any load at the output while the USB connection is active, or
> - additionally connect the regular **Power-IN input** to the same power supply. This ensures that the current for the power relay does not flow through the microcontroller and overload it.

---

### Error Diagnosis and Troubleshooting

The ZapBox has a convenient error display via the screen. There are four basic errors, which are prioritized:

| Prio. | Error type | Abbreviation | Detection method | Description |
|-----------|-----------|-----------|-------------------|--------------|
| 1 | **NO WIFI** | NW | WiFi connection status | WiFi network not connected<br>-> Is the WiFi data correct?<br>-> Is the WiFi signal too weak? |
| 2 | **NO INTERNET** | NI | HTTP check to Google | Internet connection lost<br>-> Is the internet reachable? |
| 3 | **NO SERVER** | NS | TCP port 443 check | LNbits server unreachable<br>-> Has the server hardware failed?<br>-> Is the device string correct? |
| 4 | **NO WEBSOCKET** | NWS | WebSocket connection status | WebSocket protocol/handshake error<br>-> Has LNbits failed?<br>-> Is the device string correct? |

The error messages are also logged and can be retrieved via the *Report Mode*:

- Press the HELP button twice quickly in succession to show error counters (0-99) for all four error types with their occurrence frequencies.
- Press the LED button three times quickly in succession (if an external LED button is available).

Further up-to-date information on error descriptions can be found on the Web Installer page in the "Error Detection & Report" and "Troubleshoot" chapters.

---

### Output - Dual USB-A and USB-C (switched 5V voltage)

The USB sockets are switched via a relay contact. The **total load** of the sockets should **not exceed 3 A**.

---

## Controls

The ZapBox has two small **on-board micro switches** that are connected directly to the microcontroller. All functions can be accessed via the micro switches. In addition, the ZapBox has a reset button on the underside of the front panel and two slide switches on the side.

### Function Overview - Micro Switches

| Function | Micro switch |
|---|---|
| Show help page | Press HELP once |
| Next page / product change | Press NEXT once |
| Show REPORT page | Press HELP twice |
| Open config mode | Hold NEXT for at least 5 sec. |

---

## Setup and Commissioning

The ZapBox is tested after manufacturing and shipped with the current firmware - however, it is not yet parameterized. The software is under active development, so it is recommended to flash the ZapBox with the latest firmware right at the start and then perform the parameterization. A convenient [**Web Installer**](https://installer.zapbox.space/) is available for this.

### Step 1: Firmware Update
1. Open the right side panel of the front panel, as described above under "Input - USB-C socket on the microcontroller".
2. Connect the ZapBox to the USB-C port with a cable and connect it to a computer.
3. Open a Chromium browser, for example Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera, or [Helium](https://helium.computer/).

### Step 2: Parameterization
1. Navigate to the Web Installer page in the browser.
2. Follow the instructions on the page to enter the desired parameters such as `WiFi SSID`, `WiFi password`, and `Device Settings String`.
3. Save the settings and restart the ZapBox.

> **Note:** No load should be connected to the outputs during setup, to avoid malfunctions or damage to the microcontroller.

After initialization, the ZapBox will display the product's QR code and is ready for the first payment and subsequent switching action.

---

## Technical Data

| Property | Value |
|---|---|
| Supply voltage | 5 V DC via USB-C |
| Maximum input current | 5.0 A |
| Output power | max. 3.0 A (recommended) |
| Display | 1.9" LCD (T-Display-S3) |
| Temperature range | 0–40 °C |
| Communication | Wi-Fi (ESP32-S3) |
| Payment protocol | Bitcoin Lightning Network |

---

## Safety Instructions

- Operate the device only with the specified supply voltage.
- Do not exceed the maximum current load of the outputs.
- Do not perform any work on the relay contacts under load.
- The device is not suitable for use in damp or wet environments.
- Ensure adequate ventilation around the device.
- Keep out of the reach of children.

---

## Further Links

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web Installer, quick overview & troubleshooting | https://installer.zapbox.space/ |
| Detailed documentation (parameters & functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub repository (software, PCB layouts, 3D print files, operating instructions, etc.) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Subject to changes and errors. As of: 2026*
