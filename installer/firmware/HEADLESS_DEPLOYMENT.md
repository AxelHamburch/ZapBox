# ZapBox Headless - Firmware Deployment Guide

## Firmware-Dateien für Webinstaller vorbereiten

### 1. Firmware builden

```powershell
# ESP32 Dev (Headless) Version builden
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e esp32dev
```

### 2. Firmware-Dateien kopieren

Nach erfolgreichem Build finden Sie die Dateien in `.pio\build\esp32dev\`:

```
.pio\build\esp32dev\bootloader.bin     → bootloader-headless.bin
.pio\build\esp32dev\partitions.bin     → partitions-headless.bin
.pio\build\esp32dev\firmware.bin       → firmware-headless.bin
```

### 3. In Installer-Verzeichnis kopieren

Kopieren Sie die drei Dateien nach `installer\firmware\v930750\` (oder die aktuelle Version):

```powershell
# Beispiel für v930750h
$version = "v930750"
$buildDir = ".pio\build\esp32dev"
$targetDir = "installer\firmware\$version"

Copy-Item "$buildDir\bootloader.bin" "$targetDir\bootloader-headless.bin"
Copy-Item "$buildDir\partitions.bin" "$targetDir\partitions-headless.bin"
Copy-Item "$buildDir\firmware.bin" "$targetDir\firmware-headless.bin"
```

### 4. Manifest bereits erstellt

Die Datei `manifest-headless.json` ist bereits im Verzeichnis vorhanden und referenziert die headless-Dateien.

### Versionshinweise

- **T-Display-S3**: Normale Version ohne Suffix (z.B. `v930750`)
- **ESP32 Dev Headless**: Version mit "h" Suffix (z.B. `v930750h`)
- Beide Versionen können im gleichen Verzeichnis liegen
- Separate Manifest-Dateien (`manifest.json` und `manifest-headless.json`)

### Unterschiede zwischen den Versionen

| Feature | T-Display-S3 | ESP32 Dev Headless |
|---------|-------------|-------------------|
| Display | ✅ ST7789 TFT | ❌ Nicht vorhanden |
| Touch | ✅ CST816S (optional) | ❌ Nicht vorhanden |
| Status LED | GPIO 43 (optional) | GPIO 21 |
| Flash Size | 16MB | 4MB |
| Partition Table | partitions_16mb.csv | partitions_4mb.csv |
| ChipFamily | ESP32-S3 | ESP32 |
| Bootloader Offset | 0x0000 | 0x1000 |

### Bootloader Offset Unterschied

**Wichtig:** ESP32 und ESP32-S3 haben unterschiedliche Bootloader Offsets:

- **ESP32-S3**: Offset `0` (0x0000) 
- **ESP32**: Offset `4096` (0x1000)

Das manifest-headless.json ist bereits entsprechend konfiguriert.

### Status LED Blinkmuster (Headless Version)

Die Headless Version nutzt die Status-LED auf GPIO 21 (und GPIO 2 als zusätzliche LED) für visuelle Statusrückmeldungen:

#### Normale Betriebszustände
- **3x sehr schnell blinken**: Bootvorgang (unmittelbar nach Power-On)
- **Schnelles Blinken (5Hz, 200ms)**: Initialisierung und WiFi-Verbindungsaufbau
- **Langsames Blinken (1Hz, 1000ms)**: Konfigurationsmodus aktiv
- **Dauerlicht**: Gerät ist betriebsbereit (READY) und kann Zahlungen empfangen
- **Aus**: Tiefschlaf, Help/Report-Modi

#### Fehlerzustands-Blinkmuster
Bei Netzwerkfehlern zeigt die LED spezifische Blinkmuster mit **2 Sekunden Pause** zwischen den Sequenzen:

| Fehler | Blinkmuster | Bedeutung |
|--------|-------------|-----------|
| **1 Blink** | 1x Blinken (500ms an, 500ms aus) | NO WIFI - WiFi-Verbindung verloren/nicht hergestellt |
| **2 Blinks** | 2x Blinken (je 300ms an/aus) | NO INTERNET - WiFi OK, aber kein Internet-Zugang |
| **3 Blinks** | 3x Blinken (je 250ms an/aus) | NO SERVER - Internet OK, aber LNbits-Server nicht erreichbar |
| **4 Blinks** | 4x Blinken (je 200ms an/aus) | NO WEBSOCKET - Server OK, aber WebSocket-Verbindung fehlgeschlagen |

**Fehlererkennung Priorität:** Die LED zeigt immer den ersten nicht bestätigten Netzwerkstatus in der Reihenfolge: WiFi → Internet → Server → WebSocket.
