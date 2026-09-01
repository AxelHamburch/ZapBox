# NFC

The ZapBox supports three independent NFC functions. They can all run **at the same time** on the same device — no mode switching, no configuration needed.

| Function | Hardware | Direction | What it does |
|----------|----------|-----------|--------------|
| [**NFC Payment**](#nfc-payment-bolt-card--ntag21x) | PN532 | ZapBox **reads** the card | A customer taps a Bolt Card / LNURL tag and it pays |
| [**Card Emulation**](#nfc-card-emulation-lnurlp-via-pn532) | PN532 | ZapBox **is** a tag | A phone taps the reader and gets the LNURLp |
| [**NFC Tag 2**](#nfc-tag-2--nt3h2111) | NT3H2111 | ZapBox **is** a real tag | A phone taps the tag chip and gets the LNURLp — best compatibility |

> All NFC features are activated via the build flag `ENABLE_NFC=1` (enabled by default on the display variants).

---

## Pin Assignment per Variant

The I²C bus is always **SDA / SCL** shared with the other I²C peripherals. Only the interrupt lines differ:

| Variant | I²C SDA | I²C SCL | PN532 IRQ | NT3H2111 FD |
|---------|---------|---------|-----------|-------------|
| [T-Display-S3](t-display-s3.md) | GPIO 18 | GPIO 17 | GPIO 1 | GPIO 3 |
| [Touch 3.5"](touch-3.5.md) | GPIO 18 | GPIO 17 | GPIO 9 | GPIO 46 |
| [Headless ESP32](headless-esp32.md) | GPIO 18 | GPIO 17 | GPIO 4 ¹ | GPIO 34 ² |
| [ESP32-C3](esp32-c3.md) | GPIO 20 | GPIO 21 | GPIO 10 | — |

¹ GPIO 1 is UART0 TX on the classic ESP32 and therefore unusable.
² GPIO 34 is input-only with no internal pull-up — an **external 10 kΩ pull-up to 3.3 V** is required.

**I²C addresses:** PN532 `0x24` · NT3H2111 `0x55` · PCF8574 `0x20`

An I²C bus mutex serialises access, so the PN532, the NT3H2111 and the PCF8574 can share the bus without collisions.

---

## NFC Payment (Bolt Card / NTAG21x)

**Hardware:** PN532 NFC module (HW-147, I²C mode)

Two card types are supported:

- **Bolt Cards (NTAG 424 DNA)** — authenticated LNURLW read via SUN message
- **NTAG21x (213/215/216) / LNURL tags** — plain NDEF text record containing an `lnurlw://` URL

A customer simply holds their card or tag near the PN532 reader to trigger a payment.

### Wiring

```
PN532 HW-147    →    ZapBox
────────────────────────────────────────────────
VCC (3.3V)      →    3.3V
GND             →    GND
SDA             →    I²C SDA   (see pin table above)
SCL             →    I²C SCL   (see pin table above)
IRQ             →    NFC IRQ   (see pin table above)
```

> The HW-147 module does not expose a hardware reset pin (RSTPD_N). The PN532 chip initializes automatically on power-up.

### Payment flow

```
[Bolt Card / NTAG21x]  →  tap on PN532 reader
    ↓
PN532 detects ISO14443A card (IRQ goes LOW)
    ↓
FreeRTOS task reads the card:
  • NTAG424 DNA: authenticated file read (SUN message)
  • NTAG21x:     NDEF text record parse
    ↓
ZapBox validates the "lnurlw://" prefix
    ↓
Device signals PENDING (screen or LED)
    ↓
Event over the persistent device channel (zapbox_extension ≥ 2.6.0):
  { "event": "lnurlw", "request_id": "...", "lnurlw": "lnurlw://...", "pin": <activePin> }
    ↓
Extension resolves LNURLW → invoice → payment detected
    ↓
Relay trigger back over the same channel → ZapBox activates the channel
```

### Transport: persistent device channel

The tap event travels over a **persistent WebSocket** to the extension
(`/zapbox/api/v1/ws/nfc/<deviceId>`), opened once after boot and kept alive by
device-side protocol pings. No fresh TLS connection is opened in the payment
path — some consumer routers drop *new* TLS connections in phases while
established connections keep working, which made per-tap HTTPS requests fail
intermittently.

While the channel is connected the device runs in **single-connection mode**:
the LNbits core WebSocket (`/api/v1/ws/<id>`) is deliberately released and the
extension (≥ 2.6.1) routes all events — relay triggers, `pin_required` /
`pin_error`, teach events — over the device channel. Routers with small NAT
tables cannot hold two persistent connections to the same host, so exactly one
is kept. If the channel drops, the core WebSocket takes over automatically.

The 4-digit payment PIN is also submitted over the channel
(`pin_submit` / `pin_submit_result`, extension ≥ 2.6.2).

**Fallback:** when the channel is down (older extension, connect failure), the
tap falls back to `POST /zapbox/api/v1/nfc/<deviceId>` and the PIN to
`POST .../nfc/pin_submit` over HTTPS — old firmware and old extensions keep
working, but do not mix the 2.6.0/2.6.1 intermediates with newer counterparts.

### Timeout & "NO LUCK"

After a card tap the device enters a **pending** state while waiting for payment confirmation:

| | Pending | Timeout (60 s) | Success |
|---|---|---|---|
| **Display variants** | "PENDING NFC" screen | "NO LUCK" screen (5 s) | "ACTION TIME" → "THANK YOU" |
| **Headless** | LED 200 ms ON / 800 ms OFF | LED 3× fast blink | LED 2× fast blink, then relay fires |

An error reply on the device channel (`lnurlw_result` with `status: ERROR`) shows NO LUCK immediately, followed by the server's error detail. On the HTTPS fallback path, connection-level errors do **not** immediately trigger NO LUCK — the server may still confirm the payment within the timeout window.

### Card removal detection

After a successful read, the NFC task waits for the card to be physically removed. Detection requires **2 consecutive absent polls** (~0.8 s) to prevent false triggers. This is what stops a single tap from firing two payment requests.

### Hardware test mode

To verify the hardware without a running LNbits extension:

```ini
build_flags =
  -DENABLE_NFC=1
  -DENABLE_NFC_TEST=1   ; hardware test — no server needed
```

Flash, open the serial monitor (115200 baud) and hold a Bolt Card near the reader. The display shows **"NFC OK!"** with a preview of the LNURLW, and the full string is printed to serial. This exercises the complete read path (PN532 init → NTAG424 read → LNURLW decode) independently of the payment backend.

---

## NFC Card Emulation (LNURLp via PN532)

The ZapBox can act as an **NFC tag** so that smartphones read the LNURLp payment link by tapping the PN532 module — no QR scanning needed.

The PN532 switches from Reader Mode to **Target Mode**, emulating an ISO 14443-4 Type 4 Tag. The phone detects the ZapBox as a standard NDEF tag containing a `lightning:LNURL...` URI.

```
Smartphone (Initiator)          ZapBox + PN532 (Target)
─────────────────────           ──────────────────────────
NFC field ON          ────►     PN532 Target Mode (passive)
                                IRQ fires → phone detected
ISO 14443-4 activation ◄──►     RATS/ATS exchange
SELECT NDEF App       ────►     OK (9000)
SELECT / READ CC      ────►     Capability Container
SELECT NDEF file      ────►     OK (9000)
READ BINARY (chunks)  ────►     NDEF URI record
                                  "lightning:LNURL1DP68GURN..."
Phone opens wallet    ◄────     Payment flow starts
```

**Key properties:**

- **Zero interaction** — the customer just taps their phone
- **Automatic NDEF update** — the payload updates whenever the QR code changes (product selection, channel switch)
- **Coexists with Reader Mode** — Bolt Card reading and card emulation run in parallel automatically
- **No additional hardware** — same PN532 module and wiring

**Supported phones:** Android (any NFC phone), iOS (iPhone 7 and later).

> **Note on Wallet of Satoshi:** WoS's NFC feature is designed exclusively for Bolt Cards (NTAG424 with LNURLW authentication) and does not process plain LNURLp from NFC tags. WoS users should scan the QR code. This is an app limitation — a PN532-based emulation cannot replicate the NTAG424 AES key derivation that Bolt Card authentication requires.

<details>
<summary><strong>Why not NTAG21x (Type 2 Tag) emulation?</strong></summary>

The PN532 cannot emulate an NTAG21x via its TgGetData/TgSetData interface because the `GET_VERSION` command (`0x60`), which modern NFC readers use to fingerprint NTAG chips, collides with the Mifare `AUTH_A` command (also `0x60`). In Type 2 Tag mode the PN532 intercepts `0x60` internally and answers with a Mifare authentication response; the phone — expecting a GET_VERSION reply — immediately deselects the tag (PN532 error `0x25`).

ISO-DEP (Type 4 Tag, SAK=0x20) is the only PN532 target mode that provides transparent APDU data exchange, which is why card emulation uses it.

</details>

---

## NFC Tag 2 — NT3H2111

**Hardware:** NT3H2111 NFC Tag 2 module (I²C address `0x55`)

The ZapBox writes the current LNURLp as an NDEF record into the **NT3H2111 tag chip** via I²C. When a smartphone approaches, the phone's NFC field powers the chip directly — **the ESP32 is not involved during the tap at all**.

Unlike the PN532-based card emulation, the NT3H2111 is a **real NTAG chip**. It passes the `GET_VERSION` fingerprint check that modern phones perform, which gives it the best wallet compatibility of the three methods.

```
ZapBox (ESP32)                   NT3H2111             Smartphone
──────────────                   ────────             ──────────
LNURL changes     ──I²C──►       NDEF updated
(product switch)                 (chip stores data)
                                                      NFC field ON
                                 ◄── powered by phone field ──
                                 NDEF URI record ──────────►
                                                      wallet opens
```

**Key properties:**

- **True NTAG chip** — no emulation artifacts, broad wallet compatibility
- **ESP32 not involved during the tap** — once programmed, the chip answers the phone autonomously
- **Automatic NDEF update** — the LNURL is rewritten via I²C whenever the active product or channel changes
- **No configuration required** — auto-detected at startup via I²C scan, silently skipped if absent
- **Coexists with the PN532** — both run independently, no mode switching

### Wiring

```
NT3H2111 Module    →    ZapBox
──────────────────────────────────────────────────
VCC (3.3V)         →    3.3V
GND                →    GND
SDA                →    I²C SDA   (see pin table above)
SCL                →    I²C SCL   (see pin table above)
FD / INT           →    FD pin    (see pin table above)   ← optional
```

### The FD (Field Detection) pin

The FD pin is open-drain and **active LOW**: a phone's NFC field pulls it LOW, no field leaves it HIGH. The firmware uses it to **pause the PN532's RF field** while a phone is tapping — otherwise the two NFC fields would interfere.

No configuration is needed. FD polling is gated on the NT3H2111 being found on I²C, so a floating FD pin on a device without an NT3H2111 cannot block the PN532.

**Supported wallets:** Phoenix (Android/iOS), and any wallet supporting LNURL-pay via NFC NDEF URI records.
