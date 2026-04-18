# ZapBox - Initial Review

The ZapBox undergoes an initial review at a test station where device functions and parameters are checked for quality assurance.

---

## Device Identification

**ZapBox Type:** ________________________     **Serial Number:** ________________________     **Housing Version:** ________________________

**Color Body:** ___________     **Front:** ___________     **Button:** ___________     **NFC Module:** ___________

**Miscellaneous / Special Features:** ________________________________________________________________

---

**Note:** For each ZapBox type, relevant tests are marked with an "O". Each version has options. If not available, please mark with *n.a.* for *not available*.

## Testing ZapBox with Display (T-Display-S3)

| Test Item \ Type | Compact | Duo | Quattro | ZapOMat | Servo | Hybrid |
|---|---|---|---|---|---|---|
| Output Voltage [V] Terminal 1-2 idle | - | - | - | ____,____V | ____,____V | ____,____V |
| Output Voltage [V] Terminal 1-2 switched | - | - | - | ____,____V | ____,____V | ____,____V |
| USB OUT Output Voltage [V] switched | ____,____V | ____,____V | - | ____,____V | ____,____V | ____,____V |
| CH1 Relay GPIO Pin 12 | - | O | O | O | O | O |
| CH2 Relay GPIO Pin 13 | - | O | O | O | - | O |
| CH3 Relay GPIO Pin 10 | - | - | O | O | - | O |
| CH4 Relay GPIO Pin 11 | - | - | O | O | O | O |
| CH2 Servo GPIO Pin 13 | - | - | - | - | O | O |
| CH3 Servo GPIO Pin 10 | - | - | - | - | O | O |
| Sensor GPIO Pin 2 | - | - | - | O | O | O |
| Display Function | O | O | O | O | O | O |
| OnBoard-Button Function | O | O | O | O | O | O |
| LED-Button Function | - | O | O | O | O | O |
| Panel Opening | O | O | O | O | O | O |
| Assembly Snap Lock | O | O | O | O | O | O |
| NFC Module | O | O | O | O | O | O |

---

## Testing ZapBox Headless (ESP32)

| Test Item | Headless | Headless Servo |
|---|---|---|
| Output Voltage Terminal 1-2 idle | - | ____,____V |
| Output Voltage Terminal 1-2 switched | - | ____,____V |
| USB OUT Output Voltage switched | ____,____V | - |
| Terminal 3 - Control Signal Relay Pin 12 | - | O |
| Terminal 3 - Control Signal Servo Pin 12 | - | O |
| Terminal 4 - Sensor 1 GPIO Pin 22 | - | O |
| Terminal 4 - Sensor 2 GPIO Pin 23 | - | O |
| LED-READY Function | O | O |
| LED-ACTION Function | O | O |
| Panel Opening | O | O |
| Assembly Snap Lock | O | O |
| NFC Module | O | O |

---

## Notes

⚠️ **NFC Module:** The NFC module can work but still be defective. The error is identified by high power consumption above 0.3A in standby state.

---

## Test Result

**Test Result:** [ &nbsp; ] Passed / [ &nbsp; ] Failed

**Other Defects:** [ &nbsp; ] No / [ &nbsp; ] Yes — If yes, which ones: _________________________________________________________________

**Block Time ⛓️ :** ________________________     **Tester 🖊 :** ________________________


---

*Best Quality - **[zapbox.space](https://zapbox.space)*** | Version: qa945642 | Language: English
