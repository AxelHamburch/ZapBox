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

### 3. Git Tag erstellen und pushen

```bash
cd D:\VSCode\ZapBox
git tag v<version>
git push origin v<version>
```

Beispiel: `git tag v927726 && git push origin v927726`

### 4. Release-Zusammenfassung generieren

💡 **Copilot macht das automatisch!**  
Sage einfach: *"bitte führe jetzt release aus mit der letzten firmware version"*

Copilot wird dann:
- Git Tag erstellen und pushen
- Eine vollständige Release-Zusammenfassung erstellen mit:
  - Release Title (`ZapBox - v<version> - <Name>`)
  - Release Description (Features, Technical Improvements, Power Consumption, Installation)
  - Liste der Binary-Dateien zum Anhängen
  - Link zum GitHub Release erstellen
  - Alle Informationen fertig zum Copy & Paste

### 5. GitHub Release manuell erstellen

Nachdem Copilot die Zusammenfassung bereitgestellt hat:

#### 5.1 GitHub Repository öffnen
- Öffne den von Copilot bereitgestellten Link: `https://github.com/AxelHamburch/ZapBox/releases/new?tag=v<version>`
- Alternativ: https://github.com/AxelHamburch/ZapBox → "Releases" → "Draft a new release"

#### 5.2 Tag auswählen
- **Tag version**: Sollte bereits ausgewählt sein (z.B. `v927726`)
- **Target**: `main` (Branch)

#### 5.3 Release Title eintragen
Kopiere den von Copilot generierten Title:
- Beispiel: `ZapBox - v927726 - Savers`

#### 5.4 Release Description eintragen
Kopiere die komplette von Copilot generierte Beschreibung (Markdown-formatiert)
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

**💡 Hinweis**: Copilot generiert diese komplette Beschreibung automatisch mit allen relevanten Details!

#### 5.5 Binaries als Assets anhängen
⚠️ **WICHTIG**: Die folgenden Binaries müssen als Assets angehängt werden:

Copilot gibt dir die genauen Pfade. Beispiel für v927726:
- `bootloader.bin` (aus `installer/firmware/v927726/`)
- `partitions.bin` (aus `installer/firmware/v927726/`)
- `firmware.bin` (aus `installer/firmware/v927726/`)

**So fügst du Assets hinzu:**
1. Scrolle auf der Release-Seite nach unten zu "Attach binaries"
2. Ziehe die drei .bin Dateien in das Feld oder klicke "choose your files"
3. Wähle alle drei Dateien aus dem von Copilot angegebenen Pfad
4. Warte bis Upload abgeschlossen ist (grüner Haken erscheint)

#### 5.6 Release veröffentlichen
- ✅ **"Set as the latest release"** aktivieren (für stabile Releases)
- Bei Bedarf "Set as pre-release" aktivieren (für Beta-Versionen)
- Klicke auf **"Publish release"** (grüner Button)

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
5. Copilot fragen: "bitte führe jetzt release aus mit der letzten firmware version"
   → Copilot erstellt Git Tag und generiert komplette Release-Zusammenfassung
6. GitHub Release manuell erstellen → Copy & Paste von Copilot-Ausgabe
7. Binaries anhängen (bootloader.bin, partitions.bin, firmware.bin)
8. Release veröffentlichen → Nutzer können installieren
```

## Copilot Release-Workflow

💡 **So nutzt du Copilot für Releases:**

1. **Nach FIRMWARE.md-Schritten**: Sage zu Copilot:
   ```
   "bitte führe jetzt release aus mit der letzten firmware version"
   ```

2. **Copilot macht automatisch**:
   - ✅ Git Tag erstellen (`git tag v<version>`)
   - ✅ Tag pushen (`git push origin v<version>`)
   - ✅ Vollständige Release-Zusammenfassung generieren mit:
     - Release Title (z.B. "ZapBox - v927726 - Savers")
     - Release Description (Markdown-formatiert)
     - Features-Liste (detailliert beschrieben)
     - Technical Improvements
     - Power Consumption Comparison (Tabelle)
     - Battery Life Estimates
     - Documentation Updates
     - Installation Instructions
     - Breaking Changes
   - ✅ Liste der Binary-Dateien mit korrekten Pfaden
   - ✅ Link zum GitHub Release-Formular

3. **Du machst**:
   - Link öffnen
   - Title & Description von Copilot kopieren
   - 3 Binary-Dateien hochladen
   - "Publish release" klicken

**Vorteil**: Copilot kennt alle Details aus dem Code und erstellt eine konsistente, vollständige Beschreibung!

## Wichtige Hinweise

- **Versionsnummer**: Immer Bitcoin Block Height verwenden (z.B. v927726)
- **Branch-Reihenfolge**: Feature-Branch → main → dann erst Tag/Release
- **Release Name**: Kurz und prägnant (z.B. "Savers", "Threshold", "Colors")
- **Web Installer**: Wird automatisch aktualisiert durch HTML-Änderungen
- **Testing**: Vor dem Release auf Hardware testen!
- **Copilot nutzen**: Spart Zeit und stellt sicher, dass nichts vergessen wird!

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
