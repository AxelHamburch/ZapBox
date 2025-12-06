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

---

## Spezial-Firmware mit Takt/Blink-Modi

### Übersicht
Erweiterte Firmware-Versionen mit konfigurierbaren Takt-/Blink-Modi für den Ausgangs-Pin anstelle des einfachen Ein/Aus-Schaltens.

### 1. Firmware-Erweiterungen (ESP32)

#### 1.1 Neue Konfigurationsvariablen
- [ ] `float frequency` - Frequenz in Hz (z.B. 0.5, 1, 2, 5 Hz)
- [ ] `float dutyCycleRatio` - Verhältnis Ein:Aus (z.B. 1:1 = 1.0, 2:1 = 2.0, 1:2 = 0.5)
- [ ] `String specialMode` - Modus-Auswahl ("standard", "blink", "pulse", "fast-blink", "custom")

#### 1.2 Config-Struktur erweitern
- [ ] JSON-Index hinzufügen für:
  - Index 11: `frequency` (Standard: 1.0 Hz)
  - Index 12: `dutyCycleRatio` (Standard: 1.0 = 1:1)
  - Index 13: `specialMode` (Standard: "standard")

#### 1.3 Neue Funktion: `executeSpecialMode()`
```cpp
void executeSpecialMode(unsigned long duration_ms, float frequency, float ratio) {
  unsigned long startTime = millis();
  unsigned long period_ms = (unsigned long)(1000.0 / frequency);
  unsigned long onTime_ms = (unsigned long)(period_ms / (1.0 + 1.0/ratio));
  unsigned long offTime_ms = period_ms - onTime_ms;
  
  while (millis() - startTime < duration_ms) {
    digitalWrite(OUTPUT_PIN, HIGH);
    delay(onTime_ms);
    digitalWrite(OUTPUT_PIN, LOW);
    delay(offTime_ms);
    if (inConfigMode) break;
  }
}
```

#### 1.4 Integration in main.cpp
- [ ] `readFiles()` erweitern um neue Parameter zu lesen
- [ ] In WebSocket-Handler prüfen ob `specialMode != "standard"`
- [ ] Bei Spezial-Modi: `executeSpecialMode()` statt einfaches `digitalWrite()` aufrufen
- [ ] Standard-Verhalten beibehalten wenn `specialMode == "standard"`

#### 1.5 Vordefinierte Modi
- [ ] **"standard"**: Normales Ein/Aus ohne Taktung (aktuelles Verhalten)
- [ ] **"blink"**: 1 Hz, 1:1 Verhältnis (0.5s ein, 0.5s aus)
- [ ] **"pulse"**: 2 Hz, 1:4 Verhältnis (kurzer Puls: 0.2s ein, 0.8s aus)
- [ ] **"fast-blink"**: 5 Hz, 1:1 Verhältnis (schnelles Blinken)
- [ ] **"custom"**: Nutzer definiert Frequenz + Verhältnis

### 2. Webinstaller-Erweiterungen (HTML/JavaScript)

#### 2.1 UI-Erweiterungen in index.html
- [ ] **Dropdown "Spezial-Version"** nach Theme:
  ```html
  <select name="specialMode" id="specialMode">
    <option value="standard" selected>Standard (kein Takten)</option>
    <option value="blink">Blink (1 Hz, 1:1)</option>
    <option value="pulse">Pulse (2 Hz, 1:4)</option>
    <option value="fast-blink">Fast Blink (5 Hz, 1:1)</option>
    <option value="custom">Custom (eigene Werte)</option>
  </select>
  ```

- [ ] **Frequenz-Eingabefeld** (nur bei "custom" sichtbar):
  ```html
  <input type="number" id="frequency" step="0.1" min="0.1" max="10" value="1.0">
  <label>Frequenz (Hz)</label>
  ```

- [ ] **Verhältnis-Eingabefeld** (nur bei "custom" sichtbar):
  ```html
  <input type="number" id="dutyCycleRatio" step="0.1" min="0.1" max="10" value="1.0">
  <label>Verhältnis Ein:Aus</label>
  ```

