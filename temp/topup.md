# Bolt Card Top-Up via ATM – Konzept & Analyse

## Idee

Ein ATM (z.B. Münzautomat) soll Bolt Cards **aufladen** können:
1. User wirft Münzen ein → ATM kennt den Sats-Betrag
2. User hält Bolt Card an NFC-Reader des ATM
3. ATM liest die lnurlw:// URL der Karte (SUN-verschlüsselt)
4. Sats werden auf das Bolt Card Wallet übertragen

---

## Variante 1: Bestehender LUD19 Refund-Mechanismus (funktioniert heute)

### Flow

```
Münzen eingeworfen → ATM hat X Sats zum Verteilen
    ↓
User hält Bolt Card an NFC-Reader des ATM
    ↓
ATM liest lnurlw:// URL von der Karte (SUN-verschlüsselt)
    ↓
ATM ruft GET /boltcards/api/v1/scan/{external_id}?p=...&c=... auf
    ↓
Bolt Cards Extension:
  ✓ Entschlüsselt SUN (K1), validiert MAC (K2)
  ✓ Counter-Check (Anti-Replay)
  ✓ Erstellt Hit-Record
  ✓ Gibt LnurlWithdrawResponse zurück MIT payLink (LUD19)
    ↓
ATM IGNORIERT den Withdraw-Teil (callback, k1)
ATM NUTZT den payLink: "lnurlp://server/boltcards/api/v1/lnurlp/{hit_id}"
    ↓
ATM ruft GET /api/v1/lnurlp/{hit_id} → bekommt LnurlPayResponse
    ↓
ATM ruft GET /api/v1/lnurlp/cb/{hit_id}?amount={münzen_in_msat}
    ↓
Extension erstellt Invoice auf dem Card-Wallet
    ↓
ATM bezahlt die Invoice → Sats landen im Bolt Card Wallet ✅
    ↓
Background Task (tasks.py) erstellt Refund-Record
```

### Relevante Endpoints (boltcards Extension)

| Endpoint | Datei | Zweck |
|----------|-------|-------|
| `GET /api/v1/scan/{external_id}?p=&c=` | views_lnurl.py Z.46-107 | Kartenscan → LnurlWithdrawResponse mit payLink |
| `GET /api/v1/lnurlp/{hit_id}` | views_lnurl.py Z.269-293 | LNURL-Pay Response (min/max Sats, callback) |
| `GET /api/v1/lnurlp/cb/{hit_id}?amount=` | views_lnurl.py Z.234-265 | Erstellt Invoice auf Card-Wallet |
| Background Task | tasks.py Z.14-33 | Erkennt bezahlte Invoice → erstellt Refund-Record |

### Einschränkungen

1. **Verschwendeter Hit-Record**: Scan erstellt immer einen Hit + inkrementiert Card-Counter. Da der Withdraw nie ausgelöst wird, bleibt `spent=false, amount=0` → "verschmutzt" die Hit-Tabelle.

2. **tx_limit als Obergrenze für Pay**: Der LNURL-Pay Callback prüft:
   ```python
   if int(amount) > int(card.tx_limit) * 1000:
       return LnurlErrorResponse(reason="Amount too high.")
   ```
   Das tx_limit ist eigentlich das Abhebe-Limit, begrenzt aber auch den Top-Up-Betrag.

3. **Daily-Limit**: Wird nur beim Scan geprüft, summiert aber nur gespente Hits. Ungespente Top-Up-Hits zählen nicht dagegen → kein Problem für Top-Up.

4. **Kein doppelter Tap-Schutz**: ATM muss selbst Card-Removal-Detection implementieren (2 consecutive absent polls wie ZapBox).

---

## Variante 2: Dedizierter Top-Up Endpoint (saubere Lösung)

### Vorteile gegenüber Variante 1
- Kein unnötiger Hit-Record
- Kein Counter-Inkrement für "leeren" Withdraw
- Eigenes Top-Up-Limit (unabhängig von tx_limit)
- Saubere Datenbank (keine spent=false Geister-Hits)

### Vorgeschlagener Endpoint

```
GET /boltcards/api/v1/topup/{external_id}?p=...&c=...
```

### Verhalten
1. SUN entschlüsseln (K1), MAC validieren (K2) – identisch zum Scan
2. Counter validieren + aktualisieren – identisch zum Scan
3. **Kein Hit erstellen**
4. **Kein Withdraw anbieten**
5. Direkt `LnurlPayResponse` zurückgeben

### Geschätzter Aufwand
~30 Zeilen neuer Code in `views_lnurl.py`:
- Neuer GET-Endpoint `/api/v1/topup/{external_id}`
- SUN-Validierung (Copy von Scan-Endpoint)
- Direkte LnurlPayResponse (ohne Hit-Umweg)
- Callback-Endpoint für Invoice-Erstellung

Optional:
- Eigenes `topup_limit` Feld im Card-Model (Migration nötig)
- Eigene `TopUp`-Tabelle statt Refund-Records

---

## Kryptographie-Hintergrund (NXP NTAG424 DNA)

Die Bolt Card erzeugt bei jedem Tap eine **SUN-Nachricht** (Secure Unique NFC):

```
URL: lnurlw://server/boltcards/api/v1/scan/{external_id}?p={encrypted}&c={mac}
```

| Parameter | Inhalt | Schlüssel |
|-----------|--------|-----------|
| `p` | AES-CBC verschlüsselt: UID (7 Bytes) + Counter (3 Bytes) | K1 |
| `c` | CMAC (SV2-Prefix + UID + Counter) | K2 |

- **K0**: Diversification Key (Karten-Programmierung)
- **K1**: Encryption Key (SUN-Entschlüsselung)
- **K2**: MAC Key (Integritätsprüfung)
- **Counter**: Strikt aufsteigend → verhindert Replay-Angriffe

Relevanter Code: `boltcards/nxp424.py`
- `decrypt_sun(sun, key)` → (uid, counter)
- `get_sun_mac(uid, counter, key)` → mac bytes

---

## Referenzen

- **LUD19**: LNURL-Pay in Withdraw-Response (payLink) – https://github.com/lnurl/luds/blob/luds/19.md
- **Bolt Cards Extension**: `d:\VSCode\boltcards\`
- **ZapBox NFC Implementation**: `d:\VSCode\ZapBox\src\NFCBoltCard.cpp` / `NFCPN532.cpp`
- **ZapBox README Payment Flow**: `d:\VSCode\ZapBox\README.md` ab Zeile 674
