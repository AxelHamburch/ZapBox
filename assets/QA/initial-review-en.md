# Test Protocol for Initial Review

This document serves as the documentation for the initial inspection of the ZapBox, during which the tests performed and the parameters checked ensure the quality and functionality of the device. The ZapBox undergoes a comprehensive initial inspection at a test station, where the functions and parameters of the device are carefully examined to ensure quality and reliability.

---

## Device Identification

**ZapBox Type:** ____________________     **Serial Number:** ____________________     **Housing Version:** ____________________

**Base Color:** ___________     **Front:** ___________     **Button:** ___________     **NFC Module:** ___________

**Other / Special Features:** ________________________________________________________________

---

**Note:** For each ZapBox type, the relevant tests are marked with an "O". Each version has options. If not applicable, please mark with *n.a.* for *not available*.

**Abbreviations:**<br>
I = Current / U = Voltage / Term. = Terminal / GPIO = General-Purpose Input/Output

## Inspection of ZapBox with Display (T-Display-S3)

| Test Object \ Type | Compact | Duo | Quattro | ZapOMat | Servo | Hybrid |
|---|---|---|---|---|---|---|
| I-Input USB-C (idle)  | - | - | - | ___,____A | ___,____A | ___,____A |
| U-Output Term.1-2 (idle) | - | - | - | ___,____V | ___,____V | ___,____V |
| U-Output USB-A/C (switched) | ___,____V | ___,____V | - | - | ___,____V | ___,____V |
| Relay Channel 1 (GPIO Pin 12) | - | O | O | O | O | O |
| Relay Channel 2 (GPIO Pin 13) | - | O | O | O | - | O |
| Relay Channel 3 (GPIO Pin 10) | - | - | O | O | - | O |
| Relay Channel 4 (GPIO Pin 11) | - | - | O | O | O | O |
| Servo Channel 2 (GPIO Pin 13) | - | - | - | - | O | O |
| Servo Channel 3 (GPIO Pin 10) | - | - | - | - | O | O |
| Sensor Term.11 (GPIO Pin 2) | - | - | - | O | O | O |
| Display | O | O | O | O | O | O |
| OnBoard-Button | O | O | O | O | O | O |
| LED-Button | - | O | O | O | O | O |
| Side Cover | O | O | O | O | O | O |
| Snap-Fit Mounting | O | O | O | O | O | O |
| NFC Module | O | O | O | O | O | O |

---

## Inspection of ZapBox Headless (ESP32)

| Test Object | Headless | Headless Servo |
|---|---|---|
| I-Input USB-C (idle)  | ___,____A | ___,____A |
| U-Output Term.1-2 (idle) | - | ___,____V |
| U-Output Term.1-2 (switched) | - | ___,____V |
| U-Output USB-C (switched) | ___,____V | - |
| Relay Channel 3 - Term.3 - (GPIO Pin 12) | - | O |
| Servo Channel 3 - Term.3 - (GPIO Pin 12) | - | O |
| Sensor 1 - Term.4 - (GPIO Pin 22) | - | O |
| Sensor 2 - Term.5 - (GPIO Pin 23) | - | O |
| LED-READY | O | O |
| LED-ACTION | O | O |
| Side Cover | O | O |
| Snap-Fit Mounting | O | O |
| NFC Module | O | O |

---

## Notes

⚠️ **NFC Module:** The NFC module may function and still be defective. The error can be identified by the high current consumption of over 0.3A in idle mode.

---

## Inspection Result

**Inspection Result:** [ &nbsp; ] Passed / [ &nbsp; ] Failed

**Other Defects:** [ &nbsp; ] No / [ &nbsp; ] Yes — If yes, which: __________________________________________________________

 **Block Time ⛓️ :** ________________________     **Inspector 🖊 :** ________________________

---

*Best Quality - **[zapbox.space](https://zapbox.space)*** | Version: qa946083 | Language: English