# Firmware v930331 - Release Preparation

## Version Information
- **Version**: v930331
- **Bitcoin Block Height**: 930331 (December 31, 2025)
- **Release Name**: "Golden Timer" (suggested)

## Changes Since v930053

### 🎨 Visual Improvements
1. **Gold Theme Color Update**
   - Changed zapbox theme from yellow (0xD600) to gold (0xFEA0)
   - Benefits:
     - More aesthetically pleasing color (#FFD700 gold)
     - More stable RGB565 bit pattern (full red channel, higher green value)
     - May improve display controller stability compared to previous yellow
   - Updated both:
     - Theme definition color value
     - Inverted QR code background color

### 🐛 Bug Fixes
2. **Navigation Race Condition Fix**
   - Fixed: NEXT button required two presses from product selection screen
   - Issue affected both Duo and Quattro modes in "when selecting" mode
   - Solution: Reset timer immediately when navigating from PRODUCT_SELECTION state
   - Prevents race condition between navigation and timeout check

3. **Bitcoin Data Retry Logic**
   - Intelligent retry for failed Bitcoin data fetches
   - Error detection: Tracks if price or block height failed to load
   - Retry intervals:
     - On error: 1 minute (faster recovery)
     - Normal: 5 minutes (standard update)
   - Improves startup experience when CoinGecko or Mempool APIs are slow/unavailable

### 📝 Technical Details
- **Files Modified**:
  - `src/Display.cpp`: Gold color implementation (lines 103, 939)
  - `src/Navigation.cpp`: Timer reset fix (line 101)
  - `src/API.cpp`: Retry logic implementation (lines 17-24, 154-161, 180-185)
  - `platformio.ini`: Version updated to v930331

## Build Instructions

### 1. Compile Firmware
```bash
# Using PlatformIO CLI
pio run

# Or VS Code: Terminal → Run Task → "PlatformIO: Build" (or press Ctrl+Alt+B)
```

### 2. Copy Binary Files
After successful build, copy these files from `.pio/build/lilygo-t-display-s3/` to `installer/firmware/v930331/`:
- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`

The `manifest.json` is already created in the target directory.

### 3. Update Web Installer
Update `installer/index.html` to add the new version:

**Line 59** - Add new option at the top of the select dropdown:
```html
<option value="./firmware/v930331/manifest.json">v930331 (Latest - Gold theme, navigation fix, BTC retry logic)</option>
```

**Line 76** - Update default manifest:
```html
<esp-web-install-button id="flash-button" manifest="./firmware/v930331/manifest.json">
```

### 4. Create Git Commit
```bash
git add platformio.ini installer/firmware/v930331/ installer/index.html
git commit -m "Release v930331: Gold theme, navigation fix, and BTC retry logic"
git push origin main
```

### 5. Create Git Tag
```bash
git tag v930331
git push origin v930331
```

## Release Notes (Copy to GitHub Release)

### Title
```
ZapBox - v930331 - Golden Timer
```

### Description
```markdown
# ZapBox v930331 - Golden Timer ⚡

This release focuses on visual improvements, bug fixes, and improved reliability for Bitcoin data fetching.

## 🎨 Visual Improvements

### Gold Theme Color
- Changed zapbox theme from yellow (0xD600) to **gold** (0xFEA0 / #FFD700)
- More aesthetically pleasing appearance
- More stable RGB565 bit pattern may improve display controller stability
- Applied to both normal and inverted QR code screens

## 🐛 Bug Fixes

### Navigation Race Condition
- **Fixed**: NEXT button now works on first press from product selection screen
- Previously required two presses in Duo/Quattro modes with "when selecting" mode
- Timer reset moved earlier to prevent race condition with timeout check

### Bitcoin Data Retry Logic
- **Improved**: Faster recovery when Bitcoin data fails to load at startup
- Intelligent retry intervals:
  - **1 minute** when data fetch errors detected (Price or Block Height = "Error")
  - **5 minutes** for normal periodic updates when data loads successfully
- Better startup experience when CoinGecko or Mempool APIs are slow/unavailable

## 📊 Technical Details

- **Previous Version**: v930053 (Code refactoring and BTC orange theme)
- **Block Height**: 930331 (December 31, 2025)
- **Platform**: ESP32-S3 (Lilygo T-Display S3)
- **Display**: ST7789 170x320, RGB565 color format

## 🔧 Installation

Flash via **Web Installer**: https://ereignishorizont.xyz/ZapBox/

### Requirements
- LILYGO T-Display-S3 board
- Chrome or Edge browser (for Web Serial API support)
- USB-C cable (data transfer capable)

## 📦 Binary Files

Download and flash manually using esptool.py:

```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**Note**: Attach these files to the release:
- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`

## 🔗 Links

- **Web Installer**: https://ereignishorizont.xyz/ZapBox/
- **Repository**: https://github.com/AxelHamburch/ZapBox
- **Documentation**: See README.md for full feature list and setup guide
- **Support**: Open an issue on GitHub

---

**Full Changelog**: https://github.com/AxelHamburch/ZapBox/compare/v930053...v930331
```

## Recommended Testing

Before creating the release, test these scenarios:

### 1. Gold Color Display
- [ ] Verify gold color appears correctly on all screens
- [ ] Check both normal and inverted QR code screens
- [ ] Confirm no display corruption during color transitions
- [ ] Test with "zapbox" theme selected in config

### 2. Navigation Fix
- [ ] Test Duo mode: From product selection → press NEXT → should show first product immediately
- [ ] Test Quattro mode: From product selection → press NEXT → should show first product immediately
- [ ] Test "when selecting" mode in both Duo and Quattro
- [ ] Verify timer no longer causes race condition

### 3. Bitcoin Data Retry
- [ ] Disconnect WiFi temporarily during startup
- [ ] Verify "Error" appears for Price/Block Height
- [ ] Reconnect WiFi
- [ ] Verify retry happens within 1 minute (not 5 minutes)
- [ ] Check logs for "[BTC] ERROR detected - will retry in 1 minute instead of 5 minutes"

### 4. General Functionality
- [ ] Payment processing works correctly
- [ ] All display modes work (single, duo, quattro)
- [ ] BTC ticker updates correctly
- [ ] Touch controls work as expected
- [ ] Config mode accessible via BOOT button

## Additional Notes

### Why Gold Color?
The gold color (0xFEA0) has several advantages over the previous yellow (0xD600):
- **Visual**: More pleasant, warmer tone (#FFD700 is classic web gold)
- **Technical**: Full red channel (31/31) vs. partial (26/31) - more stable bit pattern
- **Stability**: Similar bit pattern to orange (0xFCC0) which never had display issues

### Commit History Since v930053
1. `61057a0` - feat: Change zapbox theme color from yellow to gold
2. `498c60e` - fix: Improve navigation timing and Bitcoin data retry logic

## Release Checklist

- [ ] Firmware compiled successfully
- [ ] Binary files copied to `installer/firmware/v930331/`
- [ ] `manifest.json` created
- [ ] `installer/index.html` updated with new version
- [ ] All changes committed to main branch
- [ ] Git tag created and pushed
- [ ] Testing completed (see Recommended Testing section)
- [ ] GitHub release draft created with title and description
- [ ] Binary files attached to GitHub release
- [ ] Release published

## Support Information

If issues are encountered:
1. Check serial monitor output for error messages
2. Look for lines containing `[BTC]`, `[Navigation]`, or `[Display]`
3. Report issues on GitHub with:
   - Firmware version (v930331)
   - Hardware (T-Display S3)
   - Config mode settings (mode, theme, btc ticker mode)
   - Serial log output

---

**Date Prepared**: December 31, 2025
**Bitcoin Block Height**: 930331
**Prepared By**: GitHub Copilot + User
