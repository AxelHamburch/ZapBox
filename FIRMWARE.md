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

- **Hardware Variants:**
  - **Standard** (T-Display-S3): `vBLOCKHEIGHT` format
  - **Headless** (ESP32 Dev): `vBLOCKHEIGHTh` suffix (note the 'h')
  - **Touch 3.5"** (JC3248W535C): `vBLOCKHEIGHTt` suffix (note the 't')
  - **ESP32-C3-21-1** (ESP32-C3-WROOM-02): `vBLOCKHEIGHTc` suffix (note the 'c')
- **Release Approach:** Versions can be released individually (e.g., touch3.5 only) or as a group
- **Binary files:** Copied WITHOUT any suffix (just `bootloader.bin`, `partitions.bin`, `firmware.bin`)

### ⚠️ ESP32-C3-21-1: Only release when explicitly requested!

**Do NOT automatically include the ESP32-C3-21-1 (`vBLOCKHEIGHTc`) in every release.**
The C3 variant is only built and released when the user explicitly asks for it (e.g., "also release the C3 firmware").
In a standard 3-variant release (Standard + Headless + Touch 3.5"), skip all C3 steps entirely.

## Automated Release Steps

### 1. Update Version in platformio.ini

Update the VERSION flag with the block height from step above:
```ini
-DVERSION=\"v936746\"
```

### 2. Create Firmware Directories (for the variants you are releasing)

```powershell
mkdir installer/firmware/v936746      # Standard version
mkdir installer/firmware/v936746h     # Headless version (note the 'h' suffix)
mkdir installer/firmware/v936746t     # Touch 3.5" version (note the 't' suffix)
mkdir installer/firmware/v936746c     # ESP32-C3-21-1 version (note the 'c' suffix)
```

Create only the directories needed for the current release.

### 3. Create manifest.json files (for the variants you are releasing)

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

**Touch 3.5" version** - Create `installer/firmware/v936746t/manifest.json`:
```json
{
  "name": "ZapBox Touch 3.5\"",
  "version": "v936746t",
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

**ESP32-C3-21-1 version** - Create `installer/firmware/v936746c/manifest.json`:
```json
{
  "name": "ZapBox esp32-c3-21-1",
  "version": "v936746c",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-C3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin",   "offset": 65536 }
      ]
    }
  ]
}
```

**⚠️ Important differences:**
- Headless uses `"chipFamily": "ESP32"` (not ESP32-S3)
- Headless bootloader offset is `4096` (not 0)
- Touch 3.5" uses `"chipFamily": "ESP32-S3"`
- Touch 3.5" manifest name should clearly identify the hardware
- ESP32-C3-21-1 uses `"chipFamily": "ESP32-C3"`, bootloader offset `0`

### 4. Update Web Installer (match the installer page to the released variant)

**Standard installer (`installer/index.html`) - 4 locations total**

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

**Touch 3.5" installer (`installer/touch3.5/index.html`) - 2 locations total**

**Location 1: Touch 3.5" version dropdown** - Add new version at TOP:
```html
<option value="../firmware/v936746t/manifest.json">v936746t (Latest - Short description)</option>
```
Remove `(Latest)` from the previous top touch3.5 version.

**Location 2: Touch 3.5" flash button** - Update manifest path:
```html
<esp-web-install-button id="flash-button-touch35" manifest="../firmware/v936746t/manifest.json">
```

**C3 installer (`installer/c3/index.html`) - 2 locations total**

**Location 1: C3 version dropdown** - Add new version at TOP:
```html
<option value="../firmware/v936746c/manifest.json">v936746c (Latest - Short description)</option>
```
Remove `(Latest)` from the previous top C3 version.

**Location 2: C3 flash button** - Update manifest path:
```html
<esp-web-install-button id="flash-button-c3" manifest="../firmware/v936746c/manifest.json">
```

### 5. Compile Firmware (for the variants you are releasing)

**⚠️ MANDATORY: You MUST compile the firmware AND copy the binaries (step 6) before committing!**
**A release folder with only `manifest.json` but no `.bin` files is INCOMPLETE and broken.**

**Standard version (T-Display-S3):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e lilygo-t-display-s3
```

**Headless version (ESP32 Dev):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e esp32dev
```

**Touch 3.5" version (JC3248W535C):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e Touch3_5
```

