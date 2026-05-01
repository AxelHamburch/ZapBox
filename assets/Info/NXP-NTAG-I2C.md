# NXP NT3H2111 / NT3H2211 (NTAG I2C)

## Hintergrund

Der PN532 kann Type 2 Tag (NTAG21x) **nicht** emulieren, weil `GET_VERSION` (0x60) identisch mit dem Mifare `AUTH_A`-Opcode ist. Der PN532 fängt `0x60` intern ab und antwortet mit einer Mifare-Auth-Challenge — das Handy erwartet eine GET_VERSION-Antwort, versteht sie nicht und sendet sofort HLTA (Deselect). PN532 Error `0x25` = "Target released". ISO-DEP (SAK=0x20, Type 4 Tag) ist der einzige PN532-Modus mit transparentem APDU-Datendurchsatz.

## Lösung: NT3H2111 als Brücke

Kein Emulations-Chip — ein **echter NTAG21x-kompatibler NFC-Chip** mit zusätzlicher I2C-Schnittstelle zur Host-CPU.

```
ESP32 (I2C Master)          NT3H2111 (NTAG I2C)          Smartphone
──────────────────          ───────────────────          ──────────
LNURL schreiben  ─── I2C ─► NFC-Speicher
                             (echter NTAG213)  ─── NFC ─► liest NTAG21x
                                                          GET_VERSION ✓
                                                          READ ✓
                                                          FAST_READ ✓
```

- Das Handy sieht einen **echten NTAG213/216** — kein Protokoll-Problem, kein `0x60`-Konflikt
- Wallet of Satoshi erkennt SAK=`0x00` → behandelt es als normales NDEF-Tag → **WoS funktioniert**
- ESP32 schreibt die LNURL einfach via I2C in den Chip-Speicher

## Chip-Eigenschaften

| Eigenschaft | NT3H2111 | NT3H2211 |
|---|---|---|
| NFC-Protokoll | ISO 14443-3A (Type 2 Tag / NFC-A) | ISO 14443-3A (Type 2 Tag / NFC-A) |
| NFC-Speicher | 1 kBit (104 Byte Nutzdaten) | 2 kBit (232 Byte Nutzdaten) |
| I2C-Adresse | 0x55 (konfigurierbar via Registerblock) | 0x55 (konfigurierbar) |
| Versorgungsspannung | 1.8 – 3.6 V | 1.8 – 3.6 V |
| Formfaktor | TSSOP8, SOT23, Antenne-on-chip | TSSOP8, SOT23, Antenne-on-chip |
| Energy Harvesting | ✓ (FD-Pin, bis 5 mA aus NFC-Feld) | ✓ |
| NDEF-Speicher ausreichend | Ja (LNURL bech32 ≈ 130–150 Byte) | Ja (mit Reserve) |

## Integration ins ZapBox

### I2C-Bus
- NT3H2111 I2C-Adresse: `0x55`
- PN532 I2C-Adresse: `0x24`
- → Beide auf demselben Bus (SDA=GPIO18, SCL=GPIO17) — **kein Konflikt**

### FD-Pin (Field Detected)
- Geht HIGH wenn ein NFC-Feld erkannt wird (Handy nähert sich)
- Geht LOW wenn das NFC-Feld weg ist
- → ESP32 kann darauf reagieren (z.B. I2C-Schreibzugriffe während NFC-Session verhindern)
- An freien GPIO anschließen, z.B. GPIO2 (wenn Lichtschranke deaktiviert)

### Arbitration (I2C ↔ NFC gleichzeitig)
- NT3H2111 hat internes Arbitrations-Schiedsverfahren: I2C-Zugriff wird blockiert solange NFC-Session läuft
- ESP32 muss auf FD=LOW warten bevor er schreibt (oder `NSSACK`-Bit im Session-Register prüfen)
- Für ZapBox unkritisch: LNURL ändert sich nur bei Produktwechsel, nicht während eines Taps

### Firmware-Änderung (Konzept)
```cpp
// Beim Produktwechsel (URL geändert):
void ntag_i2c_write_lnurl(const String &lnurl) {
    // NDEF TLV in Seitenspeicher (ab Page 4) schreiben
    // Page 3 = Capability Container E1 10 6D 00 (vorher einmalig schreiben)
    uint8_t ndefData[104];
    buildNdefTlv(lnurl, ndefData);
    // via Wire.write() auf I2C-Adresse 0x55, Block-Schreibzugriff
    ntag_i2c_write_pages(4, ndefData, sizeof(ndefData));
}
```

## Verfügbare Breakout-Boards

| Board | Chip | Antenne | Preis (ca.) |
|---|---|---|---|
| Mikroe "NFC Tag 2 Click" | NT3H2111 | integriert | ~8 € |
| DFRobot Gravity NFC Tag | NT3H2111 | integriert | ~5 € |
| AliExpress Bare-Boards | NT3H2111 | extern (Spule löten) | ~2 € |

## Weitere Optionen (Alternative Chips)

| Chip | Typ | Besonderheit |
|---|---|---|
| **ST25DV04K** (STMicro) | Dynamic NFC/RFID Tag | I2C + GPO-Pin + Energy Harvesting, günstig |
| **M24SR64-Y** (STMicro) | Type 4 Tag + I2C | ISO 14443-4, für ISO-DEP falls gewünscht |
| **AS3953** (ams) | Type 2/4 Tag emulierbar | Echter Emulations-Controller, komplexer |

## Nächste Schritte (wenn umgesetzt)

1. NT3H2111 Breakout-Board bestellen
2. An I2C-Bus anschließen (SDA=18, SCL=17, FD=frei wählbar)
3. Capability Container einmalig schreiben (Page 3: `E1 10 6D 00`)
4. Neue Firmware-Funktion: `ntag_i2c_write_lnurl()` die bei Produktwechsel aufgerufen wird
5. PN532-Emulation-Modus wird damit komplett hinfällig für NFC-Tap-Funktion
6. PN532 bleibt nur noch für Bolt Card Reader-Modus aktiv
