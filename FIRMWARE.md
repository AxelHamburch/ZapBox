# Firmware Release Process (Copilot Workflow)

**⚠️ IMPORTANT: This is THE ONLY authoritative source for firmware releases. Follow these steps exactly.**

This is an automated workflow guide for GitHub Copilot to create firmware releases.

## ⚡ Quick Start - DO THIS FIRST

**ALWAYS start by getting the current Bitcoin block height:**
```powershell
Invoke-WebRequest -Uri "https://mempool.space/api/blocks/tip/height" -UseBasicParsing | Select-Object -ExpandProperty Content
```

This becomes your version number: `vBLOCKHEIGHT` (e.g., v936746)

## 📋 Release Strategy

- **Both versions are typically released together:** Standard (T-Display-S3) AND Headless (ESP32 Dev)
- **Version format:** `vBLOCKHEIGHT` for standard, `vBLOCKHEIGHTh` for headless (note the 'h' suffix)
- **Binary files:** Copied WITHOUT any suffix (just `bootloader.bin`, `partitions.bin`, `firmware.bin`)

## Automated Release Steps

### 1. Update Version in platformio.ini

Update the VERSION flag with the block height from step above:
```ini
-DVERSION=\"v936746\"
```

### 2. Create Firmware Directories (BOTH versions)

```powershell
mkdir installer/firmware/v936746    # Standard version
mkdir installer/firmware/v936746h   # Headless version (note the 'h' suffix)
```

### 3. Create manifest.json files (BOTH versions)

