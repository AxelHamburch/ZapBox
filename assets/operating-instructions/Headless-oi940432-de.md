# ZapBox Headless – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** oi940432

---

## Inhaltsverzeichnis

1. [Übersicht](#übersicht)
2. [Ansichten](#ansichten)
3. [Anschlüsse](#anschlüsse)
4. [Bedienelemente](#bedienelemente)
5. [Einrichtung und Inbetriebnahme](#einrichtung-und-inbetriebnahme)
6. [Option: NFC-Modul](#option-nfc-modul)
7. [Technische Daten](#technische-daten)
8. [Sicherheitshinweise](#sicherheitshinweise)
9. [Weiterführende Links](#weiterführende-links)

---

## Übersicht

Die **ZapBox Headless** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen ohne Display. Mit einer Zahlung über das Lightning-Netzwerk lässt sich ein Ausgang schalten – ideal für eingebettete Anwendungen, verdeckte Installationen, Maschinenbau und überall dort, wo kein Display benötigt wird.

Der Betriebszustand wird ausschließlich über eine **Status-LED** angezeigt. Eine zweite **Action-LED** zeigt die Schaltfunktion an.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | ESP32 Dev Module (kein Display) |
| Eingang | USB-C (Power IN) |
| Ausgang | USB-C (Power OUT) |
| Statusanzeige | Status-LED mit Blinkmustern und Action-LED als Rückmeldung |
| Bedienelement | Mikrotaster für BOOT (Config-Modus) und Reset |
| Schalter | 3-poliger Schiebeschalter (AUTO / OFF / ON) |
| Option NFC-Modul | Für Bolt Cards (NTAG424 DNA) und Standard NTAG213/215/216 (mit LNURL-withdraw) |

---

## Ansichten

<img src="pic-Headless/Headless-oi-01.webp" alt="Frontansicht-Draufsicht" width="67%">

*Bild 1: Frontansicht / Draufsicht*

---

## Anschlüsse

### Eingang - USB-C Buchse zur Spannungsversorgung (5V)

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 5 A)**.

> **Hinweis:** Der Power-IN-Anschluss ist nicht „intelligent". Einige Ladegeräte oder Powermodule mit USB-C-Ausgang erkennen die ZapBox nicht und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine andere Stromquelle.

---

### Eingang - Micro-USB Buchse am Mikrocontroller (Datenzugang, vorne)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **Vorderseite links** befindet sich eine kleines Panel links neben den USB-Anschlüssen. Öffnen Sie das Panel, indem Sie von unten mit einem **schmalen Schraubendreher** das Panel nach rechts wegschieben.
2. Schließen Sie ein Micro-USB-Kabel an den Mikrocontroller an.

<img src="pic-Headless/Headless-oi-02.webp" alt="Panel öffnen für Datenverbindung" width="67%">

*Bild 2: Panel öffnen für Datenverbindung*

<img src="pic-Headless/Headless-oi-03.webp" alt="Micro USB Port" width="67%">

*Bild 3: Micro USB Port*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller ist ausschließlich zum Flashen der Firmware und zur Übertragung von Konfigurationsparametern vorgesehen. Während des Flashvorgangs darf keine Last am Ausgang angeschlossen oder geschaltet werden, da dies zu Fehlfunktionen oder zur **Beschädigung des Mikrocontrollers** führen kann.
>
> Es wird daher empfohlen:
> - während der USB-Verbindung keine Last am Ausgang anzuschließen oder
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen. Dadurch wird sichergestellt, dass der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

---

### Ausgang - USB-C-Buchse (geschaltete 5V Spannung)

Die USB-Buchse wird über einen Relais-Schaltkontakt geschaltet. Die **Gesamtbelastung** der Buchsen sollte **3 A nicht überschreiten**.

---

## Bedienelemente

Die ZapBox Headless hat **kein Display**. Der Betriebszustand wird ausschließlich über die **Status-LED** und **Action-LED** signalisiert.

### Status-LED – Blinkmuster

| Muster | Bedeutung |
|---|---|
| 3× kurzes Blinken beim Start | Boot abgeschlossen |
| Schnelles Blinken | Verbindungsaufbau / Initialisierung |
| Langsames Blinken (1 Hz) | Config-Modus aktiv |
| Dauerlicht | Betriebsbereit, wartet auf Zahlung |
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

### Dreifach-Schiebeschalter (AUTO / OFF / ON)

| Stellung | Funktion |
|---|---|
| **A** (AUTO) | Automatikbetrieb – Normalbetrieb |
| **0** (OFF) | Spannungsversorgung unterbrochen – Ausgang AUS |
| **1** (ON) | Ausgang (USB-C) dauerhaft EIN |

---

## Einrichtung und Inbetriebnahme

Die ZapBox wird nach der Fertigung getestet und mit der aktuellen Firmware ausgeliefert - sie ist aber nicht parametriert. Die Software wird aktiv weiterentwickelt, daher ist es empfehlenswert, die ZapBox gleich zu Beginn einmal mit der neuesten Firmware zu bespielen und dann eine Parametrierung durchzuführen. Dafür gibt es einen komfortablen **Web-Installer** (Headless-Version).

Hier eine Schritt-für-Schritt-Anleitung für die Einrichtung:

1. Schließt die ZapBox am Micro-USB-Port des Mikrocontrollers mit einem Kabel an und verbindet es mit einem Computer.
2. Öffnet einen Chromium Browser, zum Beispiel Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera oder [Helium](https://helium.computer/).
3. Ruft die Web-Installer Seite **https://installer.zapbox.space/headless/** auf.
4. Flasht die aktuellste "Latest" Version (Headless), wie unter Punkt 1 des Web-Installer beschrieben.
5. Nach dem Flashvorgang schließt das kleine Fenster und geht zu Punkt 3 - Load config values. Dort wählt ihr den Button `🔌 Connect`.
6. Jetzt solltet ihr in dem grünen Feld `✅ Connected` und `✅ Config mode` sehen. Der Config-Modus ist aktiv, sobald die **Status-LED langsam blinkt** (ca. 1 Hz). Falls nicht, prüft einmal den Punkt 2 - Prepare connection.
7. Drei Parameter benötigt die ZapBox: `WiFi SSID` / `WiFi password` / `Device settings string`. Den Device-Settings-String bekommt ihr von eurer LNbits Wallet. Fügt dazu die Erweiterung **Bitcoin Switch** oder **ZapBox** hinzu. Die ZapBox Erweiterung unterstützt auch das NFC-Modul, ansonsten sind sie identisch.
8. Nachdem alle drei Parameter hinterlegt wurden, muss diese mit dem Button `🔥 Write Config` einmal gespeichert werden und die ZapBox mit dem Button `🔁 Restart` neu gestartet werden.

Nach der Initialisierung leuchtet die Status-LED dauerhaft – die ZapBox ist betriebsbereit und wartet auf die erste Zahlung.

Bei Fehler oder Störungen bitte auf der Web Installer Seite weiter unten die Kapitel "Error Detection & Report" und "Troubleshoot" beachten. 

---

## Option: NFC-Modul

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
| Ausgangsleistung | max. 3,0 A (empfohlen) |
| Mikrocontroller | ESP32 Dev Module (WROOM-32) |
| Flash-Speicher | 4 MB |
| SRAM | 512 KB |
| Display | keines (Headless) |
| Statusanzeige | Status-LED / Action-LED |
| Kommunikation | Wi-Fi (ESP32) |
| Relaiskanäle | 1 - Erweiterbar bis zu 12 (CH01–CH12) |
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
| Web-Installer, Kurzübersicht & Fehlerbehebung | https://installer.zapbox.space/headless/ |
| Detaillierte Dokumentation (Parameter & Funktionen) | https://ereignishorizont.xyz/zapbox/ |
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien, Bedienungsanleitungen) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
