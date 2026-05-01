# WoS NFC — Warum der PN532 nicht reicht und was hilft

## Ergebnis der Tests (01.05.2026)

| App | Ergebnis | Details |
|---|---|---|
| **Wallet of Satoshi** | ❌ Funktioniert nicht | Immer error 0x13 → 0x25 (Deselect) — WoS-App-Limitation |
| **Phoenix** | ✅ Funktioniert (mit Gelegenheitsabbrüchen) | ISO-DEP NDEF Type 4 Tag, 8 APDUs, Zahlung ausgelöst |

---

## Warum WoS nicht funktioniert

**Log-Beweis:**
```
TgInitAsTarget: mode=0x8 init=e0803600038001fe
Phone connected — processing APDUs
error 0x13 (PPS/WTX?) — retrying TgGetData
getDataTargetIRQ: error status 0x25
First APDU not received (800ms) — retrying Target Mode
```

**Ablauf:**
1. Phone erkennt ISO-DEP Tag (SAK=0x20) — RATS wird gesendet, PN532 antwortet mit ATS
2. WoS sendet nach RATS **keinen** NFC Forum Type 4 Tag SELECT-APDU
3. WoS sendet stattdessen einen Bolt Card-spezifischen Initialisierungsframe
4. Der PN532 kann damit nichts anfangen → error 0x13
5. WoS erhält keine passende Antwort → sendet HLTA/Deselect → error 0x25
6. Keine einzige NDEF-APDU erreicht den Handler

**Ursache — WoS App-Design:**
WoS hat keine generische NDEF-Tap-Funktion. Der NFC-Feature in WoS ist **ausschließlich für Bolt Cards** (NTAG424 DNA mit LNURLW + AES-Signatur). WoS erkennt ISO-DEP (SAK=0x20) und versucht sofort den Bolt Card Auth-Flow. Ein statisches LNURLp ohne AES-Parameter scheitert zwangsläufig.

**Das ist keine Firmware-Bug** — es ist eine bewusste Einschränkung der WoS-App. Kein Software-Fix im ZapBox möglich.

---

## Warum der PN532 Type 2 Tag nicht emulieren kann

**Versuch (branch nfc-reloaded, 01.05.2026):**
ATQA=`0x44 0x00`, SAK=`0x00` gesetzt → sofortiger error `0x25` bei jedem Tap.

**Ursache:**
- Type 2 Tag (NTAG21x): erstes Kommando des Handys ist `GET_VERSION` (Opcode `0x60`)
- Mifare Classic: `AUTH_A` hat ebenfalls Opcode `0x60`
- Der PN532 fängt `0x60` **intern** ab, antwortet mit Mifare-Auth-Challenge
- Das Handy erwartet GET_VERSION-Response, versteht die Auth-Challenge nicht, sendet sofort Deselect
- Firmware-Änderung unmöglich — das passiert auf Chip-Hardware-Ebene vor TgGetData

---

## Lösung: NT3H2111 (NTAG I2C Plus)

Ein echter NTAG21x-kompatibler Chip mit I2C-Schnittstelle zum ESP32. Kein Emulations-Chip — das Handy liest einen echten NTAG, der ESP32 schreibt die LNURL rein.

```
ESP32  ──I2C──►  NT3H2111  ──NFC──►  WoS / Phoenix / Zeus
                 (echter NTAG)       GET_VERSION ✓  READ ✓
                                     SAK=0x00 → kein Bolt Card Flow
                                     → plain NDEF → LNURLp ✓
```

**Warum WoS damit funktionieren würde:**
- SAK=`0x00` signalisiert Type 2 Tag
- WoS startet **keinen** Bolt Card Auth-Flow bei SAK=0x00
- WoS liest das NDEF-URI-Record wie jede andere NFC-App
- `lightning:LNURL1...` → WoS öffnet LNURLp-Payment direkt

**Details:** siehe `NXP-NTAG-I2C.md`

---

## Phoenix-Status

Phoenix funktioniert bereits mit dem PN532 ISO-DEP Emulation:
- NDEF Type 4 Tag (SAK=0x20), SELECT/READ BINARY APDU-Protokoll
- 8 APDUs, LNURL gelesen, Zahlung korrekt ausgelöst ✅
- Gelegentliche I2C-Bus-Abbrüche (`pn532TransactIRQ: bad, cnt=32`) — Known Issue PN532-Clone FW 1.6
- Mit NT3H2111 würde auch Phoenix zuverlässiger (kein I2C-Timing-Stress durch APDU-Fristen)