**Standard version** - Create `installer/firmware/v936746/manifest.json`:
```json
{
  "name": "ZapBox",
  "version": "v936746",
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

**Headless version** - Create `installer/firmware/v936746h/manifest.json`:
```json
{
  "name": "ZapBox Headless",
  "version": "v936746h",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

**⚠️ Important differences:**
- Headless uses `"chipFamily": "ESP32"` (not ESP32-S3)
- Headless bootloader offset is `4096` (not 0)

### 4. Update Web Installer (installer/index.html) - 4 locations!

**Location 1: Standard version dropdown (~line 68)** - Add new version at TOP:
```html
<option value="./firmware/v936746/manifest.json">v936746 (Latest - Short description)</option>
```
Remove "(Latest)" from the previous top version.

**Location 2: Standard flash button (~line 90)** - Update manifest path:
```html
<esp-web-install-button id="flash-button" manifest="./firmware/v936746/manifest.json">
```

**Location 3: Headless version dropdown (~line 101)** - Add new version at TOP:
```html
<option value="./firmware/v936746h/manifest.json">v936746h (Latest - Short description)</option>
```
Remove "(Latest)" from the previous top headless version.

**Location 4: Headless flash button (~line 107)** - Update manifest path:
```html
<esp-web-install-button id="flash-button-headless" manifest="./firmware/v936746h/manifest.json">
```

### 5. Compile Firmware (BOTH versions)

**Standard version (T-Display-S3):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e lilygo-t-display-s3
```

**Headless version (ESP32 Dev):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e esp32dev
```

### 6. Copy Binary Files

**⚠️ CRITICAL: Copy files WITHOUT any suffix! Just `bootloader.bin`, `partitions.bin`, `firmware.bin`**

**Standard version (lilygo-t-display-s3) → v936746/:**
```powershell
Copy-Item -Path ".pio\build\lilygo-t-display-s3\bootloader.bin" -Destination "installer\firmware\v936746\bootloader.bin"
Copy-Item -Path ".pio\build\lilygo-t-display-s3\partitions.bin" -Destination "installer\firmware\v936746\partitions.bin"
Copy-Item -Path ".pio\build\lilygo-t-display-s3\firmware.bin" -Destination "installer\firmware\v936746\firmware.bin"
```

**Headless version (esp32dev) → v936746h/:**
```powershell
Copy-Item -Path ".pio\build\esp32dev\bootloader.bin" -Destination "installer\firmware\v936746h\bootloader.bin"
Copy-Item -Path ".pio\build\esp32dev\partitions.bin" -Destination "installer\firmware\v936746h\partitions.bin"
Copy-Item -Path ".pio\build\esp32dev\firmware.bin" -Destination "installer\firmware\v936746h\firmware.bin"
```

**Note:** The directory has the 'h' suffix (v936746h), but the filenames do NOT!

### 7. Generate Release Description

Use `git log` to get commits since last release:
```powershell
git log --oneline v936258..HEAD
```

Create a **SHORT and CONCISE** summary of all changes/commits since last firmware release:
- Group by type: Features, Bug Fixes, Visual Improvements, Technical
- 1-2 lines per change maximum
- Focus on user-visible improvements
- Include technical details only if relevant
- Write in **ENGLISH**

**Format:**
```markdown
## Changes Since v936258

### Features
- Advanced LED error diagnostic patterns for headless version
- Detection of deleted bitcoinswitch instances

### Bug Fixes  
- Fixed connection timeout handling
- Improved error recovery
```

### 8. Git Commit

```bash
git add platformio.ini installer/firmware/v936746/ installer/firmware/v936746h/ installer/index.html
git commit -m "Release v936746 & v936746h: <short description in English>"
```

### 9. Inform User

Tell user:
- Release v936746 & v936746h prepared
- Show brief changelog in English
- Provide English GitHub release description
- Next steps: Test → Tag → Push → GitHub Release

## Quick Checklist

**⚠️ Follow in this exact order:**

- [ ] **STEP 0:** Get Bitcoin block height (ALWAYS DO THIS FIRST!)
- [ ] Update platformio.ini with new version
- [ ] Create BOTH firmware directories (standard + headless with 'h' suffix)
- [ ] Create BOTH manifest.json files (note ESP32 vs ESP32-S3 differences)
- [ ] Update installer/index.html at 4 locations (2 for standard, 2 for headless)
- [ ] Compile standard firmware (lilygo-t-display-s3)
- [ ] Copy standard binary files WITHOUT suffix
- [ ] Compile headless firmware (esp32dev)
- [ ] Copy headless binary files WITHOUT suffix  
- [ ] Generate release description from git log (in English)
- [ ] Git commit (DO NOT push yet)
- [ ] Inform user with English release notes

## Common Mistakes to Avoid

1. ❌ **DON'T** add "-headless" suffix to binary filenames
2. ❌ **DON'T** forget the 'h' suffix in the headless directory name (v936746h)
3. ❌ **DON'T** use same chipFamily for both (ESP32-S3 vs ESP32)
4. ❌ **DON'T** use same bootloader offset for both (0 vs 4096)
5. ❌ **DON'T** forget to update all 4 locations in installer/index.html
6. ❌ **DON'T** look at HEADLESS_DEPLOYMENT.md (it's outdated/deleted)

## Binary File Locations

After `pio run`:
```
# Standard version (lilygo-t-display-s3):
.pio/build/lilygo-t-display-s3/
├── bootloader.bin
├── partitions.bin
└── firmware.bin

# Headless version (esp32dev):
.pio/build/esp32dev/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
```

## Final Release Structure

After completing all steps, your installer/firmware directory should look like:
```
installer/firmware/
├── v936746/                    # Standard version
│   ├── bootloader.bin          # WITHOUT -headless suffix!
│   ├── partitions.bin
│   ├── firmware.bin
│   └── manifest.json
└── v936746h/                   # Headless version (note 'h' suffix in directory)
    ├── bootloader.bin          # WITHOUT -headless suffix!
    ├── partitions.bin
    ├── firmware.bin
    └── manifest.json
```

## GitHub Release Template

Use this template for GitHub releases:
```markdown
## 🎯 Release vXXXXXX & vXXXXXXh - [Title]

### ✨ Features (Both Versions)
- Feature description

### 💡 Headless Version (vXXXXXXh) - [Special features]
- Headless-specific improvements

### 📦 Standard Version (vXXXXXX)
- Standard-specific improvements

### 🛠️ Technical Details
- Updated to Bitcoin block height XXXXXX
- Technical improvements
```

