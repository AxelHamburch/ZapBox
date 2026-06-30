# ZapBox Identity Mode

Identity Mode verwandelt die ZapBox in ein Identifikations-Terminal. Statt eine Zahlung auszulösen wird eine bekannte Identität (Wallet oder NFC-Karte) erkannt und ein GPIO-Relais geschaltet — ohne dass etwas bezahlt wird. Typische Anwendung: Zugangskontrolle, Zeiterfassung, personalisierter Trigger.

---

## Versionsvergleich

| Funktion | Touch 3.5" | T-Display-S3 | Headless |
|----------|:----------:|:------------:|:--------:|
| **Display** | Touchscreen 3,5" | 1,9" (170×320) | — |
| **LNURL-auth (Wallet-Login via QR)** | ✓ | ✓ | ✗ |
| **NT3H2111 NFC-Tag (Smartphone-Tap)** | ✓ | ✓ (optional) | ✗ |
| **NTAG 424 DNA (Bolt Card / Ring)** | ✓ | ✓ (optional) | ✓ |
| **4-stellige PIN nach NFC-Tap** | ✓ | ✓ | ✓ |
| **Pay+Password (klassisch, QR extern)** | ✓ | ✓ | ✓ |
| **Relay (GPIO 12 / CH01–CH06)** | CH01–CH06 | GPIO 12 | GPIO 12 |
| **180° Servo** | ✗ | ✗ | ✓ |
| **360° Servo** | ✗ | ✗ | ✓ |
| **Teach-Modus Aktivierung** | 6-Tap + Geste + PIN auf Display | Installer-PIN (Einmalig) | Installer-PIN (Einmalig) |
| **Teach-Modus Bestätigung** | Display-Toast | Display-Toast | LED — 6× Rapid Flash |
| **Teach-Modus Status** | Display-Anzeige | Display-Anzeige | LED — Doppelpuls |
| **NFC abgelehnt Feedback** | Display-Toast | Display-Toast | LED — 3× Fast Blink |
| **Teach-Timeout** | 180 s | 180 s | 180 s |
| **Dual-Page Modus (Identity + Zahlung)** | ✓ | ✓ | ✗ |
| **Startup-Mode: Selection** | ✓ | ✓ | ✓ |
| **Screensaver** | ✓ | ✓ | — |
| **Installer** | `installer/touch3.5/` | `installer/` | `installer/headless/` |

---

## Was passiert bei einem erfolgreichen Login

1. Nutzer präsentiert seine Identität (QR-Scan oder NFC-Tap)
2. ZapBox verifiziert die Identität über den LNbits-Server
3. Relay schaltet für die konfigurierte Dauer (in LNbits eingestellt)
4. **Mit Display:** Bestätigungsbildschirm (Action Time) wird kurz angezeigt
5. **Headless:** Relay schaltet still — kein visuelles Feedback außer LED-Dauerleuchten

---

## Authentifizierungsmethoden

### 1. LNURL-auth (Wallet-Login) — Touch 3.5" und T-Display-S3

Die beiden Methoden laufen **gleichzeitig** — der QR-Code für Wallet-Login ist immer sichtbar, der NFC-Leser hört parallel auf Karten.

- Nutzer scannt den QR-Code mit einer Lightning-Wallet (z. B. Zeus, Breez)
- Wallet signiert eine Challenge (k1) mit dem privaten Schlüssel → LNURL-auth (LUD-04)
- Server prüft die Signatur gegen gespeicherte public keys
- Challenge ist ~120 Sekunden gültig, wird automatisch alle 90 s erneuert

> **Headless:** LNURL-auth ist nicht verfügbar — kein Display, kein QR-Code.

### 2. NTAG 424 DNA (NFC-Tap — Ring-Login / NTAG424-Login) — alle Versionen

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

#### ZapBox Headless (ESP32 Dev Module)
- Kein Display, kein Touchscreen, keine Tasten
- PN532 NFC-Leser (für NTAG 424 DNA / Ring-Login)
- Kein NT3H2111 — LNURL-auth nicht verfügbar
- GPIO 12: Relay, 180° Servo oder 360° Servo (im Installer wählbar)
- Status-LED (GPIO 2 / GPIO 21) zeigt Teach-Modus, Enrolment und Fehler
- Teach-Modus wie T-Display-S3 über Installer-PIN (Einmalnutzung)
- Installer: `installer/headless/index.html`

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

### Touch 3.5" und T-Display-S3

