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
| Option BTC-Ticker | Aktivierbar über den Web Installer |
| Option Boardtaster | Zwei On-board-Mikrotaster |
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |
| Option 3-poliger Schalter | Schiebeschalter (AUTO / OFF / ON) |

---

## Ansichten

<img src="pic-Quattro/Quattro-oi-01.webp" alt="Frontansicht" width="67%">

*Bild 1: Frontansicht*

<img src="pic-Quattro/Quattro-oi-02.webp" alt="Rückansicht" width="67%">

*Bild 2: Rückansicht*

<img src="pic-Quattro/Quattro-oi-04.webp" alt="Draufsicht" width="67%">

*Bild 3: Draufsicht*

---

## Spannungsversorgung

Die ZapBox ZapOMat verfügt über einen USB-C-Anschluss sowie über eine DC-Hohlsteckerbuchse (5,5 × 2,1 mm) für gängige DC-Netzteile.

Die DC-Buchse ist als Weitbereichseingang für Spannungen von 6 V bis 36 V ausgelegt. Die Eingangsspannung wird intern über einen Step-Down-Converter auf 5 V geregelt. Der Spannungswandler kann im Dauerbetrieb bis zu 1 A liefern. Die Wärmeentwicklung ist dabei zu beachten ⚠️. Kurzzeitig sind auch 2 A möglich, in sehr kurzen Lastspitzen bis zu 2,5 A.

Wird dauerhaft eine Leistungsabgabe von mehr als 1 A benötigt, wird empfohlen, den USB-C-Power-Anschluss mit einem separaten 5-V-Netzteil zu verwenden. In diesem Fall sind für die Summe aller Verbraucher bis zu 3 A möglich.

Da einzelne Verbraucher, wie z. B. LED-Streifen, hohe Ströme (> 1 A) aufnehmen können, ist auch eine kombinierte Einspeisung möglich. Beispielsweise kann die ZapBox über die DC-Buchse mit 12 V versorgt werden und gleichzeitig den Relaiskontakt am Ausgang 4 mit dieser Spannung speisen. Der interne Spannungswandler versorgt dabei das System sowie die Relaisausgänge 1–3 mit 5 V, während eine 12-V-LED-Leiste am Ausgang 4 direkt mit 12 V betrieben wird. Dadurch wird die Belastung des internen Spannungswandlers reduziert.

Zusätzlich kann neben der DC-Versorgung (6 V–36 V) ein 5-V-Netzteil mit maximal 3 A am USB-C-Power-Anschluss angeschlossen werden.
Der gleichzeitige Betrieb beider Versorgungen ist möglich, jedoch sind Unterschiede der 5-V-Spannungen zu beachten, um gegenseitige Beeinflussung oder Rückspeisung der Netzteile zu vermeiden.

### Spannungseingang USB-C Buchse (5V) 

Die ZapBox kann über den USB-C-Anschluss **Power** mit **5 V DC (max. 3 A)** versorgt werden. Über diesen Anschluss können die 5 V ebenfalls abgegriffen werden, wenn die Spannungsversorgung über die DC-Hohlsteckerbuchse erfolgt.

> **Hinweis:** Der USB-Power-Anschluss unterstützt keine automatische USB-C-Leistungsanforderung (kein USB-C Power Delivery). Einige USB-C-Ladegeräte oder Powermodule erkennen die ZapBox daher nicht als Verbraucher und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine alternative 5-V-Stromquelle. Die maximale Stromstärke darf 3 A nicht überschreiten.

### Spannungseingang DC-Hohlsteckerbuchse (6 V–36 V)

Die DC-Buchse ist für Hohlstecker des Typs 5,5 × 2,1 mm ausgelegt und eignet sich für gängige Netzteile, z. B. mit 12 V oder 24 V Ausgangsspannung.

Intern besteht die Möglichkeit, die Eingangsspannung abzugreifen und auf die Steckerleiste zu verdrahten, um die Ausgangskanäle direkt mit der Eingangsspannung zu versorgen. Dies erfordert einen Eingriff in das Gerät und darf ausschließlich von elektrotechnischen Fachkräften durchgeführt werden. Details hierzu sind im E-Layout der ZapBox ZapOMat beschrieben.

> **Hinweis:** Der interne Spannungswandler besitzt eine begrenzte Leistungsfähigkeit. Bei hohen Lastströmen entsteht erhöhte Wärmeentwicklung, die zu einer Erwärmung des Geräts führen kann. Sorgen Sie für ausreichende Kühlung und überschreiten Sie die angegebenen Stromgrenzen nicht.

