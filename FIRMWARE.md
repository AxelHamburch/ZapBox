# Firmware Release Process

This guide describes how to create and publish a new firmware version for the ZapBox.

## Version Numbering

ZapBox uses Bitcoin block height as version numbers to create a timestamp reference and celebrate the Bitcoin network.

**Format:** `vBLOCKHEIGHT[-suffix]`
- Example: `v927292` (block height 927292)
- With suffix: `v927292-pre` (pre-release), `v927292-beta`, etc.

**Get current block height:**
- Visit: https://mempool.space/api/blocks/tip/height
- Or check: https://mempool.space/ (displayed on homepage)

## Step-by-Step Release Process

### 1. Update Version in `platformio.ini`

Open `platformio.ini` and update the `VERSION` flag:

```ini
build_flags = 
    -DVERSION=\"v927292\"
```

Replace `927292` with the current block height from mempool.space.

### 2. Compile Firmware

Build the firmware to generate binary files:

```bash
# Using PlatformIO CLI
pio run

# Or use VS Code: PlatformIO: Build (Ctrl+Alt+B)
```

This creates binary files in `.pio/build/lilygo-t-display-s3/`:
- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`

### 3. Create Firmware Directory

Create a new folder under `installer/firmware/` with the version number:

```bash
mkdir installer/firmware/v927292
```

### 4. Copy Binary Files

Copy the three binary files from `.pio/build/lilygo-t-display-s3/` to the new firmware folder:

```bash
cp .pio/build/lilygo-t-display-s3/bootloader.bin installer/firmware/v927292/
cp .pio/build/lilygo-t-display-s3/partitions.bin installer/firmware/v927292/
cp .pio/build/lilygo-t-display-s3/firmware.bin installer/firmware/v927292/
```

**Important paths in `.pio/`:**
- Bootloader: `.pio/build/lilygo-t-display-s3/bootloader.bin`
- Partitions: `.pio/build/lilygo-t-display-s3/partitions.bin`
- Firmware: `.pio/build/lilygo-t-display-s3/firmware.bin`

### 5. Create `manifest.json`

Create `installer/firmware/v927292/manifest.json` with the following structure:

```json
{
  "name": "ZapBox",
  "version": "v927292",
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

**Important:** Update the `version` field with your new version number.

### 6. Update Web Installer (`installer/index.html`)

Add the new version to the firmware selector dropdown:

```html
<select id="firmware-version">
    <!-- Add new version at the top with (Latest) -->
    <option value="./firmware/v927292/manifest.json">v927292 (Latest - Your feature description)</option>
    
    <!-- Remove (Latest) from previous version -->
    <option value="./firmware/v926800/manifest.json">v926800 (Add options for color and special mode)</option>
    
    <!-- Keep older versions as-is -->
    <option value="./firmware/v926705/manifest.json">v926705 (Add Threshold Mode)</option>
    ...
</select>
```

**Also update the flash button manifest:**

```html
<esp-web-install-button id="flash-button" manifest="./firmware/v927292/manifest.json">
```

### 7. Version Description

**If unclear what to write as version description:**
- Check recent commits: `git log --oneline -10`
- Review TODO.md for completed features
- Review recent conversation/chat history
- **ASK THE USER** for a short description (preferred!)

**Examples of good descriptions:**
- "Add screensaver and deep sleep power saving"
- "Fix WiFi reconnection issues"
- "Add threshold mode with wallet monitoring"
- "Improve error handling and diagnostics"

### 8. Verify Installation Structure

Final structure should look like this:

```
installer/firmware/
├── v926531/
│   ├── bootloader.bin
│   ├── firmware.bin
│   ├── manifest.json
│   └── partitions.bin
├── v926561/
│   └── ...
├── v927292/          ← NEW VERSION
│   ├── bootloader.bin
│   ├── firmware.bin
│   ├── manifest.json
│   └── partitions.bin
```

### 9. Test the Installation

1. Open `installer/index.html` in a web browser (Chrome/Edge)
2. Verify new version appears in dropdown
3. Verify "Latest" tag is on new version only
4. Test flash process with a device
5. Verify device boots and shows correct version

### 10. Commit and Push

```bash
git add platformio.ini
git add installer/firmware/v927292/
git add installer/index.html
git commit -m "Release v927292: <feature description>"
git push
```

## Quick Reference

| Step | Command/File | Description |
|------|-------------|-------------|
| 1 | `platformio.ini` | Update `-DVERSION` |
| 2 | `pio run` | Compile firmware |
| 3 | `mkdir installer/firmware/vXXXXXX` | Create version folder |
| 4 | Copy from `.pio/build/lilygo-t-display-s3/` | Copy 3 binary files |
| 5 | `installer/firmware/vXXXXXX/manifest.json` | Create manifest |
| 6 | `installer/index.html` | Update version selector |
| 7 | Ask user if needed | Get version description |
| 8 | Test | Flash and verify |

## Binary File Locations

After running `pio run`, find binaries at:

```
.pio/build/lilygo-t-display-s3/
├── bootloader.bin    (offset: 0)
├── partitions.bin    (offset: 32768 / 0x8000)
└── firmware.bin      (offset: 65536 / 0x10000)
```

These offsets are specified in the `manifest.json` file.

## Troubleshooting

**Build fails:**
- Check `platformio.ini` syntax
- Verify all dependencies installed
- Try clean build: `pio run --target clean`

**Binary files missing:**
- Run `pio run` first
- Check `.pio/build/lilygo-t-display-s3/` directory
- Verify partition table: `partitions_16mb.csv`

**Web installer doesn't show new version:**
- Clear browser cache
- Check `manifest.json` path in HTML
- Verify JSON syntax (use jsonlint.com)

**Flash fails:**
- Verify all 3 binary files present
- Check offsets match manifest
- Try "Erase device" first

## Version History Format

When updating `index.html`, maintain this format:

```html
<option value="./firmware/vXXXXXX/manifest.json">vXXXXXX (Latest - Short description)</option>
<option value="./firmware/vYYYYYY/manifest.json">vYYYYYY (Feature description without Latest)</option>
```

Keep descriptions concise (~50-80 characters) and focus on the main feature/fix.
