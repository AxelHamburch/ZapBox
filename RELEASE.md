# GitHub Release Anleitung

Diese Anleitung beschreibt, wie ein neues GitHub Release mit Tag erstellt wird, nachdem die Firmware vorbereitet wurde.

## Voraussetzungen

⚠️ **WICHTIG**: Ein GitHub Release mit Tag darf erst erstellt werden, nachdem der Feature-Branch in `main` gemerged wurde!

## Schritt-für-Schritt Anleitung

### 1. Firmware vorbereiten (FIRMWARE.md)

Folge zuerst der kompletten Anleitung in [FIRMWARE.md](FIRMWARE.md):
- Version in `platformio.ini` aktualisieren
- Firmware kompilieren
- Binaries kopieren
- Manifest erstellen
- Web Installer HTML aktualisieren
- Git Commit erstellen

### 2. Branch in main mergen

Nachdem alle Firmware-Schritte abgeschlossen sind:

```bash
# Aktuellen Feature-Branch pushen
git push origin <feature-branch-name>

# Zu main wechseln
git checkout main

# main aktualisieren
git pull origin main

# Feature-Branch in main mergen
git merge <feature-branch-name>

# main pushen
git push origin main
```

### 3. GitHub Release erstellen

Nachdem der Merge in `main` abgeschlossen ist:

#### 3.1 GitHub Repository öffnen
- Gehe zu: https://github.com/AxelHamburch/ZapBox
- Klicke auf "Releases" (rechte Seitenleiste)

#### 3.2 Neuen Release erstellen
- Klicke auf "Draft a new release"

#### 3.3 Tag erstellen
- **Tag version**: `v927726` (Bitcoin Block Height als Version)
- **Target**: `main` (Branch auswählen)
- **Release title**: `ZapBox - v927726 - <Kurzer Name>`
  - Beispiel: "ZapBox - v927726 - Savers"

#### 3.4 Release Notes schreiben
```markdown
# ZapBox v927726 - <Release Name>

## Features
- Feature 1 Beschreibung
- Feature 2 Beschreibung
- Feature 3 Beschreibung

## Technical Improvements
- Technische Verbesserung 1
- Technische Verbesserung 2

## Installation
Flash via Web Installer: https://ereignishorizont.xyz/ZapBox/

## Power Consumption (falls relevant)
| Mode | Consumption | Use Case |
|------|-------------|----------|
| Normal | 150-250mA | Active display |
| Screensaver | 40-60mA | Backlight off |
| Light Sleep | 3-5mA | WiFi active |
| Deep Sleep | 0.01-0.15mA | Maximum battery |

## Changes
See full commit history for details.
```

#### 3.5 Optional: Binaries anhängen
Falls gewünscht, können die kompilierten Binaries als Assets angehängt werden:
- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`

Diese sind aber bereits im Web Installer verfügbar, daher optional.

#### 3.6 Release veröffentlichen
- Bei Bedarf "Set as pre-release" aktivieren (für Beta-Versionen)
- "Set as latest release" aktivieren (für stabile Releases)
- Klicke auf "Publish release"

## Beispiel für Release v927726 "Savers"

**Tag**: `v927726`  
**Target**: `main`  
**Title**: `ZapBox - v927726 - Savers`

**Release Notes**:
```markdown
# ZapBox v927726 - Savers

Add power saving modes with screensaver and deep sleep functionality.

## Features
- **Screensaver mode**: Backlight off, ~80-90% power saving
- **Deep Sleep Light mode**: ~3-5mA consumption, WiFi stays active for payments
- **Deep Sleep Freeze mode**: ~0.01-0.15mA consumption, maximum battery life
- **Boot-Up screen**: Wake status indication after deep sleep
- **RTC GPIO wake-up**: Both BOOT and HELP buttons wake the device
- **Power Saving status display**: Shows mode and recommendations at startup
- **Configurable timeout**: 1-120 minutes activation time
- **Mutual exclusion**: Only one power saving mode active at a time
- **Grace period**: 5-second prevention of accidental button triggers
- **WiFi reconnection**: Automatic reconnection after wake-up

## Technical Improvements
- RTC GPIO functions for ESP32-S3 deep sleep freeze mode
- EXT0/EXT1 wake-up configuration for both buttons (GPIO 0 and GPIO 14)
- Watchdog management during sleep preparation
- Power domain optimization (RTC_PERIPH ON, SLOW/FAST_MEM OFF)
- WiFi power save mode for light sleep

## Power Consumption Comparison
| Mode | Consumption | WiFi | Wake-up Time | Use Case |
|------|-------------|------|--------------|----------|
| Normal | 150-250mA | ✅ Active | - | Active display |
| Screensaver | 40-60mA | ✅ Active | Instant | Short pauses |
| Light Sleep | 3-5mA | ✅ Active | ~1-2s | Longer breaks, payments possible |
| Deep Sleep Freeze | 0.01-0.15mA | ❌ Off | ~3-5s | Maximum battery life |

## Battery Life Estimates (with 10000mAh battery)
- Normal mode: ~40-67 hours
- Screensaver mode: ~167-250 hours (7-10 days)
- Light Sleep mode: ~2000-3333 hours (83-139 days)
- Deep Sleep Freeze mode: ~66,000-1,000,000 hours (7.5-114 years!)

## Documentation
- Added FIRMWARE.md: Complete firmware release process guide
- Updated README.md: Power consumption comparison table

## Installation
Flash via Web Installer: https://ereignishorizont.xyz/ZapBox/

Select **v927726 (Latest)** from the version dropdown.

## Breaking Changes
None - fully backward compatible with previous configurations.
```

## Workflow Zusammenfassung

```
1. Feature entwickeln → Feature-Branch
2. FIRMWARE.md Schritte ausführen → Firmware vorbereiten
3. Feature-Branch committen & pushen
4. Feature-Branch in main mergen → PR oder direkter Merge
5. GitHub Release erstellen → Tag & Release Notes
6. Release veröffentlichen → Nutzer können installieren
```

## Wichtige Hinweise

- **Versionsnummer**: Immer Bitcoin Block Height verwenden (z.B. v927726)
- **Branch-Reihenfolge**: Feature-Branch → main → dann erst Tag/Release
- **Release Name**: Kurz und prägnant (z.B. "Savers", "Threshold", "Colors")
- **Web Installer**: Wird automatisch aktualisiert durch HTML-Änderungen
- **Testing**: Vor dem Release auf Hardware testen!

## Troubleshooting

### "Tag already exists"
Der Tag wurde bereits erstellt. Tags können nicht überschrieben werden. Entweder:
- Tag löschen: `git tag -d v927726 && git push origin :refs/tags/v927726`
- Neue Versionsnummer verwenden

### "Release not showing in Web Installer"
- Prüfe ob `installer/index.html` korrekt aktualisiert wurde
- Cache im Browser löschen
- Manifest-Pfad in HTML prüfen

### "Binaries not found"
- Prüfe ob Binaries in `installer/firmware/v927726/` vorhanden sind
- Prüfe ob Git LFS aktiviert ist (falls verwendet)
- Manifest.json Pfade überprüfen