**ESP32-C3-21-1 version (only when explicitly requested by user!):**
```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e esp32-c3-21-1
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

**Touch 3.5" version (jc3248w535c) → v936746t/:**
```powershell
Copy-Item -Path ".pio\build\Touch3_5\bootloader.bin" -Destination "installer\firmware\v936746t\bootloader.bin"
Copy-Item -Path ".pio\build\Touch3_5\partitions.bin" -Destination "installer\firmware\v936746t\partitions.bin"
Copy-Item -Path ".pio\build\Touch3_5\firmware.bin" -Destination "installer\firmware\v936746t\firmware.bin"
```

**ESP32-C3-21-1 version (esp32-c3-21-1) → v936746c/:**
```powershell
Copy-Item -Path ".pio\build\esp32-c3-21-1\bootloader.bin" -Destination "installer\firmware\v936746c\bootloader.bin"
Copy-Item -Path ".pio\build\esp32-c3-21-1\partitions.bin" -Destination "installer\firmware\v936746c\partitions.bin"
Copy-Item -Path ".pio\build\esp32-c3-21-1\firmware.bin" -Destination "installer\firmware\v936746c\firmware.bin"
```

**Note:** The directory may have the `h`, `t`, or `c` suffix, but the filenames do NOT!

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

**Touch 3.5" only example:**
```bash
git add platformio.ini installer/firmware/v936746t/ installer/touch3.5/index.html FIRMWARE.md
git commit -m "Release v936746t: <short description in English>"
```

**ESP32-C3-21-1 only example:**
```bash
git add platformio.ini installer/firmware/v936746c/ installer/c3/index.html FIRMWARE.md
git commit -m "Release v936746c: <short description in English>"
```

### 9. Inform User

Tell user:
- Which release variants were prepared
- Show brief changelog in English
- Provide English GitHub release description
- Next steps: Test → Tag → Push → GitHub Release

## Quick Checklist

**⚠️ Follow in this exact order:**

- [ ] **STEP 0:** Get Bitcoin block height (ALWAYS DO THIS FIRST!)
- [ ] Update platformio.ini with new version
- [ ] Create the required firmware directories (`vBLOCKHEIGHT`, `vBLOCKHEIGHTh`, `vBLOCKHEIGHTt` — **skip C3 unless user requests it**)
- [ ] Create the required manifest.json files for the selected variants
- [ ] Update the matching web installer pages:
  - `installer/index.html` for Standard (T-Display-S3)
  - `installer/headless/index.html` for Headless
  - `installer/touch3.5/index.html` for Touch 3.5"
  - `installer/c3/index.html` for C3 (only if C3 is being released)
- [ ] **Compile firmware for each variant** (pio run -e ...) — **DO NOT SKIP!**
- [ ] **Copy all 3 binary files** (bootloader.bin, partitions.bin, firmware.bin) into each firmware folder — **DO NOT SKIP!**
- [ ] Verify each firmware folder contains 4 files: `manifest.json` + 3 `.bin` files
- [ ] Generate release description from git log (in English)
- [ ] Git commit (DO NOT push yet)
- [ ] Inform user with English release notes

## Common Mistakes to Avoid

1. ❌ **DON'T** skip compiling the firmware — the folder MUST contain `bootloader.bin`, `partitions.bin`, `firmware.bin` in addition to `manifest.json`
2. ❌ **DON'T** forget to copy the `.bin` files after compiling (step 6) — creating the manifest alone is not enough
3. ❌ **DON'T** release the ESP32-C3-21-1 variant unless the user explicitly asks for it
4. ❌ **DON'T** add "-headless" suffix to binary filenames
5. ❌ **DON'T** forget the 'h' suffix in the headless directory name (v936746h)
6. ❌ **DON'T** forget the 't' suffix in the Touch 3.5" directory name (v936746t)
7. ❌ **DON'T** forget the 'c' suffix in the ESP32-C3-21-1 directory name (v936746c)
8. ❌ **DON'T** use same chipFamily for all variants (ESP32-S3 vs ESP32 vs ESP32-C3)
9. ❌ **DON'T** use same bootloader offset for headless and ESP32-S3/C3 variants (4096 vs 0)
10. ❌ **DON'T** update the wrong installer page — Headless has its own page at `installer/headless/index.html` (NOT inside `installer/index.html`)
11. ❌ **DON'T** look at HEADLESS_DEPLOYMENT.md (it's outdated/deleted)

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

# Touch 3.5" version (jc3248w535c):
.pio/build/Touch3_5/
├── bootloader.bin
├── partitions.bin
└── firmware.bin

# ESP32-C3-21-1 version (esp32-c3-21-1):
.pio/build/esp32-c3-21-1/
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

# Touch 3.5" release example:
installer/firmware/
└── v936746t/                   # Touch 3.5" version (note 't' suffix in directory)
  ├── bootloader.bin
  ├── partitions.bin
  ├── firmware.bin
  └── manifest.json

# ESP32-C3-21-1 release example:
installer/firmware/
└── v936746c/                   # ESP32-C3-21-1 version (note 'c' suffix in directory)
  ├── bootloader.bin
  ├── partitions.bin
  ├── firmware.bin
  └── manifest.json
```

