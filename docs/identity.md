# Identity🫆Login

Identity Login turns the ZapBox into an **identification terminal**. Instead of processing a payment, the device verifies a known identity — a Lightning wallet or an NFC card — and triggers a GPIO output. No money changes hands.

**Typical use cases:** access control, time tracking, personalized triggers.

> **Board-specific details** — which GPIO is triggered, how teach mode is started, LED patterns — are on the variant pages:
> [T-Display-S3](t-display-s3.md#identitylogin) · [Touch 3.5"](touch-3.5.md#identitylogin) · [Headless ESP32](headless-esp32.md#identitylogin)

---

## Table of Contents

- [What Each Variant Supports](#what-each-variant-supports)
- [How a Login Works](#how-a-login-works)
- [Authentication Methods](#authentication-methods)
- [Requirements](#requirements)
- [Configuration](#configuration)
- [Teach Mode — Enrolling Identities](#teach-mode--enrolling-identities)
- [Security](#security)
- [Extension Architecture](#extension-architecture)
- [Error Messages](#error-messages)

---

## What Each Variant Supports

The only real difference between the variants is **whether a screen exists**. Without one, there is no QR code — and therefore no wallet login.

| | Touch 3.5" | T-Display-S3 | Headless |
|---|:---:|:---:|:---:|
| **LNURL-auth** (wallet login via QR) | ✅ | ✅ | ❌ *no screen* |
| **NTAG 424 DNA** (Bolt Card / Ring tap) | ✅ | ✅ | ✅ |
| **4-digit PIN after tap** | ✅ | ✅ | ✅ |
| **Pay+Password** (classic, external QR) | ✅ | ✅ | ✅ |
| **Dual-page** (identity + payment) | ✅ | ✅ | ❌ |
| **Teach mode started by** | 6-tap gesture + PIN on screen | Installer PIN *(one-shot)* | Installer PIN *(one-shot)* |
| **Feedback** | Display toast | Display toast | [LED patterns](headless-esp32.md#led-status-diagnostics) |

Teach mode always times out after **180 s**.

---

## How a Login Works

1. The user presents their identity — QR scan **or** NFC tap
2. The ZapBox verifies it against the LNbits server
3. The relay switches for the duration configured in LNbits
4. **With a display:** a confirmation screen (Action Time) is shown briefly
   **Headless:** the relay switches silently; the LED stays solid ON

On the display variants both methods run **simultaneously** — the wallet-login QR code is visible while the NFC reader listens for cards in parallel.

---

## Authentication Methods

### LNURL-auth (wallet login)

*Display variants only.*

- The user scans the QR code with a Lightning wallet (Zeus, Breez, …)
- The wallet signs a challenge (`k1`) with its private key — LNURL-auth, [LUD-04](https://github.com/lnurl/luds/blob/luds/04.md)
- The server verifies the signature against the stored public keys
- The challenge is valid for ~120 s and is automatically renewed every 90 s

### NTAG 424 DNA (NFC tap)

*All variants.*

- The user taps a Bolt Card, Bolt Ring or any NTAG 424 DNA card on the PN532 reader
- The card delivers encrypted **SUN** parameters (`p` = PICC block, `c` = CMAC)
- The server verifies the AES-CMAC and the replay counter via the **TagID extension**
- Optionally, a 4-digit PIN is requested after each tap *(recommended)*

**Supported card URL formats:**

| Format | URL prefix |
|--------|-----------|
| TagID Bolt Card | `lnurlw://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` |
| HTTPS Bolt Card | `https://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` |

Both are recognised — the ZapBox checks for SUN parameters first and only then routes to the payment logic.

---

## Requirements

### Hardware

- A **PN532 NFC reader** for card/ring login (all variants)
- An **NT3H2111 NFC Tag** for smartphone taps (display variants, optional)

Wiring and pin assignments: [docs/nfc.md](nfc.md)

### Software

| Component | Min. version | Role |
|-----------|-------------|------|
| **LNbits** | any (self-hosted or cloud) | Backend |
| **zapbox_extension** | v2.5.0+ | Manages identities, provides the LNURL-auth endpoints |
| **tagid_extension** | v2.1.0+ | NTAG 424 DNA verification; needs *TagID Base URL* and *TagID Invoice Key* in the ZapBox instance config |

### Firmware

Set the ZapBox Mode to **Identity🫆Login — LNURL-auth & NFC Tag** (`multiControl = authy`).

---

## Configuration

The Identity section in the Web Installer is **collapsed by default**. It appears once the mode is set to *Identity* or *Selection*, and must be expanded explicitly.

| Parameter | Description | Default |
|-----------|-------------|---------|
| **Pin (GPIO triggered on success)** | Which channel switches on success — the choice depends on your board, see the variant page | CH01 |
| **Activation time (ms)** | How long the output stays active | 1000 ms |
| **Identity trigger label** | Text shown next to the QR code *(display variants)* | "ZAPBOX Identity Trigger" |
| **Identity and payment trigger** | Adds a second page with the classic payment QR *(display variants)* | No |
| **Teach mode** | Allows enrolling new identities | ENABLE |
| **Teach PIN (6 digits)** | Starts teach mode — see below | — |
| **NTAG 424 DNA PIN** | 4-digit PIN entry after each NFC tap | Yes *(recommended)* |
| **Servo parameters** | *Headless only* — start/end angle (180°) or speed/duration (360°) | — |

---

## Teach Mode — Enrolling Identities

Teach mode is the state in which **new** identities may be registered. Outside of it, unknown wallets and cards are always rejected.

### Starting teach mode

**With a touchscreen (Touch 3.5"):**

1. Tap **6 times** on the surface, **holding the 6th tap**
2. Enter the **6-digit Teach PIN** on the keypad
   - The PIN is set and verified **server-side** in the zapbox_extension — it is never stored on the device
   - 3 failed attempts lock teach access (unlockable in LNbits)
3. The teach screen appears: a registration QR code, with the NFC reader active

**Without a touchscreen (T-Display-S3, Headless):**

1. Enter the Teach PIN in the Web Installer (*Teach Mode — One-time PIN*, 6 digits) — it must match the PIN in the zapbox_extension
2. Write the config and restart — teach mode starts **automatically, once**
3. The PIN is **erased from flash immediately** on that boot, so it cannot repeat unintentionally

### Enrolling a wallet (LNURL-auth)

*Display variants only.*

1. The teach screen shows a QR code with `action=register`
2. The wallet scans it and registers its public key on the server
3. The display confirms **"Wallet registered"** — the next wallet can follow immediately (the QR auto-renews)

### Enrolling an NFC card or ring

*All variants.*

1. In teach mode, hold the card or ring against the PN532 reader
2. The ZapBox reads the SUN parameters and sends them to the TagID server
3. **Success:** green toast *"NFC card enrolled"* — headless: LED 6× rapid flash
4. **Failure** (card not in TagID, server unreachable): red toast *"Card not enrolled"* — headless: no separate signal, teach mode simply continues

Always verify the result in the LNbits zapbox_extension (a CTRL+F5 refresh may be needed).

### Ending teach mode

| Method | Display variants | Headless |
|--------|:---:|:---:|
| Button / touch | CANCEL or NEXT | — |
| Automatic timeout | 180 s | 180 s |
| Server event (`teach_ended`) | ✅ | ✅ |
| Power cycle | ✅ | ✅ |

---

## Security

### Method comparison

| Method | Factor | Replay protection | Brute-force protection | Strength |
|--------|--------|-------------------|------------------------|----------|
| **Pay+Password** | Payment + PIN | One-time invoice | Each attempt costs sats | Basic |
| **LNURL-auth** | Wallet (private key) | `k1` one-time challenge | Cryptographic | Medium–High |
| **NTAG 424 DNA** | Hardware card (possession) | AES-CMAC + counter | Cloning impossible | Medium |
| **NTAG 424 DNA + PIN** | Card + knowledge | AES-CMAC + counter | Hardware + PIN | **High** |

> LNURL-auth is cryptographically strong (secp256k1) and benefits indirectly from the phone's device lock and wallet PIN — in practice roughly 1.5 factors.
>
> NTAG 424 DNA **without** a PIN reliably prevents cloning, but losing the card is enough to lose access — it is a single factor (possession). **Adding the PIN is what makes it strong.**

### LNURL-auth details

| Aspect | Property |
|--------|----------|
| Replay protection | `k1` is a one-time challenge (~120 s valid, renewed every 90 s) |
| Forgery resistance | Cryptographic signature (secp256k1) |
| Identity assignment | Yes — each wallet has a unique public key |

### NTAG 424 DNA details

| Aspect | Property |
|--------|----------|
| Replay protection | AES-CMAC + counter (SUN mechanism) — every tap is unique |
| Forgery resistance | Hardware-secured AES-128 key |
| Card cloning | A clone has a different counter → rejected |
| Extra PIN | Optional (4-digit, recommended) |

**Privacy UID (optional):** the NTAG 424 DNA can be configured to broadcast a **random air UID** on every tap instead of a static one. This prevents passive tracking — a third-party NFC reader cannot re-identify the card. Authentication is unaffected: the real UID stays AES-encrypted inside the `p` parameter and is decrypted server-side.

Privacy UID is a **privacy measure, not a substitute for the PIN**. It is activated with the Bolt Card Programmer App (v0.1.4+) and is **irreversible**.

### Pay+Password

Classic ZapBox functionality, usable as a simple access mechanism **without** enabling full Identity mode.

- The ZapBox shows a Lightning invoice QR (e.g. 10 sats)
- The user pays — the payment is the first factor (wallet possession)
- A **password / PIN dialog** appears (the LNbits "comment" mechanism)
- Only when payment **and** password match does the relay switch
- **Requires** the *Comment* function to be enabled in the zapbox_extension

**Why it is reasonably secure:** every invoice is unique (so a captured QR cannot be replayed) and every guess costs sats (so brute-forcing is economically pointless).

**Its limit:** the identity is not individually assigned. Anyone with the password *and* sats can trigger it — no allowlist, no per-person audit trail.

> **Headless:** since there is no display, the LNbits QR code must be physically attached to the device. Pay+Password and the Identity NFC trigger are fully independent — they use separate LNbits endpoints and do not interfere.

---

## Extension Architecture

```
ZapBox (Firmware)
    │
    ├── LNURL-auth ──► zapbox_extension  ──► LNbits Wallet
    │   (display only)  (v2.5.0+)             (identities, k1)
    │
    └── NFC SUN tap ──► zapbox_extension ──► tagid_extension
        (all variants)  /api/v1/nfc/auth      (AES-CMAC check,
                        /api/v1/nfc/teach       allowlist, PIN)
```

- **zapbox_extension** is the central coordinator: it provides the auth URLs, verifies LNURL-auth signatures and forwards NFC requests to TagID
- **tagid_extension** handles NTAG 424 DNA: card allowlist, CMAC verification, optional PIN validation
- The ZapBox only ever talks to the zapbox_extension — TagID runs transparently behind it, server-side

---

## Error Messages

| Message | Where | Meaning |
|---------|-------|---------|
| `Wrong PIN / N tries left / Tap card again` | Display | Wrong PIN, attempts remaining |
| `NFC tag unknown` | Display · LED 3× blink | Card not in the allowlist (not enrolled) |
| `NFC Identity Failed` | Display · LED 3× blink | Auth failed generally (CMAC error, connection issue) |
| `NFC card enrolled` | Display · LED 6× flash | Card successfully enrolled (teach mode) |
| `Card not enrolled` | Display | Teach failed — card not in TagID |
| `Wallet registered` | Display | LNURL-auth wallet successfully registered |
| `IDENTITY LOGIN DISABLED` | Display | The server returned 403 |

---

## Notes

**Screensaver behaviour** *(display variants)*: when the screensaver is active (backlight off), the **first touch only wakes the display** — no action is triggered. The second touch works normally. This prevents accidentally hitting a button on a dark screen.

**Startup-Mode: Selection**: the user can choose between multiple modes at boot, with Identity offered as one of the options. Mini-PoS and Identity mode can be used alternately on the same device — NTAG 424 DNA cards work in both.
