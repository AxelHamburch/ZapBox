# ZapBox Headless Simple – Bedienungsanleitung

**Sprache:** Deutsch | **Version:** oi967253
---
## Inhaltsverzeichnis

1. [Übersicht](#übersicht)
2. [Ansichten](#ansichten)
3. [Anschlüsse](#anschlüsse)
4. [Bedienelemente](#bedienelemente)
5. [Einrichtung und Inbetriebnahme](#einrichtung-und-inbetriebnahme)
6. [Technische Daten](#technische-daten)
7. [Sicherheitshinweise](#sicherheitshinweise)
8. [Weiterführende Links](#weiterführende-links)

---

## Übersicht

Die **ZapBox Headless Simple** ist ein elektronischer Schalter für Bitcoin-Lightning-Zahlungen ohne Display. Mit einer Zahlung über das Lightning-Netzwerk lässt sich ein Ausgang schalten – ideal für eingebettete Anwendungen, verdeckte Installationen, Maschinenbau und überall dort, wo kein Display benötigt wird.

Der Betriebszustand wird ausschließlich über eine **Status-LED** angezeigt. Eine zweite **Action-LED** zeigt die Schaltfunktion an.

### Grundausstattung

| Komponente | Beschreibung |
|---|---|
| Mikrocontroller | ESP32 Dev Module (kein Display) |
| Eingang | Dual USB-A und USB-C |
| Ausgang | Dual USB-A und USB-C |
| Statusanzeige | Status-LED mit Blinkmustern und Action-LED als Rückmeldung |
| Bedienelement | Mikrotaster für BOOT (Config-Modus) und Reset |

## Ansichten

<img src="pics/pic-Headless-Simple/01.webp" alt="Dreiseitenansicht" width="67%">

*Bild 1: Dreiseitenansicht*

---

## Anschlüsse

### Eingang - Dual USB-A und USB-C Buchse zur Spannungsversorgung (5V)

Versorgen Sie das Gerät über den Anschluss **Power IN** mit einem USB-C-Kabel mit **5 V DC (max. 5 A)**.

> **Hinweis:** Der USB-Power-Anschluss unterstützt keine automatische USB-C-Leistungsanforderung (kein USB-C Power Delivery). Einige USB-C-Ladegeräte oder Powermodule erkennen die ZapBox daher nicht als Verbraucher und liefern keinen Strom. Verwenden Sie in diesem Fall einen **USB-A-Ausgang** der Spannungsversorgung oder eine alternative 5-V-Stromquelle. Die maximale Stromstärke darf 3 A nicht überschreiten.


### Eingang - Micro-USB Buchse am Mikrocontroller (Datenzugang, seitlich)

Um Daten vom Gerät zu lesen oder zu übertragen, verbinden Sie die ZapBox mit einem Computer oder Laptop:

1. An der **rechten Seite**, neben den USB-Anschlüssen, befindet sich ein kleines Panel. Öffnen Sie das Panel mit einem **schmalen Schraubendreher**, indem Sie es vorsichtig herausheben.
2. Schließen Sie ein Micro-USB-Kabel an den Mikrocontroller an.

<img src="pics/pic-Headless-Simple/02.webp" alt="Micro USB Port" width="67%">

*Bild 2: Micro USB Port*

> **Wichtiger Hinweis:** Der USB-Anschluss direkt am Mikrocontroller ist ausschließlich zum Flashen der Firmware und zur Übertragung von Konfigurationsparametern vorgesehen. Während des Flashvorgangs darf keine Last am Ausgang angeschlossen oder geschaltet werden, da dies zu Fehlfunktionen oder zur **Beschädigung des Mikrocontrollers** führen kann.
>
> Es wird daher empfohlen:
> - während der USB-Verbindung keine Last am Ausgang anzuschließen oder
> - den regulären **Power-IN-Eingang** zusätzlich an dieselbe Spannungsversorgung anzuschließen. Dadurch wird sichergestellt, dass der Strom für das Leistungsrelais nicht über den Mikrocontroller fließt und diesen überlastet.

### Ausgang - Dual USB-A und USB-C Buchse (geschaltete 5V Spannung)

Die USB-Buchse wird über einen Relais-Schaltkontakt geschaltet. Die **Gesamtbelastung** der Buchsen sollte **3 A nicht überschreiten**.

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

---

## Einrichtung und Inbetriebnahme

Die ZapBox wird nach der Fertigung getestet und mit der aktuellen Firmware ausgeliefert - sie ist aber nicht parametriert. Die Software wird aktiv weiterentwickelt, daher ist es empfehlenswert, die ZapBox gleich zu Beginn einmal mit der neuesten Firmware zu bespielen und dann eine Parametrierung durchzuführen. Dafür gibt es einen komfortablen [**Web-Installer Headless**](https://installer.zapbox.space/headless/).

### Schritt 1: Firmware-Update
1. Öffnet auf der rechten Seite das Panel, wie oben unter "Eingang - Micro-USB Buchse am Mikrocontroller" beschrieben.
2. Schließt die ZapBox an dem USB-C-Port mit einem Kabel an und verbindet es mit einem Computer.
3. Öffnet einen Chromium Browser, zum Beispiel Google Chrome, Microsoft Edge, Brave, Vivaldi, Opera oder [Helium](https://helium.computer/).

### Schritt 2: Parametrierung
1. Navigiert im Browser zur Web-Installer-Seite.
2. Folgt den Anweisungen auf der Seite, um die gewünschten Parameter wie `WiFi SSID`, `WiFi Passwort` und `Device Settings String` einzugeben.
3. Speichert die Einstellungen und startet die ZapBox neu.

> **Hinweis:** Während der Einrichtung sollte keine Last an den Ausgängen angeschlossen sein, um Fehlfunktionen oder Schäden am Mikrocontroller zu vermeiden.

Die ZapBox wird nach der Initialisierung den QR-Code des Produkts anzeigen und ist bereit für die erste Zahlung und anschließende Schaltaktion.

---

## Technische Daten

| Eigenschaft | Wert |
|---|---|
| Versorgungsspannung | 5 V DC über USB-C |
| Maximaler Eingangsstrom | 5,0 A |
| Ausgangsleistung | max. 3,0 A (empfohlen) |
| Mikrocontroller | ESP32 Dev Module (WROOM-32) |
| Display | keines (Headless) |
| Statusanzeige | Status-LED / Action-LED |
| Temperaturbereich | 0–40 °C |
| Kommunikation | Wi-Fi (ESP32) |
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
| GitHub-Repository (Software, E-Layouts, 3D-Druckdateien, Bedienungsanleitungen, etc.) | https://github.com/AxelHamburch/ZapBox |
| ZapBox Extension | https://github.com/AxelHamburch/zapbox_extension |
| LNbits | https://lnbits.com/ |

---

*Änderungen und Irrtümer vorbehalten. Stand: 2026*
