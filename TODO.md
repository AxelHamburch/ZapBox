# ZapBox TODO

## Serial Communication Improvements

### Problem: Chaotische serielle Ausgabe im Web Installer
Die serielle Kommunikation zwischen ESP32 und Browser zeigt unvollständige/geteilte Zeilen:
- Text wird mitten im Wort getrennt (z.B. "eceived:" statt "received:")
- ASCII-Art (ZAPBOX Logo) wird zerteilt dargestellt
- Zeilen erscheinen fragmentiert im Terminal-Fenster

**Ursache:**
- Asynchrone serielle Übertragung sendet Daten in variablen Chunks
- Browser Serial API empfängt und verarbeitet jeden Chunk sofort
- Keine Pufferung auf vollständige Zeilen

**Mögliche Lösungen:**
1. **Line-Buffering in JavaScript implementieren**
   - Sammle eingehende Zeichen bis `\n` (Newline) erkannt wird
   - Erst dann die komplette Zeile verarbeiten und anzeigen
   - Verhindert geteilte Wörter und unleserliche Ausgabe

2. **ESP32 Serial-Ausgabe optimieren**
   - Größere Serial Buffer verwenden
   - Explizite `Serial.flush()` Aufrufe nach wichtigen Ausgaben
   - Ausgaben in einzelne `Serial.println()` zusammenfassen

3. **Baudrate-Test**
   - Aktuell: 115200 baud
   - Testen ob niedrigere Rate (z.B. 57600) stabilere Übertragung bringt
   - Wahrscheinlich nicht das Hauptproblem

**Priorität:** Niedrig (kosmetisch - Funktionalität ist gegeben)

**Status:** Offen

---

## Notizen
- JSON-Extraktion funktioniert trotz chaotischer Ausgabe zuverlässig
- Config Read/Write Funktionen arbeiten korrekt
- Problem betrifft nur die visuelle Darstellung im Terminal-Fenster
