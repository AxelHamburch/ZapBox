# ZapBox Identity Mode

Identity Mode verwandelt die ZapBox in ein Identifikations-Terminal. Statt eine Zahlung auszulösen wird eine bekannte Identität (Wallet oder NFC-Karte) erkannt und ein GPIO-Relais geschaltet — ohne dass etwas bezahlt wird. Typische Anwendung: Zugangskontrolle, Zeiterfassung, personalisierter Trigger.

---

## Was passiert bei einem erfolgreichen Login

1. Nutzer präsentiert seine Identität (QR-Scan oder NFC-Tap)
2. ZapBox verifiziert die Identität über den LNbits-Server
3. Relay schaltet für die konfigurierte Dauer (Standard: 1000 ms)
4. Display zeigt kurz den Bestätigungsbildschirm (Action Time)

---

## Zwei Authentifizierungsmethoden

Die beiden Methoden laufen **gleichzeitig** — der QR-Code für Wallet-Login ist immer sichtbar, der NFC-Leser hört parallel auf Karten.

### 1. LNURL-auth (Wallet-Login)
- Nutzer scannt den QR-Code mit einer Lightning-Wallet (z. B. Zeus, Breez)
- Wallet signiert eine Challenge (k1) mit dem privaten Schlüssel → LNURL-auth (LUD-04)
- Server prüft die Signatur gegen gespeicherte public keys
- Challenge ist ~120 Sekunden gültig, wird automatisch erneuert

### 2. NTAG 424 DNA (NFC-Tap — Ring-Login / NTAG424-Login)
- Nutzer tippt eine Bolt Card, Bolt Ring oder NTAG 424 DNA Karte an den PN532-Leser
- Karte liefert verschlüsselte SUN-Parameter (`p` = PICC Block, `c` = CMAC)
- Server prüft AES-CMAC und Replay-Schutz (Zähler) via **TagID Extension**
- Optional: zusätzliche 4-stellige PIN-Eingabe nach dem Tap (empfohlen)

#### Unterstützte NFC-Kartenformate
| Format | URL-Präfix | Anmerkung |
|--------|-----------|-----------|
| TagID Bolt Card | `lnurlw://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` | Standard |
| HTTPS Bolt Card | `https://server/tagid/api/v1/scan/{ext_id}?p=…&c=…` | Alternative |

Beide Formate werden erkannt — die ZapBox prüft zuerst auf SUN-Parameter und leitet erst danach zur Zahlungslogik weiter.

---

## Voraussetzungen

### Hardware-Varianten

