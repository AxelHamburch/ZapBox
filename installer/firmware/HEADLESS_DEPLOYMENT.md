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
