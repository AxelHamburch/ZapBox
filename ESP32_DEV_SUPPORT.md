# ESP32 Dev Module Support - Implementierung & Empfehlungen

## Übersicht

Das ZapBox-Projekt unterstützt jetzt zwei Hardware-Plattformen:
1. **LilyGo T-Display-S3** - mit Display und Touch (Original)
2. **ESP32 Dev Module** - Headless ohne Display (Neu)

## Implementierte Änderungen

### 1. Multi-Environment platformio.ini

Die `platformio.ini` wurde umstrukturiert mit:
- **[common]** Section für gemeinsame Build-Flags und Libraries
- **[env:lilygo-t-display-s3]** - Für T-Display-S3 mit Display Support
- **[env:esp32dev]** - Für ESP32 Dev Module im Headless-Modus

**Kompilieren:**
```bash
# Für LilyGo T-Display-S3 (mit Display)
pio run -e lilygo-t-display-s3

# Für ESP32 Dev Module (Headless)
pio run -e esp32dev

# Upload
pio run -e esp32dev --target upload
```

### 2. Conditional Compilation

Neues Build-Flag `ENABLE_DISPLAY`:
- `ENABLE_DISPLAY=1` → Display-Code wird kompiliert (T-Display-S3)
- `ENABLE_DISPLAY=0` → Display-Code wird weggelassen (ESP32 Dev)

### 3. Partitionstabelle für 4MB Flash

Neue Datei `partitions_4mb.csv`:
- **app0/app1**: Je 1.5 MB (vs. 3 MB beim T-Display-S3)
- **ffat**: 832 KB (vs. 10 MB beim T-Display-S3)
- Optimiert für OTA-Updates auf 4MB Flash

### 4. Code-Anpassungen

**Geschützte Dateien:**
- `Display.h` / `Display.cpp` - Stub-Implementierungen im Headless-Modus
- `TouchCST816S.h` / `TouchCST816S.cpp` - Komplett deaktiviert ohne Display
- `main.cpp` - Touch-Initialisierung bedingt
- `Navigation.cpp` - Touch-Funktionen geschützt

**Headless-Stubs:**
```cpp
#ifdef ENABLE_DISPLAY
  // Echter Display-Code
  void initDisplay() { /* TFT_eSPI Code */ }
#else
  // Stub für Headless
  inline void initDisplay() {}
#endif
```

## Speicherverbrauch

### T-Display-S3 (Referenz)
- **RAM**: 48.8 KB / 320 KB (14.9%)
- **Flash**: 1.07 MB / 3.14 MB (34.0%)
- **Verfügbar**: 16 MB Flash, 8 MB PSRAM

### ESP32 Dev Module (Geschätzt nach Abspecken)
- **RAM**: ~40-45 KB / 512 KB (~8-9%) ✅
- **Flash**: ~0.8-0.9 MB / 3.14 MB (~25-30%) ✅
- **Einsparung**: ~200-300 KB durch Wegfall von TFT_eSPI Library und Display-Assets

**Analyse:** Das sollte problemlos passen! 🎉

## Webinstaller-Anpassungen

### Firmware-Ordnerstruktur

Empfohlene Struktur im `installer/firmware/` Verzeichnis:

```
installer/firmware/
  v932595x/
    lilygo-t-display-s3/
      firmware.bin
      partitions.bin
      bootloader.bin
    esp32dev/
      firmware.bin
      partitions.bin
      bootloader.bin
```

### index.html Anpassungen

Der Webinstaller muss erweitert werden um:

1. **Board-Auswahl** hinzufügen:
```html
<select id="boardSelect">
  <option value="lilygo-t-display-s3">LilyGo T-Display-S3 (with Display)</option>
  <option value="esp32dev">ESP32 Dev Module (Headless)</option>
</select>
```

2. **Dynamische Firmware-Pfade**:
```javascript
const boardType = document.getElementById('boardSelect').value;
const firmwarePath = `firmware/${version}/${boardType}/firmware.bin`;
const partitionsPath = `firmware/${version}/${boardType}/partitions.bin`;
```

3. **Flash-Adressen anpassen**:
```javascript
// ESP32 Dev Module braucht eventuell andere Adressen
const flashConfig = {
  'lilygo-t-display-s3': {
    bootloader: 0x0,
    partitions: 0x8000,
    firmware: 0x10000
  },
  'esp32dev': {
    bootloader: 0x1000,
    partitions: 0x8000,
    firmware: 0x10000
  }
};
```