## Release History

### 🚧 Pending Since Last Release (v958177 / v958177t — tag commit `d33e832`)

**Not yet released.** Commits `d33e832..HEAD` (check with `git log --oneline d33e832..HEAD`) contain changes for:

- **📦 Standard (T-Display-S3) + 🖥️ Touch 3.5"** — `7f8c7f5` Pulse LED button during screensaver: the LED now slowly breathes instead of steady ON while `isReadyForReceive()`, hinting that a press wakes the device. Display builds only (headless/C3 keep their status blink codes).
- **🖥️ Touch 3.5" only** — `6a607c1` Fix mode-select single channel, screensaver default and ticker toggle:
  - Picking "Single" on the mode-selection screen no longer wrongly shows the numeric product keypad
  - Blank "Time until activation" / deep-sleep field now falls back to the documented default (5 min / 30 min) instead of clamping to 1 minute
  - Single mode with `btcTickerMode="always"` can now be toggled back and forth between ticker and QR screen by touch
- **All 4 installer pages** (`installer/index.html`, `installer/headless/index.html`, `installer/touch3.5/index.html`, `installer/c3/index.html`) — `eb08bc3`, `5baf384` fixed the broken "More e-layouts" link (now points to `docs/hardware-assets.md#electrical-layouts` instead of the old README anchor)
- **📦 Standard installer only** — `155b2eb` GPIO 10 permanently greyed out with an explanatory note (physically not connected on T-Display-S3), GPIO 4 battery ADC documented as not implemented, a few pin labels clarified

**⚠️ Before releasing, clean up in `platformio.ini`:**
- `VERSION` is currently set to the dev marker `"v958177x"` (from commit `59f82e8`) — replace with the new Bitcoin block height per step 1, not with an `x` suffix
- `default_envs` is currently `lilygo-t-display-s3` with `Touch3_5` commented out (from commit `4730b81`, for local LED-pulse testing) — this doesn't affect the release itself since each variant is built with an explicit `-e <env>` flag, but reset it to your preferred default afterwards if desired

**Suggested release scope:** Standard + Touch 3.5" (both affected). Headless and C3 have no relevant changes since `d33e832` — skip them unless something else has landed by the time you release.

---

### v958177 / v958177t — 2026-07-15

```markdown
## 🎯 Release v958177 / v958177t — Touch 3.5 Channel Re-map & Clearer Status Screens

### 🖥️ Touch 3.5" Version (v958177t)
- **Channel re-map:** CH01–CH06 now map to GPIO 14/15/16/5/6/7 — the primary channel (CH01) is **GPIO 14**. Each physical function stayed on its GPIO, only the CHxx label moved: the battery gauge is still on GPIO 5 (now CH04) and the vending sensors are still on GPIO 7 / GPIO 5 (now CH06 / CH04).
- ⚠️ **Breaking change:** update the pin number in your LNbits switch entries to match the new channel (e.g. the primary relay is now **pin 14**, not pin 6). The wiring itself does not change.
- **Third vending sensor:** all three ADC1-capable pins (GPIO 5/6/7 = CH04/CH05/CH06) can now be configured as sensors (stop / blockage / level) — previously only two.
- **Mini-PoS decoupled from the server:** the device now tells the extension which relay pin to fire, so `zapbox_extension` no longer hard-codes a GPIO. Requires **zapbox_extension v2.5.4+**.
- **Clearer single-channel status screens:** the old "READY 4 ZAP ACTION" demo QR is replaced by **LNBITS NOT CONFIGURED** (primary pin has no LNbits switch entry) and **LNBITS LABELS NOT LOADED** (offline / config not fetched yet).
- Fixed: portrait Identity teach screen — "Learning Identities" now sits in the label box below the QR instead of being squeezed top-left under it.

### 📦 Standard Version (v958177 — T-Display-S3)
- **Clearer single-channel status screens:** **LNBITS NOT CONFIGURED** / **LNBITS LABELS NOT LOADED** instead of the old demo-QR fallback.

### 🛠️ Technical Details
- Updated to Bitcoin block height 958177
- Documentation restructured into an overview README plus per-variant pages under `docs/`
- E-Layout diagrams updated for the new Touch 3.5 pin map
- Requires **zapbox_extension v2.5.4+** for the Mini-PoS / relay-pin decoupling
```

