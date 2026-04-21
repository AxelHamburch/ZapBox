# Testprotokoll zur Erstprüfung

Dieses Dokument dient der Dokumentation der Erstprüfung der ZapBox, bei der die durchgeführten Tests und überprüften Parameter die Qualität und Funktionalität des Geräts sicherstellen. Die ZapBox wird an einer Teststation einer umfassenden Erstprüfung unterzogen, bei der die Funktionen und Parameter des Geräts sorgfältig überprüft werden, um die Qualität und Zuverlässigkeit sicherzustellen.

---

## Geräteidentifikation

**ZapBox Typ:** _____________________     **Seriennummer:** _____________________     **Gehäuse Version:** _____________________

**Farbe Grundkörper:** ___________     **Front:** ___________     **Taster:** ___________     **NFC Modul:** ___________

**Sonstiges / Besonderheit:** ________________________________________________________________

---

**Hinweis:** Für den jeweiligen ZapBox Typ sind die relevanten Prüfungen mit einem "O" gekennzeichnet. Bei jeder Version gibt es Optionen. Bei nichtvorkommen bitte mit *n.a.* für *not available* kennzeichen.

**Abkürzungen:**<br>
I = Strom / U = Spannung / Kl. = Klemme / GPIO = General-Purpose Input/Output (universelle Ein-/Ausgabe) 

## Prüfung ZapBox mit Display (T-Display-S3)

| Prüfobjekt \ Typ | Compact | Duo | Quattro | ZapOMat | Servo | Hybrid |
|---|---|---|---|---|---|---|
| I-Eingang USB-C (ruhend)  | - | - | - | ___,____A | ___,____A | ___,____A |
| U-Ausgang Kl.1-2 (ruhend) | - | - | - | ___,____V | ___,____V | ___,____V |
| U-Ausgang USB-A/C (geschaltet) | ___,____V | ___,____V | - | - | ___,____V | ___,____V |
| Relais Kanal 1 (GPIO Pin 12) | - | O | O | O | O | O |
| Relais Kanal 2 (GPIO Pin 13) | - | O | O | O | - | O |
| Relais Kanal 3 (GPIO Pin 10) | - | - | O | O | - | O |
| Relais Kanal 4 (GPIO Pin 11) | - | - | O | O | O | O |
| Servo Kanal 2 (GPIO Pin 13) | - | - | - | - | O | O |
| Servo Kanal 3 (GPIO Pin 10) | - | - | - | - | O | O |
| Sensor Kl.11 (GPIO Pin 2) | - | - | - | O | O | O |
| Display | O | O | O | O | O | O |
| OnBoard-Button | O | O | O | O | O | O |
| LED-Button | - | O | O | O | O | O |
| Seitenabdeckung | O | O | O | O | O | O |
| Montage Schnappverschluss | O | O | O | O | O | O |
| NFC-Modul | O | O | O | O | O | O |

---

## Prüfung ZapBox Headless (ESP32)

| Prüfobjekt | Headless | Headless Servo |
|---|---|---|
| I-Eingang USB-C (ruhend)  | ___,____A | ___,____A |
| U-Ausgang Kl.1-2 (ruhend) | - | ___,____V |
| Ausgangsspannung Klemme 1-2 geschaltet | - | ___,____V |
| U-Ausgang USB-C (geschaltet) | ___,____V | - |
| Relais Kanal 3 - Kl.3 - (GPIO Pin 12) | - | O |
| Servo Kanal 3 - Kl.3 - (GPIO Pin 12) | - | O |
| Sensor 1 - Kl.4 - (GPIO Pin 22) | - | O |
| Sensor 2 - Kl.5 - (GPIO Pin 23) | - | O |
| LED-READY | O | O |
| LED-ACTION | O | O |
| Seitenabdeckung | O | O |
| Montage Schnappverschluss | O | O |
| NFC-Modul | O | O |

---

## Hinweise

⚠️ **NFC-Modul:** Das NFC-Modul kann funktionieren und trotzdem defekt sein. Den Fehler erkennt man an dem hohen Stromverbrauch über 0,3A im Ruhezustand.

---

## Prüfungsergebnis

**Ergebnis der Prüfung:** [ &nbsp; ] Bestanden / [ &nbsp; ] Nicht bestanden

**Sonstige Mängel:** [ &nbsp; ] Nein / [ &nbsp; ] Ja — Wenn ja, welche: _______________________________________________________

 **Blockzeit ⛓️ :** ________________________     **Prüfer 🖊 :** ________________________


---

*Beste Qualtiät - **[zapbox.space](https://zapbox.space)*** | Version: qa946083 | Sprache: Deutsch