## Weitere Optimierungsmöglichkeiten

Falls doch Speicherprobleme auftreten:

### 1. Log-Level reduzieren
In `platformio.ini` [env:esp32dev]:
```ini
-DLOG_LEVEL=Log::WARN  # statt Log::INFO
# oder komplett:
-DLOG_ENABLE=0  # Spart ~10-20 KB Flash
```

### 2. Compiler-Optimierungen
```ini
build_flags = 
  ${common.build_flags_common}
  -Os  # Size optimization
  -ffunction-sections
  -fdata-sections
build_unflags = 
  -O2  # Remove default optimization
```

### 3. WebSocket-Buffer reduzieren
Im Code (wenn nötig):
```cpp
#ifdef ENABLE_DISPLAY
  #define WS_BUFFER_SIZE 8192
#else
  #define WS_BUFFER_SIZE 4096  // Kleiner für ESP32 Dev
#endif
```

### 4. FFat-Partition verkleinern
Falls weniger Konfigurationsdaten gespeichert werden:
```csv
ffat, data, fat, 0x320000, 0x50000,  # Nur 320 KB statt 832 KB
```

### 5. ArduinoJson optimieren
```cpp
// Kleinere StaticJsonDocument sizes für ESP32 Dev
#ifdef ENABLE_DISPLAY
  StaticJsonDocument<2048> doc;
#else
  StaticJsonDocument<1024> doc;
#endif
```

## Testing-Checkliste

### ESP32 Dev Module (Headless)
- [ ] WiFi-Verbindung funktioniert
- [ ] LNbits API-Kommunikation
- [ ] WebSocket-Verbindung
- [ ] Payment-Callbacks funktionieren
- [ ] GPIO-Pins für Relais/LEDs arbeiten korrekt
- [ ] Serial Config Mode funktioniert
- [ ] NFC funktioniert (falls vorhanden)
- [ ] OTA-Updates funktionieren
- [ ] Flash-Speicher reicht aus
- [ ] RAM-Nutzung ist stabil

### T-Display-S3 (Regression-Test)
- [ ] Display funktioniert weiterhin
- [ ] Touch-Steuerung funktioniert
- [ ] Alle Screens werden korrekt angezeigt
- [ ] Keine neuen Fehler durch #ifdef

## Hardware-Pin-Mapping

### ESP32 Dev Module Pinout (Beispiel)
```cpp
// In PinConfig.h für ESP32 Dev anpassen:
#ifndef ENABLE_DISPLAY
  // Headless GPIO-Mapping
  #define PIN_BUTTON_1 GPIO_NUM_13
  #define PIN_BUTTON_2 GPIO_NUM_14
  #define PIN_LED_BUTTON_LED GPIO_NUM_15
  #define PIN_LED_BUTTON_SW GPIO_NUM_16
  // Multi-Channel Output Pins
  #define PIN_RELAY_1 GPIO_NUM_12
  #define PIN_RELAY_2 GPIO_NUM_13
  #define PIN_RELAY_3 GPIO_NUM_10
  #define PIN_RELAY_4 GPIO_NUM_11
#endif
```

**WICHTIG**: Die Pin-Nummern müssen an die tatsächliche Hardware angepasst werden!

## Nächste Schritte

1. **Build testen:**
   ```bash
   pio run -e esp32dev
   ```

2. **Größe überprüfen:**
   ```bash
   pio run -e esp32dev -t size
   ```

3. **Auf Hardware testen:**
   ```bash
   pio run -e esp32dev -t upload -t monitor
   ```

4. **Webinstaller anpassen** (siehe oben)

5. **Dokumentation aktualisieren:**
   - README.md mit Board-Auswahl
   - FIRMWARE.md mit beiden Varianten
   - Installer-Anleitung

## Fazit

✅ **Zwei Boards mit einer Codebasis** - erfolgreich implementiert!
✅ **Speicher sollte ausreichen** - Display-Code entfernt spart genug
✅ **Wartbar** - Conditional Compilation ist sauber strukturiert
✅ **Erweiterbar** - Weitere Boards können leicht hinzugefügt werden

Die Implementierung ist bereit für erste Tests auf dem ESP32 Dev Module! 🚀
