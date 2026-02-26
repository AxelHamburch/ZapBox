# ZapBox Extension – Release & Update Guide

## Überblick

Zwei Repos sind beteiligt:
- **`zapbox_extension`** – der Python-Code der LNbits Extension
- **`ZapBox`** – enthält `installer/extensions.json` mit dem SHA256-Hash des aktuellen Releases

Jedes Mal wenn `zapbox_extension` geändert wird, muss der Hash in `extensions.json` neu berechnet und hochgeladen werden.

---

## A) Bugfix / Update (gleiche Version, z. B. weiterhin v2.0.0)

### 1. Änderungen in `zapbox_extension` vornehmen und pushen

```bash
cd d:\VSCode\zapbox_extension
git add <geänderte Dateien>
git commit -m "fix: beschreibung"
git push
```

### 2. Tag löschen und neu setzen

Da sich der Inhalt des ZIPs durch den neuen Commit geändert hat, muss der Tag neu gesetzt werden, damit GitHub ein neues ZIP für denselben Tag erzeugt:

```bash
git tag -d v2.0.0                        # lokalen Tag löschen
git push origin :refs/tags/v2.0.0        # Remote-Tag löschen
git tag v2.0.0                           # neuen Tag am aktuellen Commit
git push origin v2.0.0                   # pushen
```

### 3. SHA256-Hash berechnen (PowerShell)

Kurz warten bis GitHub das ZIP gebaut hat (~3–5 Sek.), dann:

```powershell
Start-Sleep -Seconds 5
$zip = "$env:TEMP\zapbox_v2.0.0.zip"
Invoke-WebRequest -Uri "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.0.0.zip" -OutFile $zip
(Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
```

### 4. Hash in `extensions.json` eintragen

Datei: `d:\VSCode\ZapBox\installer\extensions.json`

```json
{
  "extensions": [{
    "id": "zapbox",
    "repo": "https://github.com/AxelHamburch/zapbox_extension",
    "name": "Zap⚡Box",
    "version": "2.0.0",
    "min_lnbits_version": "1.4.0",
    "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.0.0.zip",
    "hash": "<NEUER HASH>"
  }]
}
```

### 5. `extensions.json` im ZapBox-Repo committen und pushen

```bash
cd d:\VSCode\ZapBox
git add installer/extensions.json
git commit -m "fix: update extensions.json hash for zapbox_extension v2.0.0"
git push
```

### 6. `extensions.json` via SFTP hochladen

Die Datei `installer/extensions.json` auf den Webserver hochladen:

```
Ziel: /var/www/zapbox/extensions.json   (oder entsprechender Webroot)
URL:  https://installer.zapbox.space/extensions.json
```

### 7. Extension in LNbits neu installieren

In LNbits: Extension deinstallieren → neu installieren (damit die neuen statischen Dateien geladen werden).

---

## B) Neues Release (neue Version, z. B. v2.1.0)

### 1. Version hochzählen

In `zapbox_extension/config.json`:
```json
"version": "2.1.0"
```

### 2. Änderungen committen und pushen

```bash
cd d:\VSCode\zapbox_extension
git add .
git commit -m "feat: release v2.1.0 – beschreibung"
git push
```

### 3. Neuen Tag erstellen und pushen

```bash
git tag v2.1.0
git push origin v2.1.0
```

> **Kein Löschen nötig** – v2.0.0 bleibt als History erhalten.

### 4. SHA256-Hash berechnen (PowerShell)

```powershell
Start-Sleep -Seconds 5
$zip = "$env:TEMP\zapbox_v2.1.0.zip"
Invoke-WebRequest -Uri "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.1.0.zip" -OutFile $zip
(Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
```

### 5. `extensions.json` aktualisieren

`version`, `archive` und `hash` anpassen:

```json
{
  "extensions": [{
    "id": "zapbox",
    "repo": "https://github.com/AxelHamburch/zapbox_extension",
    "name": "Zap⚡Box",
    "version": "2.1.0",
    "min_lnbits_version": "1.4.0",
    "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.1.0.zip",
    "hash": "<NEUER HASH>"
  }]
}
```

### 6. Committen, pushen und hochladen

Gleich wie Schritte 5–7 aus Abschnitt A.

---

## Schnell-Checkliste

| Schritt | Bugfix (gleiche Version) | Neues Release |
|---|---|---|
| Änderungen committen & pushen | ✅ | ✅ |
| Alten Tag löschen (lokal + remote) | ✅ | ❌ (nicht nötig) |
| Neuen Tag setzen & pushen | ✅ | ✅ |
| SHA256 neu berechnen | ✅ | ✅ |
| `extensions.json` Hash aktualisieren | ✅ | ✅ |
| `extensions.json` Version + URL aktualisieren | ❌ | ✅ |
| `config.json` Version hochzählen | ❌ | ✅ |
| Via SFTP hochladen | ✅ | ✅ |
| LNbits Extension neu installieren | ✅ | ✅ |
