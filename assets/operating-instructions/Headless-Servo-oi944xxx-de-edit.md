# ZapBox Headless Servo – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** oi944935

---

## Inhaltsverzeichnis

1. [Übersicht](#übersicht)
2. [Ansichten](#ansichten)
3. [Anschlüsse](#anschlüsse)
4. [Bedienelemente](#bedienelemente)
5. [Einrichtung und Inbetriebnahme](#einrichtung-und-inbetriebnahme)
6. [Parametrierung](#parametrierung)
7. [NFC-Modul (Option)](#nfc-modul-option)
8. [Technische Daten](#technische-daten)
9. [Sicherheitshinweise](#sicherheitshinweise)
10. [Weiterführende Links](#weiterführende-links)

---

## Übersicht

Die **ZapBox Headless Servo** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen ohne Display. Mit einer Zahlung über das Lightning-Netzwerk lässt sich ein Ausgang schalten – ideal für eingebettete Anwendungen, verdeckte Installationen, Maschinenbau und überall dort, wo kein Display benötigt wird.

Die ZapBox Headless Servo verfügt über einen Steckverbinder zum Anschluss externer Relais mit 5-V-Ansteuerung sowie digitaler Servomotoren. Unterstützt werden sowohl 180°-Servomotoren für Positionierungsaufgaben als auch 360°-Servomotoren für kontinuierliche Rotationsanwendungen.
Zusätzlich stehen auf der Steckverbindung zwei universelle Anschlüsse für Aktoren und Sensoren zur Verfügung. Die Konfiguration und Parametrierung erfolgt über den Web Installer.

Der Betriebszustand wird ausschließlich über eine **Status-LED** angezeigt. Eine zweite **Action-LED** zeigt die Schaltfunktion an.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | ESP32 Dev Module (kein Display) |
| Eingang | USB-C (Power IN) |
| Ausgang | 5-polige Steckverbinder (15EDG 3,81 mm) |
| Statusanzeige | Status-LED mit Blinkmustern und Action-LED als Rückmeldung |
| Bedienelement | Mikrotaster für BOOT (Config-Modus) und Reset |
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |

---

## Ansichten

<img src="pics/pic-Headless-Servo/Headless-Servo-oi-01.webp" alt="Frontansicht-Draufsicht" width="67%">

*Bild 1: Frontansicht / Draufsicht*

---

## Anschlüsse

### Eingang - USB-C Buchse zur Spannungsversorgung (5V)

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 5 A)**.

> **Hinweis:** Der USB-Power-Anschluss unterstützt keine automatische USB-C-Leistungsanforderung (kein USB-C Power Delivery). Einige USB-C-Ladegeräte oder Powermodule erkennen die ZapBox daher nicht als Verbraucher und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine alternative 5-V-Stromquelle. Die maximale Stromstärke darf 3 A nicht überschreiten.

---

### Eingang - Micro-USB Buchse am Mikrocontroller (Datenzugang, vorne)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **Vorderseite links** befindet sich eine kleines Panel links neben den USB-Anschlüssen. Öffnen Sie das Panel, indem Sie von unten mit einem **schmalen Schraubendreher** das Panel nach rechts wegschieben.
2. Schließen Sie ein Micro-USB-Kabel an den Mikrocontroller an.

<img src="pics/pic-Headless-Servo/Headless-Servo-oi-02.webp" alt="Panel öffnen für Datenverbindung mit Micro USB-Port" width="100%">

*Bild 2: Panel öffnen für Datenverbindung mit Micro USB-Port*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller ist ausschließlich zum Flashen der Firmware und zur Übertragung von Konfigurationsparametern vorgesehen. Während des Flashvorgangs darf keine Last am Ausgang angeschlossen oder geschaltet werden, da dies zu Fehlfunktionen oder zur **Beschädigung des Mikrocontrollers** führen kann.
>
> Es wird daher empfohlen:
> - während der USB-Verbindung keine Last am Ausgang anzuschließen oder
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen. Dadurch wird sichergestellt, dass der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

### Ausgang - 5-poliger Steckverbinder

**Klemmenbelegung:**
| Klemme | Funktion | Anschlussoption | Verwendung |
|------|----------|-----------|---------------|
| 1 | 5V | Ausgang (Eingang möglich) | Spannungsversorgung für den Servo, etc. |
| 2 | GND | Ausgang (Eingang möglich) | Masseanschluss für den Servo, etc. |
| 3 | Relais oder Servomotor | Output | 3,3V Servo- bzw. Relaissteuersignal |
| 4 | Sensor / Aktor 1 | Eingang oder Ausgang | Sensor oder Aktor 1 |
| 5 | Sensor / Aktor 2 | Eingang oder Ausgang | Sensor oder Aktor 2 |

Die ZapBox kann entweder über den USB-C-Anschluss mit einer 5-V-Versorgungsspannung oder über die Klemmen 1 und 2 betrieben werden.
Für die Klemmen 3 bis 5 stehen optionale Funktionen zur Verfügung, die über den Web Installer eingestellt und parametriert werden müssen. Weitere Informationen hierzu finden sich unter „Optional Settings and Functions – ZapBox Mode“ bzw. „Special Features for Vending Machines“ im Web Installer.
Standardmäßig ist der Ausgang für Klemme 3 auf „Relay“ eingestellt, während die Klemmen 4 und 5 auf „No function“ gesetzt sind.

---

## Bedienelemente

Die ZapBox Headless hat **kein Display**. Der Betriebszustand wird ausschließlich über die **Status-LED** (GPIO 21 (extern) / GPIO 2 (onboard)) und **Action-LED** (GPIO 13) signalisiert.

### Status-LED – Blinkmuster

| Muster | Bedeutung |
|---|---|
| 3× kurzes Blinken beim Start | Boot abgeschlossen |
| Schnelles Blinken | Verbindungsaufbau / Initialisierung |
| Langsames Blinken (1 Hz) | Config-Modus aktiv |
| Dauerlicht | Betriebsbereit, wartet auf Zahlung |
| Kurzes Ausschalten (300 ms) | Aktion gestartet – Relais/Servo ausgelöst |
| 200 ms an / 800 ms aus | NFC-Zahlung ausstehend (PENDING) |
| 2× kurzes Blinken | Zahlung erfolgreich |
| 3× kurzes Blinken | NFC-Timeout / Fehler |
| 1× Blinken (500 ms an/aus, 2 s Pause) | Fehlermuster 1: Kein WLAN |
| 2× Blinken (300 ms an/aus, 2 s Pause) | Fehlermuster 2: Kein Internet |
| 3× Blinken (250 ms an/aus, 2 s Pause) | Fehlermuster 3: Server nicht erreichbar |
| 4× Blinken (200 ms an/aus, 2 s Pause) | Fehlermuster 4: WebSocket-Verbindung fehlgeschlagen |

### Bedientaster

| Funktion | Taster |
|---|---|
| Config-Modus aufrufen | BOOT-Taster mind. 5 Sek. gedrückt halten |
| Neustart | Reset-Taster |

### Montagehalterung mit Schnapp-Arretierung

<img src="pics/pic-Headless-Servo/Headless-Servo-oi-mount.webp" alt="Lösen einer Montagehalter einer Headless mit NFC-Modul" width="100%">

*Bild 3: Lösen der Montagehalterung einer Headless mit NFC-Modul*

> **Hinweis:** Die ZapBox ist auf der Montagehalterung nur aufgeschoben und mit einem Schnappverschluss arretiert. Die Arretierung kann mit einem flachen Schraubendreher gelöst werden. Dazu den Schraubendreher vorsichtig in den Schlitz einschieben, leicht anheben und dabei den oberen Teil der ZapBox in Richtung des Schraubendrehers drücken. Die Verbindung sollte sich lösen und die ZapBox kann von der Montageplatte durch anheben getrennt werden.

---

## Einrichtung und Inbetriebnahme

Die ZapBox wird nach der Fertigung getestet und mit der aktuellen Firmware ausgeliefert - sie ist aber nicht parametriert. Die Software wird aktiv weiterentwickelt, daher ist es empfehlenswert, die ZapBox gleich zu Beginn einmal mit der neuesten Firmware zu bespielen und dann eine Parametrierung durchzuführen. Dafür gibt es einen komfortablen **Web-Installer** (Headless-Version).

Hier eine Schritt-für-Schritt-Anleitung für die Einrichtung:

1. Verbinden Sie die ZapBox über den Micro-USB-Port des Mikrocontrollers mit einem Kabel mit einem Computer.
2. Öffnen Sie einen Chromium-Browser, zum Beispiel Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera oder [Helium](https://helium.computer/).
3. Rufen Sie die Web-Installer Seite **https://installer.zapbox.space/headless/** auf.
4. Flashen Sie die aktuellste "Latest" Version (Headless), wie unter Punkt 1 des Web-Installer beschrieben.
5. Nach dem Flashvorgang schließen Sie das kleine Fenster und gehen zu Punkt 3 - Load config values. Dort klicken Sie auf den Button `🔌 Connect`.
6. Jetzt sollten Sie in dem grünen Feld `✅ Connected` und `✅ Config mode` sehen. Der Config-Modus ist aktiv, sobald die **Status-LED langsam blinkt** (ca. 1 Hz). Falls nicht, überprüfen Sie Punkt 2 - Prepare connection.
7. Drei Parameter benötigt die ZapBox: `WiFi SSID` / `WiFi password` / `Device settings string`. Den Device-Settings-String erhalten Sie von Ihrer LNbits Wallet. Fügen Sie dazu die Erweiterung **Bitcoin Switch** oder **ZapBox** hinzu. Die ZapBox Erweiterung unterstützt auch das NFC-Modul, ansonsten sind sie identisch.
8. Nachdem alle drei Parameter hinterlegt wurden, müssen diese mit dem Button `🔥 Write Config` einmal gespeichert werden und die ZapBox mit dem Button `🔁 Restart` neu gestartet werden.

Nach der Initialisierung leuchtet die Status-LED dauerhaft – die ZapBox ist betriebsbereit und wartet auf die erste Zahlung.

Bei Fehler oder Störungen bitte auf der Web Installer Seite weiter unten die Kapitel "Error Detection & Report" und "Troubleshoot" beachten. 

---

## Parametrierung

Die ZapBox Headless Servo übernimmt nach der ersten Installation den Standardwert **"Relais"** für Klemme 3. Da die ZapBox kein intern verbautes Relais besitzt, können Sie ein externes **5 V-Relais (High-Level-Trigger)** ansteuern.

### Konfigurationsschritte

Grundvoraussetzung für die Parametrierung ist, dass der Web Installer mit dem ESP32 `✅ Connected` ist und die ZapBox sich im `✅ Config mode` befindet (wie unter **Einrichtung und Inbetriebnahme**, Punkt 5 beschrieben). Gehen Sie dann folgendermaßen vor:

1. **Konfiguration auslesen:** Klicken Sie auf `📖 Read Config`
2. **Parameter anpassen:** Ändern Sie die gewünschten Werte (siehe nachfolgende Abschnitte)
3. **Speichern und Neustart:** Klicken Sie `🔥 Write Config` und starten Sie mit `🔁 Restart` neu

### Servoansteuerung (Klemme 3)

Für Servomotoren müssen Sie im Web Installer unter **Optional settings and functions - ZapBox Mode** einen Servotyp auswählen:

- **180° Servo**
- **360° Servo** (kontinuierlich)

Nach Auswahl eines Servotyps erscheinen zusätzliche Parameterfelder für die Konfiguration.

### Universelle Eingänge/Ausgänge (GPIO)

Die ZapBox verfügt über zwei universell konfigurierbare **GPIOs** (General Purpose Input/Output):

| Klemme | Funktion | Beschreibung |
|--------|----------|-------------|
| 4 | Sensor / Aktor 1 | Eingang oder Ausgang, frei konfigurierbar |
| 5 | Sensor / Aktor 2 | Eingang oder Ausgang, frei konfigurierbar |

Weitere Informationen zur Konfiguration finden sich im Absatz **Special features for vending machines**.

> **Wichtig:** Alle Servomotoren sowie Sensoren/Aktoren **müssen mit 5 V betrieben werden**. Die ZapBox stellt 5 V an Klemme 1 zur Verfügung. 

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
| Maximaler Eingangsstrom | 3,0 A |
| Ausgangsleistung | max. 3,0 A |
| Mikrocontroller | ESP32 Dev Module (WROOM-32) |
| Flash-Speicher | 4 MB |
| SRAM | 512 KB |
| Display | keines (Headless) |
| Statusanzeige | Status-LED / Action-LED |
| Kommunikation | Wi-Fi (ESP32) |
| Ein-/Ausgänge | max. 3 - 1 Servo (oder Relais), 2 Sensoren/Aktoren |
| Zahlungsprotokoll | Bitcoin Lightning Network |

---

## Sicherheitshinweise

- Betreiben Sie das Gerät ausschließlich mit der angegebenen Versorgungsspannung.
- Bitte beachten Sie, dass Servomotoren hohe Ströme aufnehmen und die Stromversorgung überlasten können.
- Achten Sie darauf, den Servo oder einen Aktuator nicht zu aktivieren, wenn der ESP32 nur über den Micro-USB-Anschluss angeschlossen ist.
- Das Gerät ist nicht für den Einsatz in feuchten oder nassen Umgebungen geeignet.
- Außerhalb der Reichweite von Kindern aufbewahren.

---

## Weiterführende Links

| Ressource | Link |
|---|---|
| Übersicht aller ZapBox-Modelle | https://zapbox.space/ |
| Web-Installer, Kurzübersicht & Fehlerbehebung | https://installer.zapbox.space/headless/ |
| Detaillierte Dokumentation (Parameter & Funktionen) | https://ereignishorizont.xyz/zapbox/ |
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien, Bedienungsanleitungen) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
