# Bolt Cards – Das Bitcoin ⚡ Lightning Wallet im Scheckkarten-Format

Die **Bolt Card** ist ein moderner NFC-Chip im Kartenformat. Sie enthält die Zugangsdaten zu einem Bitcoin-Lightning-Wallet. Wenn du die Karte an ein Gerät oder ein Smartphone mit NFC-Funktion hältst, werden die Daten ausgelesen und Zahlungen autorisiert.

Es gibt leere Bolt Cards, die du selbst mit einem Lightning-Wallet verknüpfen musst. Und es gibt **vorinstallierte Bolt Cards** – für diese ist dieses Handout gedacht, um Nutzung, Einstellungen und Risiken zu verstehen.

---

## Deine vorinstallierte Bolt Card

Auf der Vorderseite deiner Bolt Card findest du:

- **ZAPBOX.SPACE** – der Link zu einer Webseite, auf der die Technologie vorgestellt wird, die Bolt Cards verarbeiten kann.
- **Batch- und Seriennummer** – direkt darunter aufgedruckt. Jede Bolt Card ist einzigartig und kann anhand dieser Nummer identifiziert werden.
- **QR-Code** – der direkte Link zu deinem persönlichen Lightning-Wallet.

### QR-Code scannen und Wallet öffnen

Scanne den QR-Code mit deinem Smartphone. Er führt direkt zu einem Lightning-Wallet auf einem LNbits-Server, der deine Zahlungen verarbeitet.

> ⚠️ **Zwei wichtige Hinweise:**
>
> 1. **Speichere dir den Link** – z. B. in einer Notiz auf deinem Smartphone oder PC. So hast du jederzeit Zugriff auf dein Wallet, auch ohne die Bolt Card. Am PC-Monitor lässt sich das Wallet besonders komfortabel bedienen.
>
> 2. **Schütze den QR-Code** – Lass niemanden die Karte in die Hand nehmen oder den QR-Code scannen. Der QR-Code ist ein **offener Wallet-Zugang**: Wer Zugriff bekommt, kann alle Guthaben entwenden.

---

## Das LNbits Wallet

Nachdem du den QR-Code gescannt und das Wallet geöffnet hast, wirst du feststellen, dass die mobile Ansicht etwas kompakt ist – die grundlegende Bedienung ist aber recht einfach.

<div style="display: flex; gap: 20px; align-items: flex-start;">
  <div style="flex: 0 0 33%;">
    <img src="lnbits-wallet.png" alt="LNbits Wallet Übersicht" style="width: 100%; max-width: 100%;" />
  </div>
  <div style="flex: 1;">

### Aufbau der Oberfläche

| Bereich | Funktion |
|---|---|
| **Oben – Betrag** | Zeigt das aktuelle Guthaben in Satoshis (sats). 100.000.000 sats = 1 Bitcoin. Ein leeres Wallet zeigt `0 sats`. |
| **Mitte – Transaktionen** | Grüner Pfeil = Eingang · Roter Pfeil = Ausgang (Zahlung) |
| **Unten – Aktionen** | `RECEIVE` zum Empfangen (Rechnung erstellen) · `SEND` zum Bezahlen · **QR-Code-Symbol** in der Mitte zum direkten Kamera-Scan |

### Navigation & Einstellungen

- **☰ Hamburger-Menü** (oben links, neben dem LNbits-Logo) – hier findest du Erweiterungen. Der Punkt **Bolt Cards** ist relevant, wenn du Einstellungen für die Karte vornehmen möchtest.
- **Oben rechts** – Sprache ändern und Account-Verwaltung. Wenn du mehrere Bolt Cards besitzt, musst du dich hier erst ausloggen, bevor du ein anderes Wallet öffnest. Im Browser kann immer nur ein Wallet gleichzeitig angemeldet sein – andernfalls erscheint ein **Fehler 404**. Lösung: aktuelles Wallet ausloggen, dann den neuen Link öffnen.
  </div>
</div>

---

## Wallet befüllen – der Lightning-ATM

Bevor du die Bolt Card nutzen kannst, muss das Wallet mit Satoshis befüllt werden. Eine Möglichkeit dazu sind **kleine Lightning-ATMs**, die einen praktischen Einstieg in die Lightning-Technologie bieten.

**Ablauf:**

1. Wirf z. B. 10 Cent in den ATM.
2. Nach Anzeige des Betrags den **LED-Taster** drücken, um die Abhebung zu bestätigen.
3. Öffne dein Bolt-Card-Wallet und tippe auf das **QR-Code-Symbol**, um die Kamera zu aktivieren.
4. Scanne den QR-Code des ATM und bestätige die Abhebung.
5. Kurz darauf erscheinen die Satoshis im Gegenwert der eingeworfenen Münzen in deinem Wallet.

> ℹ️ Der ATM arbeitet offline und bekommt die Abhebung nicht direkt mit. Da der LED-Taster danach blinkt, drücke ihn einmal, um den ATM für die nächste Person freizugeben.

---

## Zahlen mit der Bolt Card an ZapBox-Automaten

ZapBox-Automaten verfügen über ein **NFC-Modul**, erkennbar am **))** Symbol. Einige Automaten haben zusätzlich ein Display und zeigen einen QR-Code an.

### Zahlung per Bolt Card (NFC) – der einfachste Weg

1. Prüfe, ob das Gerät aktiv ist: Ein QR-Code wird angezeigt, oder eine LED leuchtet dauerhaft.
2. Halte die **Bolt Card** für mindestens zwei Sekunden an das NFC-Modul.
3. Das Display zeigt **PENDING NFC**, oder eine LED blinkt. Im Hintergrund wird die Zahlung vorbereitet.
4. Nach erfolgreicher Zahlung erscheint der **ACTION-Screen** bzw. die **ACT-LED** blinkt – das Produkt wird ausgegeben.

### Zahlung per QR-Code (alternativ)

Der klassische Weg, eine Lightning-Rechnung zu bezahlen, ist das **Scannen des QR-Codes** am Automaten:

1. Öffne dein Wallet und tippe auf das QR-Code-Symbol in der Mitte.
2. Erlaube den Kamera-Zugriff, wenn du dazu aufgefordert wirst.
3. Richte die Kamera auf den QR-Code des Automaten.
4. Bestätige die Zahlung – fertig.

---


*Weitere Informationen und aktuelle Dokumentation unter **[zapbox.space](https://zapbox.space)*** - **Version:** bch945441 | **Sprache:** Deutsch
