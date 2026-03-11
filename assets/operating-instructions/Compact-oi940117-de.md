# ZapBox Compact – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** OI940117

---

## Übersicht

Die **ZapBox Compact** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen. Mit einer Zahlung über das Lightning-Netzwerk lässt sich ein Ausgang schalten – ideal für Automaten, Präsentationen, Eventsteuerung und viele weitere Anwendungen.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | T-Display-S3 mit 1,9" LCD-Display |
| Frontpanel | Wahlweise mit 35° oder 90° Display Front (90° auch zum Einbau erhältlich) |
| Eingang | USB-C (Power IN) |
| Ausgang | USB-A Buchse |
| Bedienelement | Zwei On-board-Mikrotaster |
| Schalter 1 | 3-poliger Schiebeschalter (AUTO / OFF / ON) |
| Schalter 2 | 2-poliger Schiebeschalter (Invert Output) |
| Option BTC-Ticker | Aktivierbar über den Web Installer |
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |

---

## Ansichten

<img src="pic-Compact/Compact-oi-01.webp" alt="Frontansicht" width="67%">

*Bild 1: Frontansicht*

<img src="pic-Compact/Compact-oi-02.webp" alt="Rückansicht" width="67%">

*Bild 2: Rückansicht*

---

## Inbetriebnahme

### Stromversorgung

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 5 A)**.

> **Hinweis:** Der Power-IN-Anschluss ist nicht „intelligent". Einige Ladegeräte oder Powermodule mit USB-C-Ausgang erkennen die ZapBox nicht und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine andere Stromquelle.

### Erster Test (vorkonfiguriertes Gerät)

Ist das Gerät bereits konfiguriert, können Sie sofort einen ersten Test durchführen:

1. Drücken Sie den **NEXT**, um zwischen den Display-Seiten zu wechseln.
2. Scannen Sie mit einem Lightning-Wallet den angezeigten **QR-Code**.
3. Bezahlen Sie die Invoice.
4. Das Relais sollte hörbar schalten – der Schaltvorgang war erfolgreich.

---

## Einrichtung

Die ZapBox wird getestet und mit der letzten Firmware geflasht ausgeliefert. Sie ist weder parametriert, noch kann gewährleister werden,  es zwischenzeitig Firmware herausgegeben wurd. Um sicher zu gehen den letzten Stand der Software zu haben und folgen Sie bitte diesen Leitpfaden.

1. Aufruf des Web-Installer: **https://installer.zapbox.space/**

"SERIAL CONFIG MODE"

### Web-Installer

Zur Ersteinrichtung und Firmware-Aktualisierung steht der **Web-Installer** zur Verfügung:

**https://installer.zapbox.space/**

Die letzte Firmware zum Zeitpunkt der Auslieferung ist bereits vorinstalliert. Wird die ZapBox an einen PC/Laptop angeschlossen, startet sie direkt im "Config mode". Jetzt kann man direk zum Punkt "3 Load config values" gehen und einmal den Button Connect anwählen. Die ZapBox verbindet sich mit dem Computer und sollte das auch mit Status ✅ Connected / ✅ Config mode anzeigen. 

Jetzt müssen mindestens die drei Parameter WiFi SSID / WiFi password / Device settings string eingetragen werden. Den Device settings string bekommt ihr in eurem LNbits Account unter der Extension Bitcoin Switch oder ZapBox. Wer die NFC-Funktion nutzen möchte, muss die ZapBox Extension verwenden. Anonsten sind die Funktionen identisch.  

Nach dem die Felder ausgefüllt wurden, muss man einmal den Button "Write Config" wählen, um die Daten auf die ZapBox zu schreiben. Danach einmal den "Restart" Button. Anschließend sollte die ZabBox neu durchstarten und nach ein paas Sekunden den QR-Code anzeigen. 

Es empfiehlt sich die **„Latest"-Firmware** zu flashen, um die ZapBox auf dem neuesten Stand zu halten. Die Daten bleiben erhalten, so lange man keine "Erase Device" anwählt. 

### USB-Datenzugang (versteckte Klappe)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **rechten Seite der Frontblende** befindet sich eine kleine, verdeckte Klappe.
2. Öffnen Sie die Klappe, indem Sie von unten mit einem **schmalen Schraubendreher** die Klappe nach rechts wegschieben.
3. Schließen Sie ein USB-C-Kabel am darunter liegenden Anschluss des Mikrocontrollers an.