### v957859 / v957859h / v957859t — 2026-07-13

```markdown
## 🎯 Release v957859 / v957859h / v957859t — Battery Gauge, Vending Sensors & NFC Reliability Fix

### 🖥️ Touch 3.5" Version (v957859t)
- **Battery charge gauge** (0-100%) for devices with a LiPo battery — shown in the Mini-PoS entry screen, both portrait (top-right) and landscape (top-right of the left panel)
- Reads GPIO 5's on-board voltage divider, calibrated against two full discharge runs (1000 mAh and 3000 mAh cells) across the whole usable range
- **Vending sensors** on CH02 (GPIO 7) and CH03 (GPIO 5): Stop the advance / Monitor product blockage / Level monitoring — same behaviour as the T-Display-S3 light barrier, including the PRODUCT BLOCKED and SUPPLY BIN IS EMPTY screens
- Sensor modes that never worked on this board (they required hardware Touch 3.5 doesn't have) removed from the remaining channels
- ⚠️ **Breaking change:** the primary channel (CH01) moved from GPIO 5 to GPIO 6, freeing GPIO 5 for the battery gauge. Existing devices must re-wire CH01 to GPIO 6 and update the pin in their LNbits switch entry — a firmware-only update leaves the device silent (payments arrive, nothing switches)
- Channels renumbered: CH01=6, CH02=7, CH03=5, CH04=14, CH05=15, CH06=16
- Fixed: Identity🫆Login pin followed the old GPIO 5 default instead of the board's actual CH01 pin
- Fixed: long NFC-Auth error messages now word-wrap instead of overflowing the screen edges
- **E-Layout diagrams updated** for the new CH01=GPIO6 pin map: `E-Layout-ZapBox-Touch3.5-e950939` (prototype), `E-Layout-ZapBox-Touch3.5-FOUR-e957556`, `E-Layout-ZapBox-Touch3.5-ONE-e957575`
- **New enclosure/3D-print files**: `b956540-Touch3.5-FOUR` and `b957183-Touch3.5-ONE`, including a battery holder for the LiPo connector

### 📦 Standard (v957859) + 💡 Headless (v957859h) + 🖥️ Touch 3.5" (v957859t)
- Fixed intermittent "i2cRead Error -1" NTAG 424 DNA reads: NDEF writes from the main loop could interleave a PN532 transaction mid-frame on the shared I²C bus — now serialized with the existing bus mutex
- Fixed: a failed RF-field command on the PN532 was silently ignored instead of being treated as an error

### 🛠️ Technical Details
- Updated to Bitcoin block height 957859
```

### v956101h — 2026-06-30

```markdown
## 🎯 Release v956101h — Identity🫆Login for Headless ZapBox (NTAG 424 DNA)

### 🔌 Headless Version (v956101h — ESP32 Dev)
- **Identity🫆Login mode** (NTAG 424 DNA only) for headless ZapBox — no display, no touch, no buttons
- **LNURL-auth not available** (no screen for QR code) — NFC tap only
- **GPIO output selector**: Relay / 180° Servo / 360° Servo on GPIO 12
- **Teach mode via installer PIN** (one-shot, same as T-Display-S3): 6-digit PIN in installer → automatic teach boot → PIN erased from flash
- **LED teach status**: double-pulse (150ms/100ms/150ms/1.5s) = teach active; 6× rapid flash = enrolled; 3× fast blink = NFC rejected
- **NT3H2111 FD pin fix**: floating GPIO 34 no longer blocks PN532 when no NT3H is installed (FD polling gated on NT3H I2C probe result)
- **Pay+Password**: attach LNbits QR code physically; independent of identity trigger (separate endpoints)
- Web installer: Identity🫆Login section, servo parameter panels, teach mode PIN with LED legend
```

### v956086 — 2026-06-30

