# ZapBox Duo – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** OI939600

---

## Übersicht

Die **ZapBox Duo** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen. Mit einer Zahlung über das Lightning-Netzwerk lassen sich zwei unabhängige Ausgänge schalten – ideal für Automaten, Präsentationen, Eventsteuerung und viele weitere Anwendungen.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | T-Display-S3 mit 1,9" LCD-Display |
| Frontpanel | Optional mit 35° oder 90° Neigung |
| Eingang | USB-C (Power IN) |
| Ausgang Kanal 1 (CH1) | 30-A-Leistungsrelais mit externen Klemmen |
| Ausgang Kanal 2 (CH2) | Doppelbuchse mit USB-A und USB-C |
| Bedienelement | LED-Button (volle ZapBox-Funktionalität) |
| Schalter 1 | 3-poliger Schiebeschalter (AUTO / OFF / ON) |
| Schalter 2 | 2-poliger Schiebeschalter (Invert Output) |
| BTC-Ticker | Optional |
| Boardtaster | 2 On-board-Mikrotaster (optional) |
| NFC-Modul | Für Boltcards (optional) |

---

## Ansichten

**Bild 1: Frontansicht**

![Frontansicht](pic-Duo/Duo-oi-01.webp)

**Bild 2: Rückansicht**

![Rückansicht](pic-Duo/Duo-oi-02.webp)

---

## Inbetriebnahme

### Stromversorgung

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 5 A)**.

> **Hinweis:** Der Power-IN-Anschluss ist nicht „intelligent". Einige Ladegeräte oder Powermodule mit USB-C-Ausgang erkennen die ZapBox nicht und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine andere Stromquelle.

### Erster Test (vorkonfiguriertes Gerät)

Ist das Gerät bereits konfiguriert, können Sie sofort einen ersten Test durchführen:

1. Drücken Sie den **LED-Button**, um zwischen den Display-Seiten zu wechseln.
2. Scannen Sie mit einem Lightning-Wallet den angezeigten **QR-Code**.
3. Bezahlen Sie die Invoice.
4. Das Relais sollte hörbar schalten – der Schaltvorgang war erfolgreich.

---

## Einrichtung

### Web-Installer

Zur Ersteinrichtung und Firmware-Aktualisierung steht der **Web-Installer** zur Verfügung:

**https://installer.zapbox.space/**

Der vollständige Ablauf ist dort beschrieben. Es wird empfohlen, stets die **„Latest"-Firmware** zu flashen, um die ZapBox auf dem neuesten Stand zu halten.

### USB-Datenzugang (versteckte Klappe)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **rechten Seite der Frontblende** befindet sich eine kleine, verdeckte Klappe.
2. Öffnen Sie die Klappe, indem Sie von unten mit einem **schmalen Schraubendreher** die Klappe nach rechts wegschieben.
3. Schließen Sie ein USB-C-Kabel am darunter liegenden Anschluss des Mikrocontrollers an.

**Bild: Panel öffnen und USB-C-Anschluss für Daten**

![Panel öffnen und USB-C-Anschluss](pic-Duo/Duo-oi-03.webp)

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller dient ausschließlich zum Flashen neuer Firmware oder zur Übertragung von Konfigurationsparametern. Wenn während des Flashens gleichzeitig Schaltfunktionen ausgelöst werden, kann es zu **Fehlfunktionen oder Beschädigungen des Mikrocontrollers** kommen.
>
> Es wird daher dringend empfohlen:
> - Während des Flashens keine Schaltfunktionen auszulösen, **oder**
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen, damit der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

## Schiebeschalter

Die ZapBox Duo verfügt über zwei Schiebeschalter.

### Schalter 1 – Dreifach-Schiebeschalter (AUTO / OFF / ON)

| Stellung | Funktion |
|---|---|
| **A** (AUTO) | Automatikbetrieb – Normalbetrieb |
| **0** (OFF) | Spannungsversorgung unterbrochen – Ausgang AUS |
| **1** (ON) | Ausgang CH2 (Doppel-USB A/C) dauerhaft EIN |

### Schalter 2 – Zweifach-Schiebeschalter (Invert / Normal)

| Stellung | Funktion |
|---|---|
| **Normal** | CH2 ist im Ruhezustand spannungslos (0 V). Nach dem Schaltvorgang liegt 5 V an den USB-Buchsen an. |
| **Inv.** (Invert) | CH2 liegt im Ruhezustand auf 5 V. Nach dem Schaltvorgang wechselt der Ausgang auf 0 V (inverses Schalten). |

> **Hinweis:** Befindet sich der Zweifach-Schalter auf **Inv.** und der Dreifach-Schalter auf **Stellung 1**, ist der Ausgang – anders als im Normalbetrieb – **AUS** statt EIN.

---

## Ausgänge

### Kanal 1 (CH1) – Leistungsrelais 30 A

Das Leistungsrelais auf Kanal 1 führt die Kontakte **COM / NO / NC** nach außen. Diese sind werkseitig mit einer **Schutzkappe** abgedeckt.

- **Kappe abnehmen:** Einfach nach oben abziehen.
- **Kappe aufsetzen:** Zunächst die lange Seite auflegen, anschließend den kurzen Winkel nach unten drücken, bis die Kappe einrastet.

Die Kontakte des Leistungsrelais sind für eine **maximale Strombelastung von 30 A** ausgelegt.

Auf der **Oberseite der ZapBox** befindet sich eine kleine transparente Öffnung. Eine LED dahinter zeigt den **EIN-Status des Leistungsrelais** an.

### Kanal 2 (CH2) – Doppel-USB-Buchse (USB-A / USB-C)

Die Doppel-USB-Buchse wird über einen Relais-Schaltkontakt (CH2) geschaltet. Die **Gesamtbelastung** der Buchsen sollte **3 A nicht überschreiten**.

---

## NFC-Modul (optional)

Je nach Ausstattung ist auf der **Oberseite der ZapBox** ein NFC-Modul verbaut. Es unterstützt folgende Kartentypen:

- **Boltcards** (NTAG424)
- **LNURL-Withdraw** von NTAG21x (213 / 215 / 216)

---

## Bedienelemente

Je nach Version verfügt die ZapBox neben dem LED-Button über zwei kleine **On-board-Mikrotaster**, die direkt mit dem Mikrocontroller verbunden sind. Alle Funktionen sind sowohl über den LED-Button als auch über die Mikrotaster erreichbar.

### Funktionsübersicht

| Funktion | Mikrotaster | LED-Button |
|---|---|---|
| Hilfe-Seite anzeigen | 1× HELP drücken | LED-Button mind. 2 Sek. gedrückt halten |
| Nächste Seite / Produktwechsel | 1× NEXT drücken | 1× LED-Button kurz drücken |
| REPORT-Seite anzeigen | 2× HELP drücken | LED-Button 3× schnell hintereinander drücken |
| Config-Modus aufrufen | NEXT mind. 5 Sek. gedrückt halten | 1× kurz drücken, dann mind. 5 Sek. gedrückt halten |

---

## Technische Daten

| Eigenschaft | Wert |
|---|---|
| Versorgungsspannung | 5 V DC über USB-C |
| Maximaler Eingangsstrom | 5 A |
| CH1 Schaltleistung | max. 30 A |
| CH2 Ausgangsleistung | max. 3 A (gesamt) |
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
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien) | https://github.com/AxelHamburch/ZapBox |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