#### 2.2 JavaScript-Logik in assets/serial.js
- [ ] Event-Listener für `specialMode` Dropdown
- [ ] Zeige/Verstecke Frequenz+Verhältnis Felder bei "custom"
- [ ] Bei vordefinierten Modi: Setze Frequenz+Verhältnis automatisch
- [ ] Erweitere `config` Array um 3 neue Einträge (Index 11, 12, 13)
- [ ] Erweitere `writeConfig()` um neue Parameter zu senden
- [ ] Erweitere `readConfig()` um neue Parameter zu lesen

### 3. Display-Anzeigen

#### 3.1 Neue Screen-Funktion
- [ ] `specialModeScreen()` - Zeigt aktiven Modus während Taktung
  - Frequenz anzeigen
  - Verhältnis anzeigen
  - Aktuellen Status (ON/OFF)
  - Verbleibende Zeit

#### 3.2 Display.cpp erweitern
- [ ] Funktion zum Anzeigen von Takt-Parametern
- [ ] Visueller Indikator für ON/OFF Zustand (z.B. gefüllter Kreis)

### 4. Testing & Validierung

#### 4.1 Test-Szenarien
- [ ] Standard-Modus: Verhält sich wie vorher (Abwärtskompatibilität)
- [ ] Blink-Modus: 1 Hz, 1:1 → 0.5s ein, 0.5s aus
- [ ] Pulse-Modus: 2 Hz, 1:4 → 0.2s ein, 0.8s aus
- [ ] Custom-Modus: Benutzerdefinierte Werte funktionieren
- [ ] Config-Modus während Taktung: Unterbrechung funktioniert
- [ ] Lange Zeiträume: Keine Timing-Drift

#### 4.2 Edge Cases
- [ ] Frequenz = 0 Hz → Fehlerbehandlung
- [ ] Verhältnis = 0 → Fehlerbehandlung
- [ ] Sehr hohe Frequenzen (>10 Hz) → Performance-Test
- [ ] Sehr kurze Zeiten (<1s) → Mindestens ein Zyklus

### 5. Berechnungsbeispiele

**Beispiel 1: 1 Hz, Verhältnis 1:1**
- Periode = 1000ms
- Ein-Zeit = 500ms (50%)
- Aus-Zeit = 500ms (50%)

**Beispiel 2: 0.5 Hz, Verhältnis 2:1**
- Periode = 2000ms
- Ein-Zeit = 1333ms (66.6%)
- Aus-Zeit = 667ms (33.3%)
- Formel: onTime = period / (1 + 1/ratio)

**Beispiel 3: 2 Hz, Verhältnis 1:4**
- Periode = 500ms
- Ein-Zeit = 100ms (20%)
- Aus-Zeit = 400ms (80%)

### 6. Implementierungs-Reihenfolge

**Phase 1: Firmware-Grundlage**
1. Config-Struktur erweitern (Index 11-13)
2. `executeSpecialMode()` Funktion implementieren
3. In `readFiles()` neue Parameter einlesen
4. In WebSocket-Handler integrieren

**Phase 2: Webinstaller-Basis**
1. Dropdown + Input-Felder im HTML
2. JavaScript für Show/Hide Logik
3. Config-Array erweitern

**Phase 3: Vordefinierte Modi**
1. 4 Standard-Modi implementieren
2. Auto-Fill bei Auswahl

**Phase 4: Display**
1. `specialModeScreen()` erstellen
2. Status-Anzeige während Taktung

**Phase 5: Testing & Dokumentation**
1. Alle Modi testen
2. README.md aktualisieren

### 7. Offene Fragen

- [x] **Eine Firmware oder separate Builds?** → ✅ Eine Firmware, Modi per Config
- [ ] **Maximale Frequenz?** → Vorschlag: 10 Hz
- [ ] **Minimale Frequenz?** → Vorschlag: 0.1 Hz
- [ ] **Verhältnis-Limits?** → Vorschlag: 0.1 bis 10
- [ ] **Display-Update während Taktung?** → Vorschlag: Ja, alle 100-200ms

**Priorität:** Mittel  
**Status:** Planung  
**Erstellt:** 2025-12-07
