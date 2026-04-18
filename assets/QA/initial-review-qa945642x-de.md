# ZapBox - Erstprüfung

Die ZapBox wird an einer Teststation einer Erstprüfung unterzogen, bei der Funktionen und Parameter des Geräts zur Qualitätssicherung kontrolliert werden.

---

## Geräteidentifikation

**ZapBox Typ:** ________________________     **Seriennummer:** ________________________     **Gehäuse Version:** ________________________

**Farbe Grundkörper:** ___________     **Front:** ___________     **Taster:** ___________     **NFC Modul:** ___________

**Sonstiges / Besonderheit:** ________________________________________________________________

---

**Hinweis:** Für den jeweiligen ZapBox Typ sind die relevanten Prüfungen mit einem "O" gekennzeichnet. Bei jeder Version gibt es Optionen. Bei nichtvorkommen bitte mit *n.a.* für *not available* kennzeichen. 

## Prüfung ZapBox mit Display (T-Display-S3)

| Prüfobjekt \ Typ | Compact | Duo | Quattro | ZapOMat | Servo | Hybrid |
|---|---|---|---|---|---|---|
| Ausgangsspannung [V] Klemme 1-2 ruhend | - | - | - | ____,____V | ____,____V | ____,____V |
| Ausgangsspannung [V] Klemme 1-2 geschaltet | - | - | - | ____,____V | ____,____V | ____,____V |
| USB OUT Ausgangsspannung [V] geschaltet | ____,____V | ____,____V | - | ____,____V | ____,____V | ____,____V |
| CH1 Relais GPIO Pin 12 | - | O | O | O | O | O |
| CH2 Relais GPIO Pin 13 | - | O | O | O | - | O |
| CH3 Relais GPIO Pin 10 | - | - | O | O | - | O |
| CH4 Relais GPIO Pin 11 | - | - | O | O | O | O |
| CH2 Servo GPIO Pin 13 | - | - | - | - | O | O |
| CH3 Servo GPIO Pin 10 | - | - | - | - | O | O |
| Sensor GPIO Pin 2 | - | - | - | O | O | O |
| Funktion Display | O | O | O | O | O | O |
| Funktion OnBoard-Button | O | O | O | O | O | O |
| Funktion LED-Button | - | O | O | O | O | O |
| Panel Öffnung | O | O | O | O | O | O |
| Montage Schnappverschluss | O | O | O | O | O | O |
| NFC-Modul | O | O | O | O | O | O |

---

## Prüfung ZapBox Headless (ESP32)

| Prüfobjekt | Headless | Headless Servo |
|---|---|---|
| Ausgangsspannung Klemme 1-2 ruhend | - | ____,____V |
| Ausgangsspannung Klemme 1-2 geschaltet | - | ____,____V |
| USB OUT Ausgangsspannung geschaltet | ____,____V | - |
| Klemme 3 - Steuersignal Relais Pin 12 | - | O |
| Klemme 3 - Steuersignal Servo Pin 12 | - | O |
| Klemme 4 - Sensor 1 GPIO Pin 22 | - | O |
| Klemme 4 - Sensor 2 GPIO Pin 23 | - | O |
| Funktion LED-READY | O | O |
| Funktion LED-ACTION | O | O |
| Panel Öffnung | O | O |
| Montage Schnappverschluss | O | O |
| NFC-Modul | O | O |

---

## Hinweise

⚠️ **NFC-Modul:** Das NFC-Modul kann funktionieren und trotzdem defekt sein. Den Fehler erkennt man an dem hohen Stromverbrauch über 0,3A im Ruhezustand.

---

## Prüfungsergebnis

**Ergebnis der Prüfung:** [ &nbsp; ] Bestanden / [ &nbsp; ] Nicht bestanden

**Sonstige Mängel:** [ &nbsp; ] Nein / [ &nbsp; ] Ja — Wenn ja, welche: _________________________________________________________________

 **Blockzeit ⛓️ :** ________________________     **Prüfer 🖊 :** ________________________


---

*Beste Qualtiät - **[zapbox.space](https://zapbox.space)*** | Version: qa945642 | Sprache: Deutsch