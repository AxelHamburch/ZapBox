# NT3H2111 — NFC-Antenne

## Grundlagen

Der NT3H2111 hat **keine integrierte Antenne** — die NFC-Spule muss extern als Kupferspur in die PCB eingeätzt werden. Geometrie, Windungsanzahl und Spurabstand bestimmen Resonanzfrequenz (13,56 MHz) und Lesereichweite direkt.

Das ist der Hauptgrund warum kaum fertige Breakout-Boards existieren: Die Antenne muss auf den jeweiligen Formfaktor abgestimmt sein und kann nicht einfach universell ausgeführt werden.

## NXP Dokumentation

### AN11276 — NTAG Antenna Design Guide (offiziell)
→ https://www.nxp.com/docs/en/application-note/AN11276.zip

ZIP-Archiv mit:
- PCB-Layouts für verschiedene Formfaktoren (Kreditkartengröße, kleine Tags, etc.)
- Spulengeometrien (Windungszahl, Spurbreite, Innenabmessungen)
- Abstimmungsanleitung (Resonanzkondensatoren)
- Referenz-Footprints für verschiedene Chippackages

**→ Pflichtlektüre für eigene PCB-Entwicklung.**

### NFC Antenna Design Hub (Online-Tool)
→ https://www.nxp.com/products/rfid-nfc/nfc-hf/nfc-readers/nfc-antenna-design-hub:NFC-ANTENNA-DESIGN-TOOL

Interaktiver Rechner von NXP:
- Formfaktor (Breite × Höhe) eingeben
- Tool berechnet: Windungszahl, Spurbreite, Abstimmkondensatoren
- Export als Gerber/Footprint möglich

## Optionen für ZapBox

### Option 1: Mikroe NFC Tag 2 Click (MIKROE-2462) — Prototyping
- Antenne bereits fertig abgestimmt und integriert
- Keine eigene Antennenentwicklung nötig
- I2C-Header direkt anschließbar
- RS-Online: https://de.rs-online.com/web/p/arduino-kompatible-platinen-und-kits/1360855
- **→ Für Prototyping und erste Tests sofort einsatzbereit**

### Option 2: Eigene PCB bei JLCPCB — Serienproduktion
- XQFN-8 Package (NT3H2111W0FHKH, ~$0,77 bei LCSC) + Antennen-Footprint aus AN11276
- JLCPCB + LCSC Assembly: 5 Boards fertig bestückt für ~$5–10
- Antennengröße frei wählbar → angepasst an ZapBox-Gehäuse
- **→ Sinnvoll sobald Prototyp validiert**

### Option 3: Nackter Chip + SO-8 Breakout
- NT3H2111W0FT1X (SO-8, handlötbar) bei LCSC, ~$1,17
- Auf Standard SO-8 Breakout Board löten
- Antenne extern als Kupferspule (z.B. auf Lochraster oder FPC-Folie)
- **→ Günstigster Einstieg, aber mehr Handarbeit**

## Antennen-Richtwerte (aus NXP AN11276)

Typische Werte für einen ~25 mm × 25 mm Tag:

| Parameter | Richtwert |
|---|---|
| Windungszahl | 4–6 |
| Spurbreite | 0,3–0,5 mm |
| Spurabstand | 0,3 mm |
| Innenfläche | möglichst groß (Koppelfläche) |
| Resonanzkondensator | ~56 pF (je nach Layout zu messen) |

**Wichtig:** Endgültige Abstimmung immer mit einem NFC-Analyzer oder Smartphone messen und Kondensator anpassen.

## Weiterführende NXP App Notes

| Dokument | Inhalt |
|---|---|
| AN11276 | Antenna Design Guide (Hauptreferenz) |
| AN11579 | Bidirektionale Kommunikation (SRAM Pass-through) |
| AN11578 | Energy Harvesting mit FD-Pin |
| AN11786 | Memory Konfiguration |
| NT3H2111_2211 Datasheet | Vollständiges Datenblatt (PDF + HTML) |