| Parameter | Beschreibung | Standard |
|-----------|-------------|---------|
| **Identity Login (LNURL-auth or NTAG424)** | ENABLE / DISABLE | DISABLE |
| **Pin (GPIO triggered on success)** | Welches Relais bei Erfolg schaltet (CH01–CH06) | CH01 — GPIO 5 |
| **Activation time (ms)** | Wie lange das Relais aktiv bleibt | 1000 ms |
| **Identity trigger label** | Text neben dem QR-Code | "ZAPBOX Identity Trigger" |
| **Identity and payment trigger** | Zweite Seite mit klassischem Zahlungs-QR | Nein |
| **Teach mode** | ENABLE / DISABLE — erlaubt das Einlernen neuer Identitäten | ENABLE |
| **Teach-PIN (6 digits)** | Nur Touch 3.5": Referenzfeld (PIN ist serverseitig in zapbox_extension) | — |
| **Teach Mode — One-time PIN** | Nur T-Display-S3: 6-stellige PIN im Installer → einmaliger Teach-Boot | — |
| **NTAG 424 DNA PIN** | 4-stellige PIN-Eingabe nach jedem NFC-Tap | Ja (empfohlen) |

### Headless

| Parameter | Beschreibung | Standard |
|-----------|-------------|---------|
| **Pin (GPIO triggered on success)** | Relay / 180° Servo / 360° Servo an GPIO 12 | Relay |
| **Servo-Parameter** | Start/End-Winkel (180°) oder Speed/Duration (360°) | — |
| **Teach Mode — One-time PIN** | 6-stellige PIN im Installer → einmaliger Teach-Boot | — |
| **NTAG 424 DNA PIN** | 4-stellige PIN-Eingabe nach jedem NFC-Tap | Ja (empfohlen) |

> **Hinweis Pay+Password (Headless):** LNbits-QR-Code physisch an die ZapBox anbringen und Pay+Password in LNbits konfigurieren. Identity-Trigger (NFC) und klassische Lightning-Zahlung nutzen separate LNbits-Endpunkte und stören sich nicht gegenseitig.

---

## Teach-Modus: Neue Identitäten anmelden

### Aktivierung

**Touch 3.5":**
- **6× Tippen** auf die ZapBox-Oberfläche + **beim 6. Tap gedrückt halten**
- Eingabe der **6-stelligen Teach-PIN** auf dem Display-Tastenfeld
  - Die PIN wird serverseitig in der **zapbox_extension** gesetzt und geprüft — sie ist nicht im Gerät gespeichert
  - 3 Fehlversuche sperren den Teach-Zugang (entsperrbar in LNbits)
- Display wechselt auf den Teach-Screen: QR-Code zum Wallet-Registrieren + NFC-Leser aktiv

**T-Display-S3 und Headless:**
- Teach-PIN im **Web-Installer** eintragen (Feld *Teach Mode — One-time PIN*, 6 Stellen)
- PIN muss mit der Teach-PIN in der **zapbox_extension** übereinstimmen
- Gerät neu starten (Write Config → Restart) — Teach-Modus startet **automatisch einmalig** nach dem Booten
- PIN wird sofort aus dem Flash gelöscht (Einmalnutzung) → kein erneuter Start beim nächsten Boot
- **T-Display-S3:** Display zeigt Teach-Screen mit QR + NFC-Leser aktiv
- **Headless:** Kein Display — Status über LED (Doppelpuls = Teach aktiv, s. LED-Tabelle)

### Wallet (LNURL-auth) anmelden — Touch 3.5" und T-Display-S3

1. Teach-Screen zeigt QR mit `action=register`
2. Wallet scannt QR → registriert Public Key auf dem Server
3. Display bestätigt "Wallet registered"
4. Nächste Wallet kann sofort registriert werden (QR wird automatisch erneuert)

> **Headless:** LNURL-auth nicht verfügbar. Nur NFC-Karten können enrollt werden.

### NFC-Karte / Ring anmelden — alle Versionen

1. Im Teach-Modus Karte/Ring an den PN532-Leser halten
2. ZapBox liest SUN-Parameter aus → sendet an TagID-Server zum Enroll
3. **Mit Display:** grüner Toast **"NFC card enrolled"**  
   **Headless:** LED zeigt 6× Rapid Flash (50 ms ON/OFF)
4. Fehler (Karte nicht in TagID / Server nicht erreichbar):  
   **Mit Display:** roter Toast **"Card not enrolled"**  
   **Headless:** kein separates Fehler-Signal — Teach-Modus läuft weiter
5. Enrolment in LNbits zapbox_extension prüfen (ggf. CTRL+F5)

### Beenden
| Methode | Touch 3.5" | T-Display-S3 | Headless |
|---------|:----------:|:------------:|:--------:|
| Taste / Button | CANCEL auf Display | NEXT-Taste | — |
| Automatischer Timeout | 180 s | 180 s | 180 s |
| Server-Event | `teach_ended` WS | `teach_ended` WS | `teach_ended` WS |
| Spannungsversorgung trennen | ✓ | ✓ | ✓ |

