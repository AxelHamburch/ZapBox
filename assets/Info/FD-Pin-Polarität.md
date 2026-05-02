# NT3H2111 FD-Pin (Field Detected) — GPIO-Auswahl & Polarität

## Was ist der FD-Pin?

Der FD-Pin ist ein Hardware-Ausgang des NT3H2111-Chips:
- **HIGH** wenn ein NFC-Feld erkannt wird (Smartphone nähert sich)
- **LOW** wenn das NFC-Feld weg ist (Normalzustand)

Der ESP32 liest diesen Pin als Eingang — der NT3H2111 treibt ihn aktiv.

## Open-Drain-Ausgang (wichtig!)

Der FD-Pin ist ein **Open-Drain-Ausgang**:
- Kein NFC-Feld (Idle) → FD zieht aktiv auf **GND**
- NFC-Feld vorhanden → FD lässt los (High-Z)

Das bedeutet: Ohne externen oder internen Pull-up bleibt der Pin im NFC-Zustand undefiniert.

## GPIO3 — Analyse

GPIO3 ist ein **Strapping-Pin** des ESP32-S3 mit internem Pull-up (→ HIGH beim Boot = JTAG via GPIO-Matrix).

**Problem beim Boot ohne NFC-Feld:**
Der FD-Pin zieht aktiv auf GND (Open-Drain LOW) → kämpft gegen den internen Pull-up → GPIO3 könnte auf LOW gezogen werden → falsches Strapping.

**Lösung: Vorwiderstand 10 kΩ in Serie**

```
NT3H2111 FD ──[ 10kΩ ]── GPIO3 (ESP32-S3)
                               ↑ interner Pull-up dominiert beim Boot,
                                 Widerstand begrenzt FD-Strom
```

Damit bleibt der Strapping-Zustand beim Boot definiert (HIGH), und im Normalbetrieb signalisiert der FD-Pin trotzdem zuverlässig.

## Alternative: FD-Pin-Polarität invertieren

Im NT3H2111 Konfigurationsregister lässt sich die FD-Pin-Polarität softwareseitig invertieren:
- FD **HIGH** im Idle (kein NFC-Feld)
- FD **LOW** bei erkanntem Feld

→ Der interne Pull-up von GPIO3 dominiert im Boot-Moment ohne externen Widerstand.
→ Erfordert einen **einmaligen I2C-Konfigurationsschreibzugriff** beim ersten Inbetriebnahme-Setup.

## Empfehlung für ZapBox

| Option | Aufwand | Risiko |
|--------|---------|--------|
| GPIO3 + 10 kΩ Vorwiderstand | minimal (1 Bauteil) | sicher |
| GPIO3 + Polarität invertiert (I2C) | Firmware-Setup nötig | sicher, kein Bauteil |
| Nicht-Strapping GPIO (z.B. GPIO38/39) | kein Aufwand | kein Risiko |

Für eine saubere PCB-Lösung: **10 kΩ Vorwiderstand + GPIO3**.
Für schnelles Prototyping: freien Nicht-Strapping GPIO wählen.
