# ZapBox Extension – Release & Update Guide

## Overview

Two repositories are involved:
- **`zapbox_extension`** – the Python code of the LNbits extension
- **`ZapBox`** – contains `installer/extensions.json` with the SHA256 hash of the current release

Every time `zapbox_extension` is changed, the hash in `extensions.json` must be recalculated and re-uploaded.

---

## A) Bugfix / Update (same version, e.g. still v2.0.0)

### 1. Make changes in `zapbox_extension` and push

```bash
cd d:\VSCode\zapbox_extension
git add <changed files>
git commit -m "fix: description"
git push
```

### 2. Delete and recreate the tag

Since the ZIP content changes with every new commit, the tag must be moved so GitHub generates a fresh ZIP for the same version:

```bash
git tag -d v2.0.0                        # delete local tag
git push origin :refs/tags/v2.0.0        # delete remote tag
git tag v2.0.0                           # create tag at current commit
git push origin v2.0.0                   # push
```

### 3. Calculate SHA256 hash (PowerShell)

Wait a few seconds for GitHub to build the ZIP, then:

```powershell
Start-Sleep -Seconds 5
$zip = "$env:TEMP\zapbox_v2.0.0.zip"
Invoke-WebRequest -Uri "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.0.0.zip" -OutFile $zip
(Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
```

### 4. Update the hash in `extensions.json`

File: `d:\VSCode\ZapBox\installer\extensions.json`

> **Important:** Do **not** overwrite the old entry. **Add the new version as a new object at the top** of the array so users can choose which version to install. Old versions remain available.

```json
{
  "extensions": [
    {
      "id": "zapbox",
      "repo": "https://github.com/AxelHamburch/zapbox_extension",
      "name": "Zap⚡Box",
      "version": "2.0.0",
      "min_lnbits_version": "1.4.0",
      "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.0.0.zip",
      "hash": "<NEW HASH>"
    },
    {
      "id": "zapbox",
      "repo": "https://github.com/AxelHamburch/zapbox_extension",
      "name": "Zap⚡Box",
      "version": "<PREVIOUS VERSION>",
      "min_lnbits_version": "1.4.0",
      "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v<PREVIOUS VERSION>.zip",
      "hash": "<PREVIOUS HASH>"
    }
  ]
}
```

### 5. Commit and push `extensions.json` in the ZapBox repo

```bash
cd d:\VSCode\ZapBox
git add installer/extensions.json
git commit -m "fix: update extensions.json hash for zapbox_extension v2.0.0"
git push
```

### 6. Upload `extensions.json` via SFTP

Upload `installer/extensions.json` to the web server:

```
Target: /var/www/zapbox/extensions.json   (or the appropriate webroot)
URL:    https://installer.zapbox.space/extensions.json
```

### 7. Reinstall the extension in LNbits

In LNbits: uninstall the extension → reinstall it (so the new static files are loaded).

---

## B) New Release (new version, e.g. v2.1.0)

### 1. Bump the version number

In `zapbox_extension/config.json`:
```json
"version": "2.1.0"
```

### 2. Commit and push changes

```bash
cd d:\VSCode\zapbox_extension
git add .
git commit -m "feat: release v2.1.0 – description"
git push
```

### 3. Create and push a new tag

```bash
git tag v2.1.0
git push origin v2.1.0
```

> **No deletion needed** – v2.0.0 remains intact in history.

### 4. Calculate SHA256 hash (PowerShell)

```powershell
Start-Sleep -Seconds 5
$zip = "$env:TEMP\zapbox_v2.1.0.zip"
Invoke-WebRequest -Uri "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.1.0.zip" -OutFile $zip
(Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
```

### 5. Update `extensions.json`

Add the new version **at the top** of the array. Keep the old entry below it – do **not** delete it.

```json
{
  "extensions": [
    {
      "id": "zapbox",
      "repo": "https://github.com/AxelHamburch/zapbox_extension",
      "name": "Zap⚡Box",
      "version": "2.1.0",
      "min_lnbits_version": "1.4.0",
      "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v2.1.0.zip",
      "hash": "<NEW HASH>"
    },
    {
      "id": "zapbox",
      "repo": "https://github.com/AxelHamburch/zapbox_extension",
      "name": "Zap⚡Box",
      "version": "<PREVIOUS VERSION>",
      "min_lnbits_version": "1.4.0",
      "archive": "https://github.com/AxelHamburch/zapbox_extension/archive/refs/tags/v<PREVIOUS VERSION>.zip",
      "hash": "<PREVIOUS HASH>"
    }
  ]
}
```

### 6. Commit, push and upload

Same as steps 5–7 from section A.

---

## Quick Checklist

| Step | Bugfix (same version) | New Release |
|---|---|---|
| Commit & push changes | ✅ | ✅ |
| Delete old tag (local + remote) | ✅ | ❌ (not needed) |
| Create & push new tag | ✅ | ✅ |
| Recalculate SHA256 | ✅ | ✅ |
| Update `extensions.json` hash | ✅ | ✅ |
| Update `extensions.json` version + URL | ❌ | ✅ |
| Keep old version entry in `extensions.json` | ✅ | ✅ |
| Bump version in `config.json` | ❌ | ✅ |
| Upload via SFTP | ✅ | ✅ |
| Reinstall extension in LNbits | ✅ | ✅ |
