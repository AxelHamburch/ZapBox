# Firmware Release Process (Copilot Workflow)

This is an automated workflow guide for GitHub Copilot to create firmware releases.

## Version Numbering

Use Bitcoin block height as version number: `vBLOCKHEIGHT`

**Get current block height:**
```powershell
Invoke-WebRequest -Uri "https://mempool.space/api/blocks/tip/height" -UseBasicParsing | Select-Object -ExpandProperty Content
```

## Automated Release Steps

### 1. Update Version in platformio.ini

Update the VERSION flag:
```ini
-DVERSION=\"v930331\"
```

### 2. Create Firmware Directory

```powershell
mkdir installer/firmware/v930331
```

### 3. Create manifest.json

Create `installer/firmware/v930331/manifest.json`:
```json
{
  "name": "ZapBox",
  "version": "v930331",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

### 4. Update Web Installer (installer/index.html)

**Line ~59** - Add new version option at top:
```html
<option value="./firmware/v930331/manifest.json">v930331 (Latest - Short description)</option>
```
Remove "(Latest)" from previous version.

**Line ~76** - Update flash button manifest:
```html
<esp-web-install-button id="flash-button" manifest="./firmware/v930331/manifest.json">
```

### 5. Compile Firmware

```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run
```

### 6. Copy Binary Files

```powershell
Copy-Item -Path ".pio\build\lilygo-t-display-s3\bootloader.bin" -Destination "installer\firmware\v930331\bootloader.bin"
Copy-Item -Path ".pio\build\lilygo-t-display-s3\partitions.bin" -Destination "installer\firmware\v930331\partitions.bin"
Copy-Item -Path ".pio\build\lilygo-t-display-s3\firmware.bin" -Destination "installer\firmware\v930331\firmware.bin"
```

### 7. Generate Release Description

Use `git log` to get commits since last release:
```powershell
git log --oneline v930053..HEAD
```

Create a **SHORT and CONCISE** summary of all changes/commits since last firmware release:
- Group by type: Visual, Bug Fixes, Features, Technical
- 1-2 lines per change maximum
- Focus on user-visible improvements
- Include technical details only if relevant

**Format:**
```
## Changes Since v930053

### Visual Improvements
- Changed zapbox theme to gold color (0xFEA0)

### Bug Fixes  
- Fixed NEXT button race condition in Duo/Quattro modes
- Improved Bitcoin data retry (1min on error vs 5min)
```

### 8. Git Commit

```bash
git add platformio.ini installer/firmware/v930331/ installer/index.html
git commit -m "Release v930331: <short description>"
```

### 9. Inform User

Tell user:
- Release v930331 prepared
- Show brief changelog
- Next steps: Test → Tag → Push → GitHub Release

## Quick Checklist

- [ ] Get block height
- [ ] Update platformio.ini
- [ ] Create firmware directory
- [ ] Create manifest.json
- [ ] Update installer/index.html (2 locations)
- [ ] Compile firmware
- [ ] Copy 3 binary files
- [ ] Generate release description from git log
- [ ] Git commit
- [ ] Inform user

## Binary File Locations

After `pio run`:
```
.pio/build/lilygo-t-display-s3/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
```

