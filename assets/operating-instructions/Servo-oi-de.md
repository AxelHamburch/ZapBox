# ZapBox Servo – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** oi947394

---

## Inhaltsverzeichnis

1. [Einleitung](#einleitung)
2. [Grundausstattung](#grundausstattung)
3. [Spannungsversorgung](#spannungsversorgung)
4. [16-poliger Steckverbinder](#16-poliger-steckverbinder)
5. [Schaltausgänge Kanal 1 bis 4](#schaltausgänge-kanal-1-bis-4)
6. [Schnittstelle Mikrocontroller (ESP32)](#schnittstelle-mikrocontroller-esp32)
7. [Bedienelemente](#bedienelemente)
8. [Montagehalterung mit Schnapp-Arretierung](#montagehalterung-mit-schnapp-arretierung)
9. [Einrichtung und Inbetriebnahme](#einrichtung-und-inbetriebnahme)
10. [NFC-Modul (Option)](#nfc-modul-option)
11. [Technische Daten](#technische-daten)
12. [Sicherheitshinweise](#sicherheitshinweise)
13. [Weiterführende Links](#weiterführende-links)

---

## Einleitung

Die **ZapBox Servo** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen. Mit einer Zahlung über das Lightning-Netzwerk lassen sich Ausgänge schalten – ideal für Automaten, Präsentationen, Eventsteuerung und viele weitere Anwendungen. 

Für umfängliche Automatisierungsaufgaben sind die Kontakte über einen 16-poligen abnehmbaren Steckverbinder nach außen geführt. Sie verfügt über einen zusätzlichen Sensoreingang, der individuell parametriert werden kann. Optional kann das NFC-Modul über die Steckerleiste extern geführt werden. 

Für einen vielseitigen Einsatz und einfachen Anschluss kann die Servo alternativ zum 5-V-USB-C-Anschluss auch über ein DC-Steckernetzteil betrieben werden. Sie verfügt über einen Weitbereichseingang (DC 6–36 V) mit einer Standard-DC-Buchse (5,5 × 2,1 mm).
Die benötigte 5-V-Spannung wird intern über einen selbstregelnden Spannungswandler erzeugt. Dadurch erweitern sich die Anwendungsmöglichkeiten erheblich, und die Spannungsversorgung der Ausgänge kann unabhängig von der 5-V-Versorgung intern bereitgestellt werden.

<img src="pics/pic-Servo/Servo-oi-10.webp" alt="Frontansicht" width="75%">

*Bild 1: Frontansicht und Rückansicht*

<img src="pics/pic-Servo/Servo-oi-11.webp" alt="Rückansicht" width="75%">

*Bild 1: Rückansicht*

## Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | T-Display-S3 mit 1,9" LCD-Display |
| Frontpanel | Wahlweise mit 35° oder 90° Display Front (90° auch Einbau erhältlich) |
| Spannungsversorgung | USB-C (5V) und Weitbereichseingang DC 6V-36V (Hohlbuchse 5,5*2,1 mm) |
| Eingänge / Ausgänge | 16-polige Steckverbinder (15EDGWC 3,81mm) |
| Ausgänge | 2 Relaisausgänge CH1/CH4 - Schaltkontakte (NO/COM/NC) |
| Ausgänge | 2 Steuerausgänge CH2/CH3 - GPIO-Ausgänge zur Servosteuerung |
| Eingang | 1 Sensoreingang |
| Interface | Anschluss für externes NFC-Modul |
| Bedienelement | LED-Button (volle ZapBox-Funktionalität) |
| Option BTC-Ticker | Aktivierbar über den Web Installer |
| Option Boardtaster | Zwei On-board-Mikrotaster (optional verdeckt)|
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |
| Option Schalter | Schiebeschalter (ON/OFF) |

---

## Spannungsversorgung

Die ZapBox Servo verfügt über einen USB-C-Anschluss sowie über eine DC-Hohlsteckerbuchse (5,5 × 2,1 mm) für gängige DC-Steckernetzteile.
Die Spannungsversorgung der Ausgänge kann individuell angepasst werden: entweder extern über die Klemmen 3 und 8 oder intern durch Abgriff der Speisespannung des Weitbereichseingangs.

### Spannungseingang USB-C Buchse (5V) 

Die ZapBox kann über den USB-C-Anschluss **Power** mit **5 V DC (max. 3 A)** versorgt werden. Die 5-V-Spannung wird über Klemme 1 des Steckverbinders nach außen geführt und kann dort zur Versorgung der Schaltausgänge auf die Klemmen 3 und 8 gebrückt werden.

> **Hinweis:** Der USB-Power-Anschluss unterstützt keine automatische USB-C-Leistungsanforderung (kein USB-C Power Delivery). Einige USB-C-Ladegeräte oder Powermodule erkennen die ZapBox daher nicht als Verbraucher und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine alternative 5-V-Stromquelle. Die maximale Stromstärke darf 3 A nicht überschreiten.

### Spannungseingang DC-Hohlsteckerbuchse (6 V–36 V)

Die DC-Buchse ist als Weitbereichseingang für Spannungen von 6 V bis 36 V ausgelegt und eignet sich für gängige Netzteile mit z. B. 12 V oder 24 V Ausgangsspannung.
Die Eingangsspannung wird intern über einen Step-Down-Converter auf 5 V geregelt. Der Spannungswandler kann im Dauerbetrieb bis zu 1 A liefern. **Die Wärmeentwicklung ist dabei zu beachten ⚠️.**
Kurzzeitig sind auch Ströme bis 2 A möglich, in sehr kurzen Lastspitzen bis zu 2,5 A.

Intern besteht die Möglichkeit, die Eingangsspannung abzugreifen und auf die Steckerleiste zu verdrahten, um die Ausgangskanäle direkt mit der Eingangsspannung zu versorgen. Dies erfordert einen Eingriff in das Gerät und darf ausschließlich von elektrotechnischen Fachkräften durchgeführt werden. Details hierzu sind im [E-Layout](https://github.com/AxelHamburch/ZapBox#electrical-layout) der ZapBox Servo beschrieben.

> **Hinweis:** Der interne Spannungswandler besitzt eine begrenzte Leistungsfähigkeit. Bei hohen Lastströmen entsteht erhöhte Wärmeentwicklung, die zu einer Erwärmung des Geräts führen kann. Sorgen Sie für ausreichende Kühlung und überschreiten Sie die angegebenen Stromgrenzen nicht.

### Hinweise zur Stromversorgung
- Wird dauerhaft eine Leistungsabgabe von mehr als 1 A benötigt, wird empfohlen, den USB-C-Power-Anschluss mit einem separaten 5-V-Netzteil zu verwenden.
- In diesem Fall sind für die Summe aller Verbraucher bis zu 3 A möglich.
- Über die Klemme 1 und 2 des Steckverbinder kann die 5 V Spannung für externe Verbraucher abgenommen werden. Über die Klemme 3 wird der Schaltausgang des Kanal 1 (CH1) versorgt und über die Klemme 8 der Schaltausgang des Kanal 4 (CH4). Die Klemmen können auch für den Spannungsabgriff für die Servomotoren verwendet werden.

#### Anschlussbeispiele

Da einzelne Verbraucher, wie z. B. LED-Streifen, hohe Ströme (> 1 A) aufnehmen können, ist auch eine kombinierte Einspeisung möglich.
Beispielsweise kann die ZapBox über die DC-Buchse mit 12 V versorgt werden und gleichzeitig den Relaiskontakt am Ausgang 4 (CH4) mit dieser Spannung speisen.
Der interne Spannungswandler versorgt dabei das System (ESP32) sowie die Ausgänge 1–3 (CH1–CH3) mit 5 V, während eine 12-V-LED-Leiste am Ausgang 4 direkt mit 12 V betrieben wird. Dadurch wird die Belastung des internen Spannungswandlers reduziert.

Eine weitere häufige Anwendung ist der Betrieb von Servomotoren, die eine höhere Versorgungsspannung benötigen, z. B. HV-Servos (High Voltage) mit Spannungen zwischen 7,4 V und 8,4 V.
Hierzu wird die ZapBox über den DC-Buchseneingang beispielsweise mit 8 V versorgt. Intern wird die Spannung vor dem Spannungsregler abgegriffen und auf die dafür vorgesehene Klemme 3 sowie GND geführt. Die externe Brücke zwischen Klemme 1 und Klemme 3 muss hierfür entfernt werden.

Dadurch werden die Schaltausgänge 1 bis 3 mit 8 V versorgt und können die Servomotoren direkt antreiben. Die Strombelastung sollte dabei 3 A nicht überschreiten, da dies aktuell die Grenze der internen Verdrahtung darstellt.

Der Schaltausgang 4 kann über eine Brücke zwischen Klemme 1 und Klemme 8 mit 5 V gespeist werden. Optinal kann an Klemme 8 auch eine andere Spannung, z. B. 12 V, angeschlossen werden. Kanal 4 kann anschließend beispielsweise zum Schalten einer LED-Beleuchtung als Ambientlicht verwendet werden.

#### Zusatzinformation

Zusätzlich kann neben der DC-Versorgung (6 V–36 V) ein 5-V-Netzteil mit maximal 3 A am USB-C-Power-Anschluss angeschlossen werden.
Der gleichzeitige Betrieb beider Versorgungen ist möglich, jedoch sind Unterschiede der 5-V-Spannungen zu beachten, um gegenseitige Beeinflussung oder Rückspeisung der Netzteile zu vermeiden.

## 16-poliger Steckverbinder

Die ZapBox verfügt über einen abnehmbaren 16-poligen Steckverbinder (15EDGWC 3,81 mm).

**Klemmenbelegung in der Übersicht:**
| Klemme | Funktion | Verwendung | Hinweis |
|------|----------|-----------|---------------|
| 1 | 5 V | Ausgang | Spannungsversorgung 5 V |
| 2 | GND | Ausgang | Masse |
| 3 | Versorgung Kanal 1-3 | Eingang / Ausgang | 5 V oder 6 V–36 V |
| 4 | CH1 Schaltausgang Kanal 1 (GPIO Pin 12) | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 5 | CH2 Servo Steuersignal 1 (GPIO Pin 13) | Ausgang | Ansteuerung Servomotor (pulsierend, 3,3 V) |
| 6 | CH3 Servo Steuersignal 2 (GPIO Pin 11) | Ausgang | Ansteuerung Servomotor (pulsierend, 3,3 V) |
| 7 | GND | Ausgang | Masse |
| 8 | Versorgung Kanal 4 | Eingang / Ausgang | 5 V oder 6 V–36 V |
| 9 | CH4 Schaltausgang Relais Kanal 4 (GPIO Pin 11) | Ausgang | Ansteuerung Verbraucher (NO – Normally Open) |
| 10 | GND| Ausgang | Masse |
| 11 | Sensor (GPIO Pin 2) | Eingang | Für z.B. eine Lichtschranke (NPN) |
| 12 | 5 V | Ausgang | 5 V für Sensor und NFC-Modul |
| 13 | GND | Ausgang | Masse für Sensor und NFC-Modul |
| 14 | NFC IRQ (GPIO Pin 1) | Schnittstelle | Externes NFC-Modul |
| 15 | I2C SCL (GPIO Pin 17) | Schnittstelle | Externes NFC-Modul |
| 16 | I2C SDA (GPIO Pin 18) | Schnittstelle | Externes NFC-Modul |

**Hinweis zur Belegung:** 

Die Klemmenbelegung des Steckverbinders ist von rechts nach links durchnummeriert: Klemme 1 befindet sich ganz rechts, Klemme 16 ganz links. Siehe dazu Bild 2: Rückansicht.

Die ZapBox Servo verfügt über vier Ausgänge (Kanal 1 bis Kanal 4). Kanal 1 und Kanal 4 sind fest mit Relais verbunden. Kanal 2 und Kanal 3 sind direkt auf die Ausgangsklemmen geführt und hauptsächlich für die Servoansteuerung vorgesehen.
An den Ausgangsklemmen 5 und 6 liegen die Steuersignale der GPIO-Pins 13 und 10 an.

**Hinweis Spannungsverteilung mit Brücken:** 

Im Werkszustand sind die Klemmen 1 (allgemeine Versorgung) und 12 (Versorgung für Sensor und NFC-Modul) mit 5 V belegt. Die Masse (GND) ist auf den Klemmen 2, 7, 10 und 13 geführt.

Die Relaiskontakte besitzen keine interne Versorgungsspannung. Die gewünschte Schaltspannung muss daher extern aufgebrückt werden.
Um beispielsweise einen 5-V-Verbraucher zu schalten, ist für die Kanäle 1–3 eine externe Brücke auf dem 16-poligen Steckverbinder von Klemme 1 auf Klemme 3 erforderlich.
Für den Betrieb von Kanal 4 mit 5 V muss zusätzlich eine Brücke von Klemme 1 auf Klemme 8 gesetzt werden.

<img src="pics/pic-Servo/jumper.webp" alt="Jumper" width="35%">

*Bild 3: Auszug E-Layout - Klemmen die gebrückt sind*

**Hinweis zu DC-Buchse (6 V–36 V für Klemmen 3 & 8):** 

Die Eingangsspannung der DC-Hohlsteckerbuchse (6 V–36 V) kann intern abgegriffen und auf die Klemmen 3 und/oder 8 geführt werden, um Relaisausgänge mit höheren Spannungen zu betreiben.

**Voraussetzungen:**
- Eingriffe dürfen ausschließlich durch elektrotechnische Fachkräfte erfolgen.
- Die ggf. vorhandenen 5-V-Brücken (Klemme 1 → 3 und/oder Klemme 1 → 8) müssen **zuvor** entfernt werden.
- Details sind dem [E-Layout](https://github.com/AxelHamburch/ZapBox#electrical-layout) der ZapBox Servo zu entnehmen.

## Schaltausgänge Kanal 1 bis 4 

Die Servo verfügt über vier Relaisausgänge, die intern als Schließer (NO – Normally Open) ausgeführt und über die Klemmen 4, 5 und 6 sowie Klemme 9 des Steckverbinders nach außen geführt sind. Die Schließerkontakte sind den Kanälen 1 bis 3 jeweils den Klemmen 4, 5 und 6 zugeordnet; Kanal 4 ist auf Klemme 9 verdrahtet.

Soll ein Relaiskontakt als Öffner (NC – Normally Closed) verwendet werden, ist eine interne Umverdrahtung erforderlich.

Die Schaltausgänge sind für eine maximale Strombelastung von 3 A Dauerlast ausgelegt. Kurzzeitig sind Belastungen bis zu 5 A zulässig.

## Schnittstelle Mikrocontroller (ESP32)

### Seitenpanel öffnen
Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **rechten Seite der Frontblende** befindet sich eine kleine Abdeckung.
2. Öffnen Sie die Abdeckung, indem Sie von unten mit einem **schmalen Schraubendreher** die Abdeckung nach rechts wegschieben. Bei einigen Modellen ist auf der Abdeckung eine kleine Aussparung. Diese Aussparung kann genutzt werden, um die Abdeckung nach vorne zu kippen.

### USB-C-Kabel anschließen
3. Schließen Sie ein USB-C-Kabel am darunter liegenden Anschluss des Mikrocontrollers an.

<img src="pics/pic-Servo/Servo-oi-02.webp" alt="Panel öffnen und USB-C-Anschluss" width="75%">

*Bild 4: Panel öffnen und USB-C-Anschluss für Daten*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller ist ausschließlich zum Flashen der Firmware und zur Übertragung von Konfigurationsparametern vorgesehen. Während des Flashvorgangs darf keine Last am Ausgang geschaltet werden, da dies zu Fehlfunktionen oder zur **Beschädigung des Mikrocontrollers** führen kann.
>
> Es wird daher empfohlen:
> - während der USB-Verbindung keine Last am Ausgang anzuschließen oder
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen. Dadurch wird sichergestellt, dass der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

## Bedienelemente

Je nach Version verfügt die ZapBox neben dem **LED-Button** über zwei **On-board-Mikrotaster**, die direkt mit dem Mikrocontroller verbunden sind. Alle Funktionen sind sowohl über den LED-Button als auch über die Mikrotaster erreichbar. Zusätzlich verfügt die ZapBox an der Unterseite des Frontpanels über einen Reset-Taster sowie optional an der Seite über einen Schiebeschalter für weitere Anwendungen.

### Funktionsübersicht - Mikrotaster / LED-Button

| Funktion | Mikrotaster | LED-Button |
|---|---|---|
| Hilfe-Seite anzeigen | 1× HELP drücken | LED-Button mind. 2 Sek. gedrückt halten |
| Nächste Seite / Produktwechsel | 1× NEXT drücken | 1× LED-Button kurz drücken |
| REPORT-Seite anzeigen | 2× HELP drücken | LED-Button 3× schnell hintereinander drücken |
| Config-Modus aufrufen | NEXT mind. 5 Sek. gedrückt halten | 1× kurz drücken, dann mind. 5 Sek. gedrückt halten |

---

## Montagehalterung mit Schnapp-Arretierung

Mit der optionalen Montagehalterung kann die ZapBox leicht montiert werden. 
Die Schnapp-Arretierung kann mit einem flachen Schraubendreher gelöst werden.

<img src="pics/pic-Servo/Servo-oi-04.webp" alt="Lösen der Montagehalter" width="100%">

*Bild 5: Lösen der Montagehalterung*

> **Hinweis:** Die ZapBox ist auf der Montagehalterung nur aufgeschoben und mit einem Schnappverschluss arretiert. Die Arretierung kann mit einem flachen Schraubendreher gelöst werden. Dazu den Schraubendreher vorsichtig in den Schlitz einschieben, leicht anheben und dabei den oberen Teil der ZapBox in Richtung des Schraubendrehers drücken. Die Verbindung sollte sich lösen und die ZapBox kann von der Montageplatte durch anheben getrennt werden.

---

## Einrichtung und Inbetriebnahme

Die ZapBox wird nach der Fertigung getestet und mit der aktuellen Firmware ausgeliefert - sie ist aber nicht parametriert. Die Software wird aktiv weiterentwickelt, daher ist es empfehlenswert, die ZapBox gleich zu Beginn einmal mit der neuesten Firmware zu bespielen und dann eine Parametrierung durchzuführen. Dafür gibt es einen komfortablen [**Web-Installer**](https://installer.zapbox.space/).

### Schritt 1: Firmware-Update
1. Öffnet das rechte Seitenpanel der Frontblende, wie oben unter "Seitenpanel öffnen" beschrieben.
2. Schließt die ZapBox an dem USB-C-Port mit einem Kabel an und verbindet es mit einem Computer.
3. Öffnet einen Chromium Browser, zum Beispiel Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera oder [Helium](https://helium.computer/).

### Schritt 2: Parametrierung
1. Navigiert im Browser zur Web-Installer-Seite.
2. Folgt den Anweisungen auf der Seite, um die gewünschten Parameter wie `WiFi SSID`, `WiFi Passwort` und `Device Settings String` einzugeben.
3. Speichert die Einstellungen und startet die ZapBox neu.

> **Hinweis:** Während der Einrichtung sollte keine Last an den Ausgängen angeschlossen sein, um Fehlfunktionen oder Schäden am Mikrocontroller zu vermeiden.

Die ZapBox wird nach der Initialisierung den QR-Code des Produkts anzeigen und ist bereit für die erste Zahlung und anschließende Schaltaktion.

### Fehlerdiagnose und -behebung 

Die ZapBox verfügt über eine komfortable Fehleranzeige über das Display. Es gibt vier grundlegende Fehler, die priorisiert sind:

| Prio. | Fehlerart | Abkürzung | Erkennungsmethode | Beschreibung |
|-----------|-----------|-----------|-------------------|--------------|
| 1 | **NO WIFI** | NW | WiFi-Verbindungsstatus | WiFi-Netzwerk nicht verbunden<br>-> Sind die WiFi-Daten korrekt?<br>-> Ist das WiFi-Signal zu schwach? |
| 2 | **NO INTERNET** | NI | HTTP-Check zu Google | Internetverbindung verloren<br>-> Ist das Internet erreichbar? |
| 3 | **NO SERVER** | NS | TCP-Port 443-Check | LNbits-Server nicht erreichbar<br>-> Ist die Server-Hardware ausgefallen?<br>-> Ist der Geräte-String korrekt? |
| 4 | **NO WEBSOCKET** | NWS | WebSocket-Verbindungsstatus | WebSocket-Protokoll-/Handshake-Fehler<br>-> Ist LNbits ausgefallen?<br>-> Ist der Geräte-String korrekt? |

Die Fehlermeldungen werden auch geloggt und können über den *Report Mode* abgerufen werden:

- Drücken Sie die HELP-Taste zweimal schnell hintereinander, um Fehlerzähler (0-99) für alle vier Fehlertypen mit ihren Auftretenshäufigkeiten anzuzeigen.
- Drücken Sie die LED-Taste dreimal schnell hintereinander (falls eine externe LED-Taste verfügbar ist).

Weitere aktuelle Informationen zu Fehlerbeschreibungen können auf der Web-Installer-Seite in den Kapiteln "Error Detection & Report" und "Troubleshoot" nachgelesen werden. 

---

## NFC-Modul (Option)

Je nach Ausstattung verfügt die ZapBox über ein NFC-Modul, das entweder auf der Oberseite montiert ist oder über den 16-poligen Steckverbinder extern als separates Modul verwendet werden kann.

Die ZapBox unterstützt aktuell folgende Kartentypen:

- **Boltcards** (NTAG424 DNA)
- **LNURL-Withdraw** von NTAG21x (213 / 215 / 216)

Voraussetzung für die Funktion ist die LNbits [ZapBox Extension](https://github.com/AxelHamburch/zapbox_extension). Diese muss vom LNbits-Server unterstützt werden. 

---

## Technische Daten

| Eigenschaft | Wert |
|---|---|
| Versorgungsspannung | 5V DC über USB-C, max. 3A |
| Versorgungsspannung | 6V–36V über DC-Hohlbuchse, max. 3A bei 24V |
| DC-DC Spannungswandler | 6V–36V → 5V, max. 1A Dauerlast, 2A kurzzeitig (2,5A Spitze) |
| Schaltausgänge Relais | Max. 3A Dauerlast (kurzzeitig 5A) |
| Display | 1,9" LCD (T-Display-S3) |
| NFC-Modul | PN532 |
| Temperaturbereich | 0–40 °C |
| Kommunikation | Wi-Fi (ESP32-S3) |
| Zahlungsprotokoll | Bitcoin Lightning Network |

---

## Sicherheitshinweise

- Betreiben Sie das Gerät ausschließlich mit der angegebenen Versorgungsspannung.
- Überschreiten Sie nicht die maximalen Strombelastungen der Ausgänge.
- Führen Sie keine Arbeiten an den Relaiskontakten unter Last durch.
- Das Gerät ist nicht für den Einsatz in feuchten oder nassen Umgebungen geeignet.
- Sorgen Sie für ausreichende Belüftung um das Gerät. 
- Außerhalb der Reichweite von Kindern aufbewahren.

---

## Weiterführende Links

| Ressource | Link |
|---|---|
| Übersicht aller ZapBox-Modelle | https://zapbox.space/ |
| Web-Installer, Kurzübersicht & Fehlerbehebung | https://installer.zapbox.space/ |
| Detaillierte Dokumentation (Parameter & Funktionen) | https://ereignishorizont.xyz/zapbox/ |
| GitHub-Repository (Software, 3D-Druckdateien, Anleitungen, etc.) | https://github.com/AxelHamburch/ZapBox |
| GitHub-Repository (E-Layouts) | https://github.com/AxelHamburch/ZapBox#electrical-layout |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