---

## Headless LED-Status

Da die Headless-Version kein Display hat, signalisiert die Status-LED alle relevanten Zustände:

| LED-Muster | Timing | Bedeutung |
|------------|--------|-----------|
| **Doppelpuls** | 150ms ON / 100ms OFF / 150ms ON / 1,5s Pause (1,9s Zyklus) | Teach-Modus aktiv — wartet auf Karte |
| **6× Rapid Flash** | 50ms ON/OFF × 6 (600ms) | Karte/Wallet erfolgreich enrollt |
| **3× Fast Blink** | 100ms ON/OFF × 3, dann Dauerleuchten | NFC-Karte nicht erkannt oder abgelehnt (tagid 404) |
| **Dauerleuchten** | Konstant AN | Bereit — wartet auf NFC-Tap |
| **Langsames Blinken** | 1 Hz | Config-Modus aktiv |

> Enrolment-Ergebnis immer in der LNbits zapbox_extension prüfen. Ein CTRL+F5 Seiten-Refresh kann nötig sein.

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

> **Headless + Pay+Password:** Da die Headless-ZapBox kein Display hat, muss der LNbits-QR-Code physisch an der ZapBox angebracht werden. Pay+Password und Identity-NFC-Trigger sind vollständig unabhängig voneinander — beide nutzen separate LNbits-Endpunkte.

---

### LNURL-auth — Details (Touch 3.5" und T-Display-S3)

| Aspekt | Eigenschaft |
|--------|------------|
| Replay-Schutz | k1 ist Einmal-Challenge (~120 s gültig, alle 90 s erneuert) |
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

**Fehlermeldungen:**
| Meldung | Anzeige | Bedeutung |
|---------|---------|-----------|
| `Wrong PIN / N tries left / Tap card again` | Display-Toast | Falsche PIN, Versuche verbleibend |
| `NFC tag unknown` | Display-Toast / LED 3× Blink | Karte nicht in der Allowlist (nicht enrollt) |
| `NFC Identity Failed` | Display-Toast / LED 3× Blink | Auth allgemein fehlgeschlagen (CMAC-Fehler, Verbindungsproblem) |
| `NFC card enrolled` | Display-Toast / LED 6× Flash | Karte erfolgreich enrollt (Teach-Modus) |
| `Card not enrolled` | Display-Toast | Teach fehlgeschlagen — Karte nicht in TagID |
| `Wallet registered` | Display-Toast | LNURL-auth Wallet erfolgreich registriert |
| `IDENTITY LOGIN DISABLED` | Display-Toast | Server hat 403 zurückgegeben |

---

## Zusammenspiel der Erweiterungen

```
ZapBox (Firmware)
    │
    ├── LNURL-auth ──► zapbox_extension  ──► LNbits Wallet
    │   (Touch / T-S3)  (v2.5.0+)              (Identitäten, k1)
    │
    └── NFC SUN tap ──► zapbox_extension ──► tagid_extension
        (alle Versionen)  /api/v1/nfc/auth       (AES-CMAC Prüfung,
                          /api/v1/nfc/teach        Allowlist, PIN)
```

- **zapbox_extension** ist der zentrale Koordinator: stellt Auth-URLs bereit, prüft LNURL-auth-Signaturen, leitet NFC-Anfragen an TagID weiter
- **tagid_extension** ist zuständig für NTAG 424 DNA: verwaltet Karten-Allowlist, prüft CMAC, optional auch PIN-Validierung
- Die ZapBox kommuniziert nur mit der zapbox_extension — tagid läuft serverseitig transparent dahinter

---

## Screensaver-Verhalten (Touch 3.5" und T-Display-S3)

Ist der **Screensaver** aktiv (Display-Hintergrundlicht aus), gilt:
- **Erster Touch** weckt nur das Display — keine Aktion wird ausgelöst
- **Zweiter Touch** arbeitet normal (Button, Cancel, PIN-Eingabe etc.)

Dies verhindert, dass unbeabsichtigt ein Button getroffen wird, wenn der Bildschirm schwarz ist.

---

## Startup-Mode: Selection

Mit dem Startup-Mode **"Selection — Mode selection at startup"** kann zwischen mehreren Modi gewählt werden. Dabei wird der Identity-Modus als eine der Optionen angeboten — nützlich wenn die ZapBox auch als Mini-PoS genutzt wird.

Mini-PoS und Identity Mode können auf derselben ZapBox abwechselnd genutzt werden; NTAG 424 DNA Karten funktionieren in beiden Modi.