#### ZapBox Touch 3.5" (ESP32-S3, JC3248W535C)
- Touchscreen-Display (3,5")
- PN532 NFC-Leser (für NTAG 424 DNA / Ring-Login)
- NT3H2111 NFC-Tag (LNURL-auth per Smartphone-Tap)
- Relais an einem der 6 GPIO-Ausgänge (CH01–CH06)
- Installer: `installer/touch3.5/index.html`

#### ZapBox T-Display-S3 (ESP32-S3, LilyGo T-Display-S3)
- 1,9" Display (170×320)
- PN532 NFC-Leser (für NTAG 424 DNA / Ring-Login, optional)
- NT3H2111 NFC-Tag Modul (MikroE NFC Tag 2 Click, optional — ermöglicht LNURL-auth per Smartphone-Tap)
- Relais an GPIO 12 (CH01)
- Teach-Modus wird über den Web-Installer gestartet (6-stellige PIN im Installer setzen → Gerät startet einmalig in Teach-Modus)
- Kein Touchscreen — Navigation per NEXT-Button
- Installer: `installer/index.html`

### Software / Dienste
- **LNbits** (selbst gehostet oder Cloud)
- **zapbox_extension v2.5.0+** — verwaltet Identitäten, stellt LNURL-auth-Endpunkte bereit
- **tagid_extension v2.1.0+** — für NTAG 424 DNA Verifizierung (Ring-Login)
  - Benötigt: *TagID Base URL* und *TagID Invoice Key* (Wallet invoice/read key) in der ZapBox-Instanzkonfiguration

### Firmware
- ZapBox Firmware mit aktiviertem `multiControl = authy` (Installer: **Identity🫆Login — LNURL-auth & NFC Tag**)

---

## Konfiguration im Web-Installer

Der Identity-Bereich im Installer ist standardmäßig **zugeklappt**. Er erscheint wenn der Modus auf *Identity* oder *Selection* gestellt ist, muss aber explizit aufgeklappt werden.

### Hauptschalter
| Feld | Beschreibung | Standard |
|------|-------------|---------|
| **Identity Login (LNURL-auth or NTAG424)** | ENABLE / DISABLE — klappt alle Felder auf/zu | DISABLE |

### Allgemeine Trigger-Einstellungen (sichtbar nach ENABLE)
| Parameter | Beschreibung | Standard |
|-----------|-------------|---------|
| **Auth Pin (GPIO)** | Welches Relais bei Erfolg schaltet (CH01–CH06) | CH01 — GPIO 5 |
| **Activation time (ms)** | Wie lange das Relais aktiv bleibt | 1000 ms |
| **Identity trigger label** | Text neben dem QR-Code (3 Zeilen: Wort 1 / Wort 2 / Rest) | "ZAPBOX Identity Trigger" |
| **Identity and payment trigger** | Zweite Seite mit klassischem Zahlungs-QR | Nein |

### Teach-Modus-Einstellungen
| Parameter | Beschreibung | Standard |
|-----------|-------------|---------|
| **Teach mode** | ENABLE / DISABLE — erlaubt das Einlernen neuer Identitäten per 6-Tap-Geste | ENABLE |
| **Teach-PIN (6 digits)** | Referenzfeld — nur zur Erinnerung; die PIN wird **serverseitig** in der zapbox_extension verwaltet und dort gesetzt | — |

### LNURL-auth Identities (Lightning Wallets)
Kein zusätzliches Installer-Feld — Wallet-Identitäten werden direkt im Teach-Modus auf dem Gerät registriert (Wallet scannt den Teach-QR).

### NFC Identities – NTAG424 (Bolt Card, Ring, etc.)
*Benötigt TagID🔐 Extension*

| Parameter | Beschreibung | Standard |
|-----------|-------------|---------|
| **NTAG 424 DNA PIN** | 4-stellige PIN-Eingabe nach jedem NFC-Tap | Ja (empfohlen) |

### Dual-Page Modus
Mit **"Identity and payment trigger: Yes"** zeigt die ZapBox zwei Seiten:
- **Seite 1**: Identity-Login QR (LNURL-auth) — Standard-Ansicht
- **Seite 2**: Zahlungs-QR (Lightning-Invoice) — über Tab-Button erreichbar

Beide Seiten triggern denselben GPIO-Pin.

---

## Teach-Modus: Neue Identitäten anmelden

### Aktivierung

**Touch 3.5":**
- **6× Tippen** auf die ZapBox-Oberfläche + **beim 6. Tap gedrückt halten**
- Eingabe der **6-stelligen Teach-PIN** auf dem Display-Tastenfeld
  - Die PIN wird serverseitig in der **zapbox_extension** gesetzt und geprüft — sie ist nicht im Gerät gespeichert
  - 3 Fehlversuche sperren den Teach-Zugang (entsperrbar in LNbits)
- Display wechselt auf den Teach-Screen: QR-Code zum Wallet-Registrieren + NFC-Leser aktiv

**T-Display-S3:**
- Teach-PIN im **Web-Installer** eintragen (Feld *Teach PIN*, 6 Stellen)
- Gerät neu starten — Teach-Modus startet **automatisch einmalig** nach dem Booten
- PIN wird sofort aus dem Flash gelöscht (Einmalnutzung) → kein erneuter Start beim nächsten Boot
- Display zeigt Teach-Screen mit QR + NFC-Leser aktiv
- Beenden: **NEXT-Taste** drücken → Gerät startet neu in Normalbetrieb

### Wallet (LNURL-auth) anmelden
1. Teach-Screen zeigt QR mit `action=register`
2. Wallet scannt QR → registriert Public Key auf dem Server
3. Display bestätigt "Wallet registered"
4. Nächste Wallet kann sofort registriert werden (QR wird automatisch erneuert)

### NFC-Karte / Ring anmelden
1. Im Teach-Modus Karte/Ring an den NFC-Leser halten
2. ZapBox liest SUN-Parameter aus → sendet an TagID-Server zum Enroll
3. Display zeigt grünen Toast: **"NFC card enrolled"**
4. Fehler (Karte unbekannt / Server nicht erreichbar): roter Toast **"Card not enrolled"**

### Beenden
- **CANCEL-Button** auf dem Display
- Automatisch nach **5 Minuten** (Backup-Timeout)
- Server sendet `teach_ended` WebSocket-Event

---

## Sicherheit

### Übersicht der Methoden

| Methode | Faktor | Replay-Schutz | Brute-Force-Schutz | Stärke |
|---------|--------|-------------|-------------------|--------|
| **Pay+Password** | Zahlung + PIN | Einmaliger Invoice | Zahlung kostet Sats | Einfach |
| **LNURL-auth** | Wallet (privater Schlüssel) | k1 ist Einmal-Challenge | Kryptografisch | Mittel-Hoch |
| **NTAG 424 DNA** | Hardware-Karte (Besitz) | AES-CMAC + Zähler | Kein Klonen möglich | Mittel |
| **NTAG 424 DNA + PIN** | Karte + Wissen | AES-CMAC + Zähler | Hardware + PIN | Hoch |

> **Hinweis zu den Stärken:**  
> LNURL-auth ist kryptografisch stark (secp256k1) und profitiert indirekt von Gerätesperre + Wallet-PIN — in der Praxis eher 1,5 Faktoren.  
> NTAG 424 DNA *ohne* PIN verhindert zuverlässig das Klonen, aber Kartenverlust oder -diebstahl genügt für Zugang — ein einziger Faktor (Besitz).

---

### Pay+Password — Einfache Zugangssicherung ohne Identity Mode

Die klassische ZapBox-Funktion kann als einfachen Sicherheitsmechanismus genutzt werden, ohne den vollständigen Identity Mode zu aktivieren.

**Funktionsweise:**
- ZapBox zeigt einen Lightning-Invoice-QR-Code (z. B. 10 Sats)
- Nutzer bezahlt mit seiner Wallet → Zahlung ist der erste Faktor (Beweis der Wallet)
- Anschließend erscheint ein **Passwort-/PIN-Dialog** (LNbits "comment" / "variable amount" Mechanismus)
- Erst wenn Zahlung **und** korrektes Passwort übereinstimmen, schaltet das Relay

**Sicherheitseigenschaften:**
- **Replay-Schutz:** Jeder Invoice ist einmalig — ein abgefangener QR-Scan kann nicht wiederverwendet werden
- **Brute-Force-Schutz:** Jeder Versuch kostet Sats — automatisiertes Durchprobieren ist wirtschaftlich unattraktiv
- **Zwei Faktoren:** Wallet (Besitz) + Passwort (Wissen)
- **Kein Server-Setup:** Läuft allein mit LNbits-Webhook, keine zusätzliche Extension nötig

**Einsatz-Beispiel:** ZapSave ("Pay+Password Trigger") — Nutzer zahlt 10 Sats + gibt ein geteiltes Passwort ein, Relay öffnet Tür oder Schrank.

**Grenzen:** Die Identität des Nutzers ist nicht fest zugewiesen — jeder mit dem Passwort (und Sats) kann triggern. Es gibt keine Allowlist, kein Audit-Log pro Person.

---

### LNURL-auth — Details

| Aspekt | Eigenschaft |
|--------|------------|
| Replay-Schutz | k1 ist Einmal-Challenge (~120 s gültig) |
| Fälschungssicherheit | Kryptografische Signatur (secp256k1) |
| Zusatz-PIN | Nicht vorgesehen |
| Identitätszuweisung | Ja — jede Wallet hat einzigartigen Public Key |

---

### NTAG 424 DNA — Details

| Aspekt | Eigenschaft |
|--------|------------|
| Replay-Schutz | AES-CMAC + Zähler (SUN-Mechanismus) — jeder Tap einmalig |
| Fälschungssicherheit | Hardware-gesicherter AES-128-Schlüssel |
| Zusatz-PIN | Optional (4-stellig, empfohlen) |
| Kartenklonen | Klon hat anderen Zähler → wird abgelehnt |

**Ohne PIN:** Wer die physische Karte hat, kann sich einloggen.  
**Mit PIN:** Karte + PIN erforderlich — deutlich sicherer.

**Privacy UID (optional):**  
Der NTAG 424 DNA kann so konfiguriert werden, dass er bei jedem Tap eine zufällige Luft-UID aussendet (statt einer statischen). Das verhindert passives Tracking — ein fremdes NFC-Lesegerät kann die Karte nicht wiedererkennen. Die Authentifizierung bleibt davon unberührt: die echte UID steckt weiterhin AES-verschlüsselt im `p`-Parameter und wird serverseitig korrekt entschlüsselt. Privacy UID ist eine **Datenschutz-Maßnahme**, kein Ersatz für PIN. Aktivierung über die Bolt Card Programmer App (ab v0.1.4) — **irreversibel**.

**Fehlermeldungen auf dem Display:**
- `Wrong PIN / N tries left / Tap card again` — falsche PIN, Versuche verbleibend
- `NFC tag unknown` — Karte nicht in der Allowlist (nicht enrollt)

---

## Zusammenspiel der Erweiterungen

```
ZapBox (Firmware)
    │
    ├── LNURL-auth ──► zapbox_extension  ──► LNbits Wallet
    │                  (v2.5.0+)              (Identitäten, k1)
    │
    └── NFC SUN tap ──► zapbox_extension ──► tagid_extension
                        /api/v1/nfc/auth       (AES-CMAC Prüfung,
                        /api/v1/nfc/teach       Allowlist, PIN)
```

- **zapbox_extension** ist der zentrale Koordinator: stellt Auth-URLs bereit, prüft LNURL-auth-Signaturen, leitet NFC-Anfragen an TagID weiter
- **tagid_extension** ist zuständig für NTAG 424 DNA: verwaltet Karten-Allowlist, prüft CMAC, optional auch PIN-Validierung
- Die ZapBox kommuniziert nur mit der zapbox_extension — tagid läuft serverseitig transparent dahinter

---

## Statusmeldungen auf dem Display

| Meldung | Bedeutung |
|---------|-----------|
| `NFC card enrolled` | Karte erfolgreich im Teach-Modus registriert (grün) |
| `Card not enrolled` | Teach fehlgeschlagen — Karte nicht in TagID oder Session abgelaufen (rot) |
| `NFC tag unknown` | Auth-Tap: Karte nicht in der Allowlist (nicht enrollt) |
| `NFC Identity Failed` | Auth-Tap allgemein fehlgeschlagen (CMAC-Fehler, Verbindungsproblem) |
| `Wallet registered` | LNURL-auth Wallet erfolgreich registriert |
| `IDENTITY LOGIN DISABLED` | Server hat 403 zurückgegeben (Identities in Extension deaktiviert) |

---

## Screensaver-Verhalten

Ist der **Screensaver** aktiv (Display-Hintergrundlicht aus), gilt:
- **Erster Touch** weckt nur das Display — keine Aktion wird ausgelöst
- **Zweiter Touch** arbeitet normal (Button, Cancel, PIN-Eingabe etc.)

Dies verhindert, dass unbeabsichtigt ein Button getroffen wird, wenn der Bildschirm schwarz ist.

---

## Startup-Mode: Selection

Mit dem Startup-Mode **"Selection — Mode selection at startup"** kann zwischen mehreren Modi gewählt werden. Dabei wird der Identity-Modus als eine der Optionen angeboten — nützlich wenn die ZapBox auch als Mini-PoS genutzt wird.

Mini-PoS und Identity Mode können auf derselben ZapBox abwechselnd genutzt werden; NTAG 424 DNA Karten funktionieren in beiden Modi.