```markdown
## 🎯 Release v956086 — Identity Login for T-Display-S3 (LNURL-auth & NFC Tag)

### 🖥️ T-Display-S3 Version (v956086)
- **Identity🫆Login mode** (LNURL-auth + NT3H2111 NFC Tag) for LilyGo T-Display-S3
- **NT3H2111 NFC Tag**: stores `lightning:lnurl1…` URI so smartphones can tap-to-open a Lightning wallet directly
- **QR alphanumeric mode**: uppercase `LIGHTNING:LNURL1…` enables denser QR (v8 ECC_LOW ~511 chars vs 193 binary) for T-Display-S3 fixed-v8 renderer
- **Auto-refresh k1 every 90 s** (server TTL = 120 s, 30 s safety margin) — minimises NT3H write frequency while preventing "expired k1" errors
- **Teach mode auto-refresh**: k1 also refreshes every 90 s during teach mode (NFC enrolment session)
- **Teach mode timeout**: 180 s (reduced from 300 s)
- **Teach PIN erased on first boot**: PIN written via installer is deleted from flash immediately after teach mode starts (one-shot)
- **Web installer**: renamed mode to "Identity🫆Login — LNURL-auth & NFC Tag"; Identity section paragraphs hidden when Identity mode selected
- Fixed: vending / channel4 installer sections hidden in Identity mode
```

### v955832t — 2026-06-28

```markdown
## 🎯 Release v955832t — NTAG 424 DNA Identity Login

### 🖥️ Touch 3.5" Version (v955832t)
- **NTAG 424 DNA NFC tap** for Identity Login: Bolt Card, Bolt Ring and any NTAG 424 DNA card can now trigger a relay without payment
- **PIN pad after NFC tap**: optional 4-digit PIN entry after tap (configurable, recommended)
- **NFC card enrolment in teach mode**: tap an unknown card during teach mode to enrol it; green/red toast confirms success or failure
- **SUN tap detection** before LNURL-auth payment path — handles both `lnurlw://` and `https://` TagID URLs
- **"NFC tag unknown"** message when a card is not in the allow list (was silent before)
- **Wrong PIN overlay**: centred multi-line error with white background shown for 5 s, PIN pad closes, fresh tap required
- Fixed: IDENTITY/IDENTITY mode screen label (was "AUTHY")
- Fixed: portrait label box no longer overlaps the CANCEL button in Mini-PoS QR view
- Fixed: web installer identity section always visible (removed unnecessary ENABLE/DISABLE toggle)
- Fixed: `miniPosInvoiceKey` loaded correctly when Mini-PoS is not the startup mode
- Installer: renamed "NTAG424" → "NFC Tag"; added NTAG 424 DNA PIN option
```

### v955197 / v955197h / v955197t — 2026-06-24

```markdown
## 🎯 Release v955197 / v955197h / v955197t — LNURL-Auth Identity Login

### ✨ Features
- **LNURL-Auth identity login** (LNURL-04): the ZapBox can now display a login QR that authenticates users via their Lightning wallet — no payment required
- **Dual-page mode** (Touch 3.5): swappable tabs between Identity login and classic Payment QR on the same screen
- **IDENTITY TRIGGER start screen** with "touch to start" hint; QR appears on touch
- **Teach mode**: 6-tap gesture on the screen opens a 6-digit PIN pad to protect the identity trigger
- Dedicated CANCEL button on teach screen; configurable QR label via web installer
- Red "IDENTITY LOGIN DISABLED" hint when server returns 403

### 🖥️ Touch 3.5" Version (v955197t)
- Numeric Product Selection panel for multi-channel mode
- Live block height on the product selection screen
- Per-channel servo parameters for multi-channel mode (configurable in web installer)
- Higher-quality Bitcoin logo in BTC ticker; 6-row landscape layout
- Reject WebSocket payments addressed to the wrong PIN
- Sleep wake sources corrected (touch cannot wake device; LED button added)
- Corner button tap targets fixed for the physical-button edge strip
- Portrait/landscape layout fix for error box and ACTION TIME

### 📦 Standard (v955197) + 💡 Headless (v955197h)
- Installer config-mode fix: config-mode detection lost to mixed CDC chunks — fixed for T-Display-S3 and headless
- Config wipe on interrupted write prevented
- I/O-Expander shown in Print Config (T-Display-S3 installer)

### 🛠️ Technical Details
- Updated to Bitcoin block height 955197
- Merged branch `feature/lnurlauth`
```

---

## GitHub Release Template

Use this template for GitHub releases:
```markdown
## 🎯 Release [Version(s)] - [Title]

### ✨ Features
- Feature description

### 💡 Headless Version (vXXXXXXh) - [Special features]
- Headless-specific improvements

### 📦 Standard Version (vXXXXXX)
- Standard-specific improvements

### 🖥️ Touch 3.5" Version (vXXXXXXt)
- Touch-specific improvements

### ⚡ ESP32-C3-21-1 Version (vXXXXXXc)
- C3-specific improvements

### 🛠️ Technical Details
- Updated to Bitcoin block height XXXXXX
- Technical improvements
```

