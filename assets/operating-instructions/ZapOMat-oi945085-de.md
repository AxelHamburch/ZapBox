# ZapBox ZapOMat – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** oi945085

---

## Inhaltsverzeichnis

1. [Übersicht](#übersicht)
2. [Ansichten](#ansichten)
3. [Anschlüsse](#anschlüsse)
4. [Bedienelemente](#bedienelemente)
5. [Einrichtung und Inbetriebnahme](#einrichtung-und-inbetriebnahme)
6. [NFC-Modul (Option)](#nfc-modul-option)
7. [Technische Daten](#technische-daten)
8. [Sicherheitshinweise](#sicherheitshinweise)
9. [Weiterführende Links](#weiterführende-links)

---

## Übersicht

Die **ZapBox ZapOMat** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen. Mit einer Zahlung über das Lightning-Netzwerk lassen sich Ausgänge schalten – ideal für Automaten, Präsentationen, Eventsteuerung und viele weitere Anwendungen. 

Für umfängliche Automatisierungsaufgaben sind die Kontakte über einen 16-poligen abnehmbaren Steckverbinder nach außen geführt. Sie verfügt über einen zusätzlichen Sensoreingang, der individuell parametriert werden kann. Optional kann das NFC-Modul über die Steckerleiste extern geführt werden. 

Für den industriellen Einsatz und komfortablen Anschluss kann die ZapOMat, alternativ zum 5V USB-C-Anschluss, mit einem DC-Steckernetz betrieben werden. Die ZapOMat kann dafür optional mit einem Weitbereichseingang (DC 6V–36V) über eine Standard 5,5×2,1 mm DC-Buchse angeschlossen werden. Für die intern benötigten 5V ist ein Step-Down-Wandler vorhanden, der sich selbstregelnd ist. Das erweitert die Anwendungsmöglichkeiten enorm und die Spannungsversorgung für die Ausgänge kann dementsprechend angepasst werden.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | T-Display-S3 mit 1,9" LCD-Display |
| Frontpanel | Wahlweise mit 35° oder 90° Display Front (90° auch Einbau erhältlich) |
| Spannungsversorgung | USB-C (5V) und Weitbereichseingang DC 6V-36V (Hohlbuchse 5,5*2,1 mm) |
| Eingänge / Ausgänge | 16-polige Steckverbinder (15EDGWC-3,81mm) |
| Ausgänge | 4 Relaisausgänge CH1-CH4 - Schaltkontakte (NO/COM/NC) |
| Eingang | 1 Sensoreingang |
| Interface | Anschluss für externes NFC-Modul |
| Bedienelement | LED-Button (volle ZapBox-Funktionalität) |
| Option Schalter 1 | 3-poliger Schiebeschalter (AUTO / OFF / ON) |
| Option BTC-Ticker | Aktivierbar über den Web Installer |
| Option Boardtaster | Zwei On-board-Mikrotaster |
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |

---

## Ansichten

<img src="pic-Quattro/Quattro-oi-01.webp" alt="Frontansicht" width="67%">

*Bild 1: Frontansicht*

<img src="pic-Quattro/Quattro-oi-02.webp" alt="Rückansicht" width="67%">

*Bild 2: Rückansicht*

<img src="pic-Quattro/Quattro-oi-04.webp" alt="Draufsicht" width="67%">

*Bild 3: Draufsicht*

---

## Anschlüsse

### Eingang - USB-C Buchse zur Spannungsversorgung (5V)

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 3 A)**. Die 5V kann an der Klemme 1 (5V) und 2 (GND) der Steckleiste abgegriffen werden.

> **Hinweis:** Der Power-IN-Anschluss ist nicht „intelligent". Einige Ladegeräte oder Powermodule mit USB-C-Ausgang erkennen die ZapBox nicht und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine andere Stromquelle.

### Eingang - Hohlbuchse 5,5*2,1 mm für DC-Netzeile 6V-36V

Der ZapOMat verfügt über Anschluss für Standard Hohlstecker 5,5*2,1 mm. Die Spannung 6V-36V wird durch einen Step-Down Wandler automatisch auf die benötigte 5V gewandelt. Sie kann über den USB-Anschluss und den Klemmen 1 und 2 abgegriffen werden. Es 

> **Hinweis:** Der Power-IN-Anschluss ist nicht „intelligent". Einige Ladegeräte oder Powermodule mit USB-C-Ausgang erkennen die ZapBox nicht und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine andere Stromquelle.

---

### Eingang - USB-C Buchse am Mikrocontroller (Datenzugang, hinter rechtem Seitenpanel)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **rechten Seite der Frontblende** befindet sich eine kleine, verdeckte Klappe.
2. Öffnen Sie die Klappe, indem Sie von unten mit einem **schmalen Schraubendreher** die Klappe nach rechts wegschieben.
3. Schließen Sie ein USB-C-Kabel am darunter liegenden Anschluss des Mikrocontrollers an.

<img src="pic-Quattro/Quattro-oi-03.webp" alt="Panel öffnen und USB-C-Anschluss" width="67%">

*Bild: Panel öffnen und USB-C-Anschluss für Daten*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller ist ausschließlich zum Flashen der Firmware und zur Übertragung von Konfigurationsparametern vorgesehen. Während des Flashvorgangs darf keine Last am Ausgang angeschlossen oder geschaltet werden, da dies zu Fehlfunktionen oder zur **Beschädigung des Mikrocontrollers** führen kann.
>
> Es wird daher empfohlen:
> - während der USB-Verbindung keine Last am Ausgang anzuschließen oder
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen. Dadurch wird sichergestellt, dass der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

### Ausgänge CH1-CH4

 Der Quattro hat vier Relaisausgänge, die als Schaltkontakte (NO/COM/NC) von Außen erreichbar sind. Um an die Schrauben für die Kontaktlemmen zu kommen, muss man eine schmale Abdeckung heraushebeln. Dazu  einen kleinen Schlitzschraubendreher jeweils in die zwei Öffnungen oberhalb der Schaltkontakte einführen und die Abdeckung hochdrücken.

Die Kontakte der Relais sind für eine **maximale Strombelastung von 10 A** ausgelegt.

---

## Bedienelemente

Je nach Version verfügt die ZapBox neben dem **LED-Button** über zwei kleine **On-board-Mikrotaster**, die direkt mit dem Mikrocontroller verbunden sind. Alle Funktionen sind sowohl über den LED-Button als auch über die Mikrotaster erreichbar. Zusätzlich hat die ZapBox an der Unterseite des Frontpanels einen Reset-Taster und an der Seite zwei Schiebeschalter.

### Funktionsübersicht - Mikrotaster / LED-Button

| Funktion | Mikrotaster | LED-Button |
|---|---|---|
| Hilfe-Seite anzeigen | 1× HELP drücken | LED-Button mind. 2 Sek. gedrückt halten |
| Nächste Seite / Produktwechsel | 1× NEXT drücken | 1× LED-Button kurz drücken |
| REPORT-Seite anzeigen | 2× HELP drücken | LED-Button 3× schnell hintereinander drücken |
| Config-Modus aufrufen | NEXT mind. 5 Sek. gedrückt halten | 1× kurz drücken, dann mind. 5 Sek. gedrückt halten |

---

## Einrichtung und Inbetriebnahme

Die ZapBox wird nach der Fertigung getestet und mit der aktuellen Firmware ausgeliefert - sie ist aber nicht parametriert. Die Software wird aktiv weiterentwickelt, daher ist es empfehlenswert, die ZapBox gleich zu Beginn einmal mit der neuesten Firmware zu bespielen und dann eine Parametrierung durchzuführen. Dafür gibt es einen komfortablen **Web-Installer**.

Hier eine Schritt-für-Schritt-Anleitung für die Einrichtung:

1. Öffnet das rechte Seitenpanel der Frontblende, wie oben unter "Eingang - USB-C Buchse am Mikrocontroller" beschrieben.
2. Schließt die ZapBox an dem USB-C-Port mit einem Kabel an und verbindet es mit einem Computer. 
3. Öffnet einen Chromium Browser, zum Beispiel Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera oder [Helium](https://helium.computer/).
4. Ruft die Web-Installer Seite **https://installer.zapbox.space/** auf.
5. Flasht die aktuellste "Latest" Version, wie unter Punkt 1 des Web-Installer beschrieben.
6. Nach dem Flashvorgang schließt das kleine Fenster und geht zu Punkt 3 - Load config values. Dort wählt ihr den Button `🔌 Connect`.
7. Jetzt solltet ihr in dem grünen Feld `✅ Connected` und `✅ Config mode` sehen, vorausgesetzt die ZapBox befindet sich jetzt auch im `SERIAL CONFIG MODE`. Das Display müsste es euch anzeigen. Falls nicht, prüft einmal den Punkt 2 - Prepare connection.
8. Drei Parameter benötigt die ZapBox: `WiFi SSID` / `WiFi password` / `Device settings string`. Den Device-Settings-String bekommt ihr von eurer LNbits Wallet. Fügt dazu die Erweiterung **Bitcoin Switch** oder **ZapBox** hinzu. Die ZapBox Erweiterung unterstützt auch das NFC-Modul, ansonsten sind sie identisch.
9. Nachdem alle drei Parameter hinterlegt wurden, muss dies mit dem Button `🔥 Write Config` einmal gespeichert werden und die ZapBox mit dem Button `🔁 Restart` neu gestartet werden.

Das sollte es auch schon gewesen sein. Die ZapBox wird nach der Initialisierung den QR-Code des Produkts anzeigen und ist bereit für die erste Zahlung und anschließender Aktion am USB-Ausgang. 

Bei Fehler oder Störungen bitte auf der Web Installer Seite weiter unten die Kapitel "Error Detection & Report" und "Troubleshoot" beachten. 

---

## NFC-Modul (Option)

Je nach Ausstattung ist auf der **Oberseite der ZapBox** ein NFC-Modul verbaut. Alternativ ist das Modul auch separat erhältlich. Die ZapBox unterstützt aktuell folgende Kartentypen:

- **Boltcards** (NTAG424)
- **LNURL-Withdraw** von NTAG21x (213 / 215 / 216)

Voraussetzung für die Funktion ist die LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension). Sie muss von dem LNbits Server unterstützt werden. 

---

## Technische Daten

| Eigenschaft | Wert |
|---|---|
| Versorgungsspannung | 5 V DC über USB-C |
| Maximaler Eingangsstrom | 5,0 A |
| Schaltleistung | max. 10 A |
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

## Weiterführende Links

| Ressource | Link |
|---|---|
| Übersicht aller ZapBox-Modelle | https://zapbox.space/ |
| Web-Installer, Kurzübersicht & Fehlerbehebung | https://installer.zapbox.space/ |
| Detaillierte Dokumentation (Parameter & Funktionen) | https://ereignishorizont.xyz/zapbox/ |
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien, Bedienungsanleitungen) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
