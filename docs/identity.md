# Identity🫆Login

Identity Login turns the ZapBox into an identification terminal. Instead of processing a payment, the device verifies a known identity (Lightning wallet or NFC card) and triggers a GPIO output — without any money changing hands. Typical use cases: access control, time tracking, personalized triggers.

---

## Table of Contents

1. [Version Comparison](#1-version-comparison)
2. [How a Successful Login Works](#2-how-a-successful-login-works)
3. [Authentication Methods](#3-authentication-methods)
   - 3.1 [LNURL-auth (Wallet Login)](#31-lnurl-auth-wallet-login)
   - 3.2 [NTAG 424 DNA (NFC Tap)](#32-ntag-424-dna-nfc-tap)
4. [Hardware Requirements](#4-hardware-requirements)
5. [Web Installer Configuration](#5-web-installer-configuration)
6. [Teach Mode — Enrolling Identities](#6-teach-mode--enrolling-identities)
7. [Headless LED Status](#7-headless-led-status)
8. [Security](#8-security)
   - 8.1 [Method Comparison](#81-method-comparison)
   - 8.2 [Pay+Password](#82-paypassword)
   - 8.3 [LNURL-auth Details](#83-lnurl-auth-details)
   - 8.4 [NTAG 424 DNA Details](#84-ntag-424-dna-details)
9. [Extension Architecture](#9-extension-architecture)
10. [Screensaver Behaviour](#10-screensaver-behaviour)
11. [Startup-Mode: Selection](#11-startup-mode-selection)

---

## 1. Version Comparison

| Feature | Touch 3.5" | T-Display-S3 | Headless |
|---------|:----------:|:------------:|:--------:|
| **Display** | Touchscreen 3.5" | 1.9" (170×320) | — |
| **LNURL-auth (wallet login via QR)** | ✓ | ✓ | ✗ |
| **NT3H2111 NFC Tag (smartphone tap)** | ✓ | ✓ (optional) | ✗ |
| **NTAG 424 DNA (Bolt Card / Ring)** | ✓ | ✓ (optional) | ✓ |
| **4-digit PIN after NFC tap** | ✓ | ✓ | ✓ |
| **Pay+Password (classic, external QR)** | ✓ | ✓ | ✓ |
| **Relay (GPIO 12 / CH01–CH06)** | CH01–CH06 | GPIO 12 | GPIO 12 |
| **180° Servo** | ✗ | ✗ | ✓ |
| **360° Servo** | ✗ | ✗ | ✓ |
| **Teach mode activation** | 6-tap + gesture + PIN on display | Installer PIN (one-shot) | Installer PIN (one-shot) |
| **Teach mode confirmation** | Display toast | Display toast | LED — 6× Rapid Flash |
| **Teach mode status** | Display screen | Display screen | LED — Double-pulse |
| **NFC rejected feedback** | Display toast | Display toast | LED — 3× Fast Blink |
| **Teach timeout** | 180 s | 180 s | 180 s |
| **Dual-page mode (identity + payment)** | ✓ | ✓ | ✗ |
| **Startup-Mode: Selection** | ✓ | ✓ | ✓ |
| **Screensaver** | ✓ | ✓ | — |
| **Installer** | `installer/touch3.5/` | `installer/` | `installer/headless/` |

---

## 2. How a Successful Login Works

1. User presents their identity (QR scan or NFC tap)
2. ZapBox verifies the identity via the LNbits server
3. Relay switches for the configured duration (set in LNbits)
4. **With display:** A confirmation screen (Action Time) is shown briefly
5. **Headless:** Relay switches silently — no visual feedback other than the LED staying solid ON

---

## 3. Authentication Methods

### 3.1 LNURL-auth (Wallet Login)

*Available on Touch 3.5" and T-Display-S3 only.*

Both methods (QR wallet login and NFC card tap) run **simultaneously** — the QR code for wallet login is always visible while the NFC reader listens for cards in parallel.

- User scans the QR code with a Lightning wallet (e.g. Zeus, Breez)
- Wallet signs a challenge (k1) with its private key → LNURL-auth (LUD-04)
- Server verifies the signature against stored public keys
- Challenge is valid for ~120 seconds and is automatically renewed every 90 s

> **Headless:** LNURL-auth is not available — no display, no QR code.

### 3.2 NTAG 424 DNA (NFC Tap — Ring Login / NTAG424 Login)

*Available on all variants.*

- User taps a Bolt Card, Bolt Ring, or NTAG 424 DNA card on the PN532 reader
- Card delivers encrypted SUN parameters (`p` = PICC Block, `c` = CMAC)
- Server verifies AES-CMAC and replay protection (counter) via the **TagID Extension**
- Optional: additional 4-digit PIN entry after each tap (recommended)

#### Supported NFC Card Formats

| Format | URL Prefix | Notes |
|--------|-----------|-------|
| TagID Bolt Card | `lnurlw://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` | Standard |
| HTTPS Bolt Card | `https://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` | Alternative |

Both formats are recognised — the ZapBox checks for SUN parameters first and only then routes to payment logic.

---

## 4. Hardware Requirements

### ZapBox Touch 3.5" (ESP32-S3, JC3248W535C)

- Touchscreen display (3.5")
- PN532 NFC reader (for NTAG 424 DNA / Ring Login)
- NT3H2111 NFC Tag (LNURL-auth via smartphone tap)
- Relay on one of 6 GPIO outputs (CH01–CH06)
- Installer: `installer/touch3.5/index.html`

### ZapBox T-Display-S3 (ESP32-S3, LilyGo T-Display-S3)

- 1.9" display (170×320)
- PN532 NFC reader (optional — NTAG 424 DNA / Ring Login)
- NT3H2111 NFC Tag module (MikroE NFC Tag 2 Click, optional — enables LNURL-auth via smartphone tap)
- Relay on GPIO 12 (CH01)
- Teach mode started via web installer (set a 6-digit PIN → device boots into teach mode once)
- No touchscreen — navigation via NEXT button
- Installer: `installer/index.html`

### ZapBox Headless (ESP32 Dev Module)

- No display, no touchscreen, no buttons
- PN532 NFC reader (NTAG 424 DNA / Ring Login)
- No NT3H2111 — LNURL-auth not available
- GPIO 12: Relay, 180° Servo, or 360° Servo (selectable in installer)
- Status LED (GPIO 2 / GPIO 21) signals teach mode, enrolment, and errors
- Teach mode like T-Display-S3 via installer PIN (one-shot)
- Installer: `installer/headless/index.html`

### Software / Services

| Component | Min. Version | Role |
|-----------|-------------|------|
| **LNbits** | any (self-hosted or cloud) | Payment backend |
| **zapbox_extension** | v2.5.0+ | Manages identities, provides LNURL-auth endpoints |
| **tagid_extension** | v2.1.0+ | NTAG 424 DNA verification (Ring Login); requires *TagID Base URL* and *TagID Invoice Key* in the ZapBox instance config |

### Firmware

ZapBox firmware with `multiControl = authy` (Installer: **Identity🫆Login — LNURL-auth & NFC Tag**).

---

## 5. Web Installer Configuration

The Identity section in the installer is **collapsed by default**. It appears when the mode is set to *Identity* or *Selection* and must be expanded explicitly.

### Touch 3.5" and T-Display-S3

| Parameter | Description | Default |
|-----------|-------------|---------|
| **Identity Login (LNURL-auth or NTAG424)** | ENABLE / DISABLE | DISABLE |
| **Pin (GPIO triggered on success)** | Which relay switches on success (CH01–CH06) | CH01 — GPIO 5 |
| **Activation time (ms)** | How long the relay stays active | 1000 ms |
| **Identity trigger label** | Text shown next to the QR code | "ZAPBOX Identity Trigger" |
| **Identity and payment trigger** | Second page with classic payment QR | No |
| **Teach mode** | ENABLE / DISABLE — allows enrolling new identities | ENABLE |
| **Teach-PIN (6 digits)** | Touch 3.5" only: reference field (PIN is verified server-side in zapbox_extension) | — |
| **Teach Mode — One-time PIN** | T-Display-S3 only: 6-digit PIN in installer → one-shot teach boot | — |
| **NTAG 424 DNA PIN** | 4-digit PIN entry after each NFC tap | Yes (recommended) |

### Headless

| Parameter | Description | Default |
|-----------|-------------|---------|
| **Pin (GPIO triggered on success)** | Relay / 180° Servo / 360° Servo on GPIO 12 | Relay |
| **Servo parameters** | Start/End angle (180°) or Speed/Duration (360°) | — |
| **Teach Mode — One-time PIN** | 6-digit PIN in installer → one-shot teach boot | — |
| **NTAG 424 DNA PIN** | 4-digit PIN entry after each NFC tap | Yes (recommended) |

> **Pay+Password (Headless):** Attach an LNbits QR code physically to the ZapBox and configure Pay+Password in LNbits. Identity trigger (NFC) and classic Lightning payment use separate LNbits endpoints and do not interfere with each other.

---

## 6. Teach Mode — Enrolling Identities

### Activation

**Touch 3.5":**
1. **Tap 6 times** on the ZapBox surface, **holding the 6th tap**
2. Enter the **6-digit Teach PIN** on the display keypad
   - The PIN is set and verified server-side in the **zapbox_extension** — it is not stored on the device
   - 3 failed attempts lock teach access (unlockable in LNbits)
3. Display switches to teach screen: QR code for wallet registration + NFC reader active

**T-Display-S3 and Headless:**
1. Enter the Teach PIN in the **web installer** (field *Teach Mode — One-time PIN*, 6 digits)
2. PIN must match the Teach PIN set in the **zapbox_extension**
3. Restart the device (Write Config → Restart) — teach mode starts **automatically once** after boot
4. PIN is erased from flash immediately on first boot (one-shot) — no repeat on next boot
5. **T-Display-S3:** Display shows teach screen with QR + NFC reader active
6. **Headless:** No display — status via LED (double-pulse = teach active, see [LED Status](#7-headless-led-status))

### Enrolling a Wallet (LNURL-auth) — Touch 3.5" and T-Display-S3

1. Teach screen shows QR with `action=register`
2. Wallet scans QR → registers its public key on the server
3. Display confirms "Wallet registered"
4. Next wallet can be registered immediately (QR auto-renews)

> **Headless:** LNURL-auth not available. Only NFC cards can be enrolled.

### Enrolling an NFC Card / Ring — All Variants

1. In teach mode, hold card/ring against the PN532 reader
2. ZapBox reads SUN parameters → sends to TagID server for enrolment
3. **With display:** green toast **"NFC card enrolled"**  
   **Headless:** LED shows 6× Rapid Flash (50 ms ON/OFF)
4. Error (card not in TagID / server unreachable):  
   **With display:** red toast **"Card not enrolled"**  
   **Headless:** no separate error signal — teach mode continues
5. Verify enrolment in LNbits zapbox_extension (CTRL+F5 to refresh)

### Ending Teach Mode

| Method | Touch 3.5" | T-Display-S3 | Headless |
|--------|:----------:|:------------:|:--------:|
| Button / touch | CANCEL on display | NEXT button | — |
| Automatic timeout | 180 s | 180 s | 180 s |
| Server event | `teach_ended` WS | `teach_ended` WS | `teach_ended` WS |
| Power cycle | ✓ | ✓ | ✓ |

---

## 7. Headless LED Status

Since the headless version has no display, the status LED signals all relevant states:

| LED Pattern | Timing | Meaning |
|-------------|--------|---------|
| **Double-pulse** | 150 ms ON / 100 ms OFF / 150 ms ON / 1.5 s pause (1.9 s cycle) | Teach mode active — waiting for card |
| **6× Rapid Flash** | 50 ms ON/OFF × 6 (600 ms) | Card / wallet successfully enrolled |
| **3× Fast Blink** | 100 ms ON/OFF × 3, then solid ON | NFC card not recognised or rejected (tagid 404) |
| **Solid ON** | Continuous | Ready — waiting for NFC tap |
| **Slow Blink** | 1 Hz | Config mode active |

> Always verify enrolment results in the LNbits zapbox_extension. A CTRL+F5 page refresh may be required.

---

## 8. Security

### 8.1 Method Comparison

| Method | Factor | Replay Protection | Brute-Force Protection | Strength |
|--------|--------|-------------------|------------------------|----------|
| **Pay+Password** | Payment + PIN | One-time invoice | Payment costs sats | Basic |
| **LNURL-auth** | Wallet (private key) | k1 is a one-time challenge | Cryptographic | Medium–High |
| **NTAG 424 DNA** | Hardware card (possession) | AES-CMAC + counter | No cloning possible | Medium |
| **NTAG 424 DNA + PIN** | Card + knowledge | AES-CMAC + counter | Hardware + PIN | High |

> **Notes:**  
> LNURL-auth is cryptographically strong (secp256k1) and benefits indirectly from device lock + wallet PIN — in practice roughly 1.5 factors.  
> NTAG 424 DNA *without* PIN reliably prevents cloning, but card loss or theft is sufficient for access — a single factor (possession).

### 8.2 Pay+Password

Classic ZapBox functionality usable as a simple access mechanism without enabling full Identity Mode.

**How it works:**
- ZapBox shows a Lightning invoice QR code (e.g. 10 sats)
- User pays with their wallet → payment is the first factor (wallet possession)
- A **password / PIN dialog** appears (LNbits "comment" mechanism)
- Only when payment **and** correct password match does the relay switch
- **Requirement:** The **Comment** function must be enabled in the zapbox_extension

**Security properties:**
- **Replay protection:** Every invoice is unique — a captured QR scan cannot be reused
- **Brute-force protection:** Every attempt costs sats — automated guessing is economically unattractive
- **Two factors:** Wallet (possession) + password (knowledge)

**Limitations:** User identity is not individually assigned — anyone with the password (and sats) can trigger. No allowlist, no per-person audit log.

> **Headless + Pay+Password:** Since the headless ZapBox has no display, the LNbits QR code must be physically attached to the device. Pay+Password and Identity NFC trigger are fully independent — both use separate LNbits endpoints. The **Comment** function must be enabled in the zapbox_extension.

### 8.3 LNURL-auth Details

*Touch 3.5" and T-Display-S3 only.*

| Aspect | Property |
|--------|----------|
| Replay protection | k1 is a one-time challenge (~120 s valid, renewed every 90 s) |
| Forgery resistance | Cryptographic signature (secp256k1) |
| Extra PIN | Not applicable |
| Identity assignment | Yes — each wallet has a unique public key |

### 8.4 NTAG 424 DNA Details

| Aspect | Property |
|--------|----------|
| Replay protection | AES-CMAC + counter (SUN mechanism) — every tap is unique |
| Forgery resistance | Hardware-secured AES-128 key |
| Extra PIN | Optional (4-digit, recommended) |
| Card cloning | Clone has a different counter → rejected |

**Without PIN:** Anyone with the physical card can log in.  
**With PIN:** Card + PIN required — significantly more secure.

**Privacy UID (optional):**  
The NTAG 424 DNA can be configured to broadcast a random air UID on every tap (instead of a static one). This prevents passive tracking — a third-party NFC reader cannot re-identify the card. Authentication is unaffected: the real UID remains AES-encrypted in the `p` parameter and is correctly decrypted server-side. Privacy UID is a **privacy measure**, not a substitute for PIN. Activation via the Bolt Card Programmer App (v0.1.4+) — **irreversible**.

**Error messages:**

| Message | Where shown | Meaning |
|---------|-------------|---------|
| `Wrong PIN / N tries left / Tap card again` | Display toast | Wrong PIN, attempts remaining |
| `NFC tag unknown` | Display toast / LED 3× Blink | Card not in allowlist (not enrolled) |
| `NFC Identity Failed` | Display toast / LED 3× Blink | Auth generally failed (CMAC error, connection issue) |
| `NFC card enrolled` | Display toast / LED 6× Flash | Card successfully enrolled (teach mode) |
| `Card not enrolled` | Display toast | Teach failed — card not in TagID |
| `Wallet registered` | Display toast | LNURL-auth wallet successfully registered |
| `IDENTITY LOGIN DISABLED` | Display toast | Server returned 403 |

---

## 9. Extension Architecture

```
ZapBox (Firmware)
    │
    ├── LNURL-auth ──► zapbox_extension  ──► LNbits Wallet
    │   (Touch / T-S3)  (v2.5.0+)              (identities, k1)
    │
    └── NFC SUN tap ──► zapbox_extension ──► tagid_extension
        (all variants)  /api/v1/nfc/auth       (AES-CMAC check,
                        /api/v1/nfc/teach        allowlist, PIN)
```

- **zapbox_extension** is the central coordinator: provides auth URLs, verifies LNURL-auth signatures, forwards NFC requests to TagID
- **tagid_extension** handles NTAG 424 DNA: manages card allowlist, verifies CMAC, optionally validates PIN
- The ZapBox communicates only with the zapbox_extension — TagID runs server-side transparently behind it

---

## 10. Screensaver Behaviour

*Touch 3.5" and T-Display-S3 only.*

When the **screensaver** is active (display backlight off):
- **First touch** wakes the display only — no action is triggered
- **Second touch** works normally (button, cancel, PIN entry, etc.)

This prevents accidentally hitting a button when the screen is dark.

---

## 11. Startup-Mode: Selection

With **Startup-Mode: Selection**, the user can choose between multiple modes at boot. Identity mode is offered as one of the options — useful when the ZapBox also serves as a mini-PoS.

Mini-PoS and Identity mode can be used alternately on the same ZapBox; NTAG 424 DNA cards work in both modes.