## Externe Anschluss - 16-poliger Steckverbinder

Die ZapBox verfügt über einen abnehmbaren 16-poligen Steckverbinder.

**Klemmenbelegung in der Übersicht:**
| Anschluss | Funktion | Verwendung | Hinweis |
|------|----------|-----------|---------------|
| 1 | 5 V | Ausgang (intern versorgt) | Spannungsversorgung 5 V |
| 2 | GND/Masse | Ausgang (intern versorgt) | Masseanschluss 0 V |
| 3 | Versorgung Relaisausgänge Kanal 1-3 | Eingang / Ausgang | 5 V, 6 V–36 V |
| 4 | Schaltkontakt (NO) Relais Kanal 1 | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 5 | Schaltkontakt (NO) Relais Kanal 2 | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 6 | Schaltkontakt (NO) Relais Kanal 3 | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 7 | GND | Ausgang | Masseanschluss 0 V (intern versorgt) |
| 8 | Versorgung Relaisausgänge Kanal 4 | Eingang / Ausgang | 5 V, 6 V–36 V |
| 9 | Schaltkontakt (NO) Relais Kanal 4 | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 10 | GND| Ausgang | Masseanschluss 0 V (intern versorgt) |
| 11 | Sensor | Eingang | GPIO Pin 2 für z.B. eine Lichtschranke (NPN) |
| 12 | 5 V | Ausgang | 5 V für Sensor und NFC-Modul (intern versorgt) |
| 13 | GND | Ausgang | Masseanschluss für Sensor und NFC-Modul (intern versorgt) |
| 14 | NFC IRQ | Schnittstelle | GPIO Pin 1 an externes NFC-Modul |
| 15 | I2C SCL | Schnittstelle | GPIO Pin 17 an externes NFC-Modul |
| 16 | I2C SDA | Schnittstelle | GPIO Pin 18 an externes NFC-Modul |

**Hinweis Spannungsverteilung mit Brücken:** 

Im Auslieferungszustand ist intern eine 5-V-Versorgung auf die Klemmen 1 (5 V) und 2 (GND) gebrückt. Zusätzlich ist Klemme 12 (Versorgung Sensor und NFC) intern mit 5 V verbunden.

Die Masse (GND) ist intern auf die Klemmen 2, 7, 10 und 13 gebrückt.

Die Relaiskontakte besitzen keine interne Versorgungsspannung. Die gewünschte Schaltspannung muss extern aufgebrückt werden.
Um beispielsweise einen 5-V-Verbraucher zu schalten, muss für die Kanäle 1–3 eine externe Brücke von Klemme 1 auf Klemme 3 gesetzt werden. Für den Betrieb von Kanal 4 mit 5 V ist eine Brücke von Klemme 1 auf Klemme 8 erforderlich.

**Hinweis zu den Relaiskontakten:** 

Die Relaiskontakte sind intern als Schließer (NO – Normally Open) ausgeführt und über die Klemmen 4–6 sowie Klemme 9 nach außen geführt. Soll ein Relaiskontakt als Öffner (NC – Normally Closed) verwendet werden, ist eine interne Umverdrahtung erforderlich.

**Hinweis zu DC-Buchse (6 V–36 V für Klemmen 3 & 8):** 

Die Eingangsspannung der DC-Hohlsteckerbuchse (6 V–36 V) kann intern abgegriffen und auf die Klemmen 3 und/oder 8 geführt werden, um Relaisausgänge mit höheren Spannungen zu schalten.

**Voraussetzungen:**
- Eingriffe dürfen ausschließlich durch elektrotechnische Fachkräfte erfolgen.
- Die vorhandenen 5-V-Brücken (Klemme 1 → 3 und/oder Klemme 1 → 8) müssen **zuvor** entfernt werden.
- Details sind dem E-Layout der ZapBox ZapOMat zu entnehmen.

---

## Weitere Anschlüsse

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
| Versorgungsspannung | 5V DC USB-C max. 3A |
| Versorgungsspannung | 6V-36V DC-Hohlbuchse max. 3A bei 24V |
| Spannungskonverter 6V-36V -> 5V | max. 1A Dauerlast, 2A kurzzeitig (2,5A in Spitze) |
| Schaltleistung Relais | max. 3A (kurzzeitig 10 A) |
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