<img src="pic-Compact/Compact-oi-03.webp" alt="Panel öffnen und USB-C-Anschluss" width="67%">

*Bild: Panel öffnen und USB-C-Anschluss für Daten*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller dient ausschließlich zum Flashen neuer Firmware oder zur Übertragung von Konfigurationsparametern. Wenn während des Flashens gleichzeitig Schaltfunktionen ausgelöst werden, kann es zu **Fehlfunktionen oder Beschädigungen des Mikrocontrollers** kommen.
>
> Es wird daher empfohlen:
> - Während des Flashens keine Schaltfunktionen auszulösen, **oder**
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen, damit der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

## Schiebeschalter

Die ZapBox Compact verfügt über zwei Schiebeschalter.

### Schalter 1 – Dreifach-Schiebeschalter (AUTO / OFF / ON)

| Stellung | Funktion |
|---|---|
| **A** (AUTO) | Automatikbetrieb – Normalbetrieb |
| **0** (OFF) | Spannungsversorgung unterbrochen – Ausgang AUS |
| **1** (ON) | Ausgang dauerhaft EIN |

### Schalter 2 – Zweifach-Schiebeschalter (Invert / Normal)

| Stellung | Funktion |
|---|---|
| **Normal** | Der Ausgang ist Ruhezustand spannungslos (0 V). Nach dem Schaltvorgang liegt 5 V an den USB-Buchsen an. |
| **Inv.** (Invert) | Der Ausgang liegt im Ruhezustand auf 5 V. Nach dem Schaltvorgang wechselt der Ausgang auf 0 V (inverses Schalten). |

> **Hinweis:** Befindet sich der Zweifach-Schalter auf **Inv.** und der Dreifach-Schalter auf **Stellung 1**, ist der Ausgang – anders als im Normalbetrieb – **AUS** statt EIN.

---

## Ausgang: USB-A-Buchse

Die USB-Buchse wird über einen Relais-Schaltkontakt geschaltet. Die **Gesamtbelastung** der Buchsen sollte **3 A nicht überschreiten**.

---

## NFC-Modul (optional)

Je nach Ausstattung ist auf der **Oberseite der ZapBox** ein NFC-Modul verbaut. Es unterstützt folgende Kartentypen:

- **Boltcards** (NTAG424)
- **LNURL-Withdraw** von NTAG21x (213 / 215 / 216)

---

## Bedienelemente

Die ZapBox verfügt über zwei kleine **On-board-Mikrotaster**, die direkt mit dem Mikrocontroller verbunden sind. Alle Funktionen sind über die Mikrotaster erreichbar. Die ZapBox hat an der Unterseite auch einen Reset Taster.

### Funktionsübersicht

| Funktion | Mikrotaster |
|---|---|
| Hilfe-Seite anzeigen | 1× HELP drücken | 
| Nächste Seite / Produktwechsel | 1× NEXT drücken |
| REPORT-Seite anzeigen | 2× HELP drücken | 
| Config-Modus aufrufen | NEXT mind. 5 Sek. gedrückt halten |

---

## Technische Daten

| Eigenschaft | Wert |
|---|---|
| Versorgungsspannung | 5 V DC über USB-C |
| Maximaler Eingangsstrom | 3,5 A |
| Ausgangsleistung | max. 3,0 A |
| Display | 1,9" LCD (T-Display-S3) |
| Kommunikation | Wi-Fi (ESP32-S3) |
| Zahlungsprotokoll | Bitcoin Lightning Network |

---

## Sicherheitshinweise

- Betreiben Sie das Gerät ausschließlich mit der angegebenen Versorgungsspannung.
- Überschreiten Sie nicht die maximalen Strombelastungen der Ausgänge.
- Führen Sie keine Arbeiten an den Relaiskontakten unter Last durch.
- Das Gerät ist nicht für den Einsatz in feuchten oder nassen Umgebungen geeignet.
- Außerhalb der Reichweite von Kindern aufbewahren.

---

---

## Weiterführende Links

| Ressource | Link |
|---|---|
| Übersicht aller ZapBox-Modelle | https://zapbox.space/ |
| Web-Installer, Kurzübersicht & Fehlerbehebung | https://installer.zapbox.space/ |
| Detaillierte Dokumentation (Parameter & Funktionen) | https://ereignishorizont.xyz/zapbox/ |
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien, Bedienungsanleitungen) | https://github.com/AxelHamburch/ZapBox |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
