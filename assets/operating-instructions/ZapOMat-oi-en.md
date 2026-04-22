# ZapBox ZapOMat – User Manual

**Language:** English | **Version:** oi946210

---

## Table of Contents

1. [Introduction](#introduction)
2. [Power Supply](#power-supply)
3. [External Connectors – 16-pin Connector](#external-connectors--16-pin-connector)
4. [USB-C Connector](#usb-c-connector)
5. [Switching Outputs Channel 1 to 4](#switching-outputs-channel-1-to-4)
6. [Control Elements](#control-elements)
7. [Mounting Bracket with Snap Lock](#mounting-bracket-with-snap-lock)
8. [Setup and Commissioning](#setup-and-commissioning)
9. [NFC Module (Optional)](#nfc-module-optional)
10. [Technical Specifications](#technical-specifications)
11. [Safety Notes](#safety-notes)
12. [Further Resources](#further-resources)

---

## Introduction

The **ZapBox ZapOMat** is an electronic switch for Bitcoin Lightning payments. With a payment over the Lightning Network, outputs can be switched – ideal for vending machines, presentations, event control and many other applications.

For comprehensive automation tasks, the contacts are routed externally via a 16-pin removable connector. It features an additional sensor input that can be individually configured. Optionally, the NFC module can be routed externally via the terminal strip.

For industrial use and convenient connection, the ZapOMat can be operated with a DC power supply unit as an alternative to the 5V USB-C connector. The ZapOMat can optionally be equipped with a wide-range input (DC 6V–36V) via a standard 5.5×2.1 mm DC jack. A voltage regulator is available for the internally required 5V, which is self-regulating. This greatly expands the range of applications and the power supply for the outputs can be adjusted accordingly.

<img src="pics/pic-ZapOMat/ZapOMat-oi-01.webp" alt="Front view" width="75%">

*Image 1: Front and rear view*

## Standard Equipment

| Component | Description |
|---|---|
| Microcontroller | T-Display-S3 with 1.9" LCD Display |
| Front Panel | Optional with 35° or 90° display front (90° also available for installation) |
| Power Supply | USB-C (5V) and Wide-Range Input DC 6V-36V (Hollow Jack 5.5*2.1 mm) |
| Inputs / Outputs | 16-pin Connector (15EDGWC-3.81mm) |
| Outputs | 4 Relay Outputs CH1-CH4 - Switching Contacts (NO/COM/NC) |
| Input | 1 Sensor Input |
| Interface | Connection for External NFC Module |
| Control Element | LED Button (Full ZapBox Functionality) |
| Option BTC-Ticker | Activatable via Web Installer |
| Option Board Buttons | Two On-board Micro Buttons (optionally hidden) |
| Option NFC Module | For Bolt Cards (NTAG424 DNA) and Standard NTAG213/215/216 (with LNURL-withdraw) |
| Option Switch | Slide Switch (ON/OFF) |

---

## Power Supply

The ZapBox ZapOMat features a USB-C connector as well as a DC hollow jack (5.5 × 2.1 mm) for standard DC power supply units.

### USB-C Connector
The USB-C connector enables power supply at 5 V and is suitable for simple applications with a power requirement of up to a maximum of 3 amperes.

### DC Hollow Jack
The DC jack is designed as a wide-range input for voltages from 6 V to 36 V. The input voltage is internally regulated to 5 V via a step-down converter. The voltage regulator can provide up to 1 A in continuous operation. **Please note the heat generation ⚠️.** Temporarily, 2 A is also possible, and in very short load peaks up to 2.5 A.

### Power Supply Notes
- If a power output of more than 1 A is required continuously, it is recommended to use the USB-C Power connector with a separate 5 V power supply.
- In this case, up to 3 A is possible for the sum of all consumers.

Since individual consumers, such as LED strips, can draw high currents (> 1 A), combined power supply is also possible. For example, the ZapBox can be powered via the DC jack with 12 V and simultaneously power the relay contact at output 4 with this voltage. The internal voltage regulator supplies the system and relay outputs 1–3 with 5 V, while a 12 V LED strip at output 4 is directly powered with 12 V. This reduces the load on the internal voltage regulator.

Another frequently required variant is the operation of 24 V motors on the switching outputs. For this, the ZapBox is powered via the DC jack input with 24 V and internally the voltage before the voltage regulator is tapped and wired to the designated terminal 3 and GND. The external bridge between terminal 1 and terminal 3 must be removed. This means switching outputs 1 to 3 are powered with 24 V and can drive 24 V motors. However, not more than 3 amperes in total. Switching output 4 can be powered with 5 V via a bridge between 1 and 8. This allows channel 4 to be used, for example, for LED lighting as ambient light for the background.

In addition, a 5 V power supply with a maximum of 3 A can be connected to the USB-C Power connector alongside the DC supply (6 V–36 V).
Simultaneous operation of both supplies is possible, however, differences in the 5 V voltages should be considered to avoid mutual interference or backfeeding of the power supplies.

### Voltage Input USB-C Socket (5V)

The ZapBox can be powered via the USB-C connector **Power** with **5 V DC (max. 3 A)**. The 5 V can also be tapped from this connector if power is supplied via the DC hollow jack.

> **Note:** The USB Power connector does not support automatic USB-C Power Request (no USB-C Power Delivery). Some USB-C chargers or power modules do not recognize the ZapBox as a consumer and therefore do not supply power. In this case, use a **USB-A output** from the power supply or an alternative 5 V power source. The maximum current must not exceed 3 A.

### Voltage Input DC Hollow Jack (6 V–36 V)

The DC jack is designed for hollow plugs of the 5.5 × 2.1 mm type and is suitable for standard power supplies, e.g., with 12 V or 24 V output voltage.

Internally, it is possible to tap the input voltage and route it to the terminal strip to supply the output channels directly with the input voltage. This requires intervention in the device and may only be performed by qualified electrical professionals. Details can be found in the [E-Layout](https://github.com/AxelHamburch/ZapBox#electrical-layout) of the ZapBox ZapOMat.

> **Note:** The internal voltage regulator has limited performance. At high load currents, increased heat generation occurs, which can cause the device to heat up. Ensure adequate cooling and do not exceed the specified current limits.

## External Connectors – 16-pin Connector

The ZapBox features a removable 16-pin connector.

**Terminal Assignment Overview:**
| Terminal | Function | Usage | Note |
|------|----------|-----------|---------------|
| 1 | 5 V | Output | Power Supply 5 V |
| 2 | GND | Output | Ground / 0 V |
| 3 | Supply Channel 1-3 | Input / Output | 5 V or 6 V–36 V |
| 4 | Switching Contact (NO) Relay Channel 1 | Output | Consumer Control (NO – Normally Open) |
| 5 | Switching Contact (NO) Relay Channel 2 | Output | Consumer Control (NO – Normally Open) |
| 6 | Switching Contact (NO) Relay Channel 3 | Output | Consumer Control (NO – Normally Open) |
| 7 | GND | Output | Ground / 0 V  |
| 8 | Supply Channel 4 | Input / Output | 5 V or 6 V–36 V |
| 9 | Switching Contact (NO) Relay Channel 4 | Output | Consumer Control (NO – Normally Open) |
| 10 | GND | Output | Ground / 0 V |
| 11 | Sensor (GPIO Pin 2) | Input | For example, a light barrier (NPN) |
| 12 | 5 V | Output | 5 V for Sensor and NFC Module |
| 13 | GND | Output | Ground for Sensor and NFC Module |
| 14 | NFC IRQ (GPIO Pin 1) | Interface | External NFC Module |
| 15 | I2C SCL (GPIO Pin 17) | Interface | External NFC Module |
| 16 | I2C SDA (GPIO Pin 18) | Interface | External NFC Module |

<img src="pics/pic-ZapOMat/ZapOMat-oi-03.webp" alt="Top view" width="50%">

*Image 2: Terminal Assignment Sequence Connector, from right to left*

**Note on Voltage Distribution with Bridges:**

In the factory state, terminals 1 (General Power Supply) and 12 (Sensor and NFC Module Power Supply) are supplied with 5 V. Ground (GND) is wired to terminals 2, 7, 10, and 13.

The relay contacts have no internal power supply voltage. The desired switching voltage must be provided externally via bridge. For example, to switch a 5 V consumer, an external bridge (on the 16-pin connector) must be set from terminal 1 to terminal 3 for channels 1–3. To operate channel 4 with 5 V, a bridge from terminal 1 to terminal 8 is required.

**Note on Relay Contacts:**

The relay contacts are internally designed as normally open (NO) and are routed externally via terminals 4, 5, and 6 as well as terminal 9. If a relay contact is to be used as a normally closed (NC) contact, internal rewiring is required.

**Note on DC Jack (6 V–36 V for Terminals 3 & 8):**

The input voltage of the DC hollow jack (6 V–36 V) can be internally tapped and routed to terminals 3 and/or 8 to switch relay outputs with higher voltages.

**Requirements:**
- Interventions may only be performed by qualified electrical professionals.
- Any existing 5 V bridges (Terminal 1 → 3 and/or Terminal 1 → 8) must be **removed first**.
- Details can be found in the [E-Layout](https://github.com/AxelHamburch/ZapBox#electrical-layout) of the ZapBox ZapOMat.

## USB-C Connector

### Opening Side Panel
To read data from the device or transfer data, connect the ZapBox to a computer or laptop:

1. A small cover is located on the **right side of the front panel**.
2. Open the cover by pushing it to the right from below with a **narrow screwdriver**. Some models have a small recess on the cover. This recess can be used to flip the cover forward.

### Connecting the USB-C Cable
3. Connect a USB-C cable to the microcontroller connector located below.

<img src="pics/pic-ZapOMat/ZapOMat-oi-02.webp" alt="Opening panel and USB-C connector" width="75%">

*Image 3: Opening panel and USB-C connector for data*

> **Important Note:** The USB connector directly on the microcontroller is intended exclusively for flashing firmware and transmitting configuration parameters. During the flashing process, no load may be connected or switched at the output, as this can cause malfunctions or **damage to the microcontroller**.
>
> Therefore, it is recommended:
> - Not to connect any load at the output during the USB connection, or
> - Additionally connect the regular **Power-IN input** to the same power supply. This ensures that the current for the power relay does not flow through the microcontroller and does not overload it.

---

## Switching Outputs Channel 1 to 4

The ZapOMat has four relay outputs that can be routed externally via the switching contacts (NO/COM/NC) through the connector terminals. The normally open contacts (NO) for channels 1 to 3 are wired to terminals 4, 5, and 6, and for channel 4 to terminal 9.

The switching outputs are designed for a **maximum current load of 3 A continuous duty**. They can also be temporarily loaded with up to 5 A.

---

## Control Elements

Depending on the version, the ZapBox features an **LED Button** and two small **on-board micro buttons** connected directly to the microcontroller. All functions can be accessed via both the LED Button and the micro buttons. Additionally, the ZapBox has a reset button on the bottom of the front panel and optionally a slide switch on the side for other applications.

### Control Element Overview - Micro Buttons / LED Button

| Function | Micro Buttons | LED Button |
|---|---|---|
| Display Help Page | Press HELP 1× | Hold LED Button for at least 2 seconds |
| Next Page / Product Switch | Press NEXT 1× | Press LED Button 1× briefly |
| Display REPORT Page | Press HELP 2× | Press LED Button 3× rapidly in succession |
| Enter Config Mode | Hold NEXT for at least 5 seconds | Press briefly 1×, then hold for at least 5 seconds |

---

## Mounting Bracket with Snap Lock

The optional mounting bracket allows for easy installation of the ZapBox.
The snap lock can be released with a flat-head screwdriver.

<img src="pics/pic-ZapOMat/ZapOMat-oi-04.webp" alt="Releasing the mounting bracket" width="100%">

*Image 4: Releasing the mounting bracket*

> **Note:** The ZapBox is merely slid onto the mounting bracket and secured with a snap lock. The lock can be released with a flat-head screwdriver. To do this, carefully insert the screwdriver into the slot, lift slightly, and push the upper part of the ZapBox toward the screwdriver. The connection should release and the ZapBox can be separated from the mounting plate by lifting.

---

## Setup and Commissioning

The ZapBox is tested during manufacturing and shipped with the current firmware – however, it is not yet configured. Since the software is actively being developed, it is recommended to flash the ZapBox with the latest firmware at the beginning and then perform configuration. For this purpose, there is a convenient [**Web Installer**](https://installer.zapbox.space/).

### Step 1: Firmware Update
1. Open the right side panel of the front panel as described above under "Opening Side Panel".
2. Connect the ZapBox to the USB-C port with a cable and connect it to a computer.
3. Open a Chromium browser, such as Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera, or [Helium](https://helium.computer/).

### Step 2: Configuration
1. Navigate in the browser to the Web Installer page.
2. Follow the instructions on the page to enter the desired parameters such as `WiFi SSID`, `WiFi Password`, and `Device Settings String`.
3. Save the settings and restart the ZapBox.

> **Note:** During setup, no load should be connected to the outputs to avoid malfunctions or damage to the microcontroller.

After initialization, the ZapBox will display the QR code of the product and is ready for the first payment and subsequent switching action.

The ZapBox features convenient error display on the screen. There are four basic errors that are prioritized:

| Priority | Error Type | Abbreviation | Detection Method | Description |
|-----------|-----------|-----------|-------------------|--------------|
| 1 | **NO WIFI** | NW | WiFi Connection Status | WiFi network not connected<br>-> Are the WiFi details correct?<br>-> Is the WiFi signal too weak? |
| 2 | **NO INTERNET** | NI | HTTP Check to Google | Internet connection lost<br>-> Is the internet reachable? |
| 3 | **NO SERVER** | NS | TCP Port 443 Check | LNbits Server Not Reachable<br>-> Has the server hardware failed?<br>-> Is the device string correct? |
| 4 | **NO WEBSOCKET** | NWS | WebSocket Connection Status | WebSocket Protocol/Handshake Error<br>-> Has LNbits crashed?<br>-> Is the device string correct? |

Error messages are also logged and can be retrieved via *Report Mode*:

- Press the HELP button twice quickly in succession to display error counters (0-99) for all four error types with their occurrence frequencies.
- Press the LED button three times rapidly in succession (if an external LED button is available).

For more current information about error descriptions, refer to the Web Installer page in the chapters "Error Detection & Report" and "Troubleshoot".

---

## NFC Module (Optional)

Depending on the configuration, the ZapBox features an NFC module that is either mounted on the top or can be used externally as a separate module via the 16-pin connector.

The ZapBox currently supports the following card types:

- **Boltcards** (NTAG424 DNA)
- **LNURL-Withdraw** from NTAG21x (213 / 215 / 216)

A requirement for the function is the LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension). This must be supported by the LNbits server.

---

## Technical Specifications

| Property | Value |
|---|---|
| Power Supply Voltage | 5V DC via USB-C, max. 3A |
| Power Supply Voltage | 6V–36V via DC Hollow Jack, max. 3A at 24V |
| DC-DC Voltage Converter | 6V–36V → 5V, max. 1A continuous duty, 2A temporarily (2.5A peak) |
| Relay Switching Outputs | Max. 3A Continuous Duty (temporarily 5A) |
| Display | 1.9" LCD (T-Display-S3) |
| NFC Module | PN532 |
| Temperature Range | 0–40 °C |
| Communication | Wi-Fi (ESP32-S3) |
| Payment Protocol | Bitcoin Lightning Network |

---

## Safety Notes

- Operate the device exclusively with the specified power supply voltage.
- Do not exceed the maximum current ratings of the outputs.
- Do not perform work on relay contacts under load.
- The device is not suitable for use in wet or damp environments.
- Keep out of reach of children.

---

## Further Resources

| Resource | Link |
|---|---|
| Overview of all ZapBox models | https://zapbox.space/ |
| Web Installer, Quick Overview & Troubleshooting | https://installer.zapbox.space/ |
| Detailed Documentation (Parameters & Functions) | https://ereignishorizont.xyz/zapbox/ |
| GitHub Repository (Software, 3D Print Files, Instructions, etc.) | https://github.com/AxelHamburch/ZapBox |
| GitHub Repository (E-Layouts) | https://github.com/AxelHamburch/ZapBox#electrical-layout |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Subject to change and errors. Version: 2026*
