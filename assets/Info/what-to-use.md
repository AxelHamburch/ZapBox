# NFC-Modul für WoS-Kompatibilität: Was verwenden?

## Problem

Der PN532 (aktuell im ZapBox verbaut) kann keine echte NTAG21x-kompatible NFC-Emulation ausführen.
Der `GET_VERSION`-Befehl (`0x60`) kollidiert intern im PN532 mit dem Mifare-Auth-Opcode.
WoS erwartet einen echten Type-2-Tag (NTAG21x, SAK=0x00) — ISO-DEP (Type 4) wird von WoS nicht als LNURL-Tag erkannt.

→ Lösung: externes **NT3H2111-Modul** als echter NTAG I2C Tag

---

## NT3H2111 — Fertig zu kaufen

### Mikroe "NFC Tag 2 Click" ⭐ Empfohlen
- NT3H2111W0FHK, Antenne integriert, alle Pins herausgeführt (I2C, FD, VCC, GND)
- Preis: ~8–10 €
- Bezug: Mouser, Digi-Key, TME, Mikroe-Shop
- Passt direkt per Dupont-Kabel an I2C (GPIO18/17)

### DFRobot "I2C NFC Tag Module"
- Ebenfalls NT3H2111, Antenne vorhanden, 4-Pin I2C-Header
- Preis: ~5–7 €
- Bezug: DFRobot-Shop, AliExpress (DFRobot-Reseller)

### AliExpress Bare-Boards
- Suche: `NT3H2111 breakout` oder `NTAG I2C breakout`
- Oft mit kleiner/keiner Antenne → geringe Reichweite
- Preis: ~1–3 €, Lieferzeit 3–4 Wochen

---

## Selber bauen (nur bei eigenem PCB sinnvoll)

- Chip: NT3H2111W0FHK (TSSOP8), ~0.80 € bei Mouser/Digi-Key
- Zusätzlich: NFC-Antenne (13,56 MHz Spule) layouten und abstimmen
- Aufwand hoch — nur lohnenswert wenn ZapBox ein eigenes PCB bekommt

---

## Geplante Firmware-Integration

Neues Modul parallel zum PN532 betreiben:

| Komponente    | Aufgabe                                               |
|---------------|-------------------------------------------------------|
| NT3H2111      | Echter NTAG21x-Tag → NFC-Feld für WoS / alle Wallets |
| PN532         | Bolt Card lesen (NFC-Reader-Modus)                    |
| ESP32 (I2C)   | Schreibt LNURL in NT3H2111 wenn sich URL ändert       |
| FD-Pin        | NT3H2111 signalisiert aktives NFC-Feld (optional)     |

Konzept-Funktion: `ntag_i2c_write_lnurl(url)` — schreibt NDEF-Record in NT3H2111 via I2C (Adresse `0x55`).

Siehe auch: [NXP-NTAG-I2C.md](NXP-NTAG-I2C.md), [WoS NFC_needs_NT3H2111.md](WoS%20NFC_needs_NT3H2111.md)
