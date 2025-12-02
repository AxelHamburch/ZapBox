# Lightning ZapBox ⚡

Bitcoin Lightning-gesteuerter USB-Power-Switch für LilyGo T-Display-S3

## Was ist die ZapBox?

Die Lightning ZapBox ist ein kompaktes Gerät, das einen USB-Ausgang per Bitcoin Lightning-Zahlung steuert. An dem USB-Ausgang können verschiedene 5V-Geräte betrieben werden, wie LED-Lampen, Ventilatoren oder andere USB-betriebene Geräte.

## Funktionsweise

1. **QR-Code-Anzeige**: Das integrierte Display des T-Display-S3 zeigt einen QR-Code mit der LNURL zum Scannen an
2. **Lightning-Zahlung**: Nach dem Scannen und Bezahlen der Invoice wird die Zahlung an den LNbits-Server gesendet
3. **WebSocket-Trigger**: Der LNbits-Server sendet über WebSocket ein Signal an den ESP32-Mikrocontroller
4. **Relais-Schaltung**: Der ESP32 aktiviert das Relais, das den USB-Ausgang für einen festgelegten Zeitraum einschaltet
5. **Bestätigung**: Das Display zeigt an, dass die Zahlung eingegangen ist und das Relais geschaltet wurde

## Hardware

- **LilyGo T-Display-S3**: ESP32-S3 Mikrocontroller mit integriertem 170x320 LCD-Display
- **Relais-Modul**: Schaltet den USB-Ausgang
- **USB-Ausgangsbuchse**: Liefert 5V für angeschlossene Geräte
- **Zwei Taster**: Für Navigation und Zugriff auf die Hilfeseite
- **3-Stellungs-Schalter**:
  - **Position 0**: Alles ausgeschaltet
  - **Position 1**: Ausgang dauerhaft eingeschaltet (Bypass-Modus)
  - **Position A**: Automatik-Modus - ESP32 aktiv, wartet auf Lightning-Zahlung

## Bedienung

- **Linker Taster**: Kurz drücken = Test-Modus, 3 Sekunden halten = Konfigurations-Modus
- **Rechter Taster**: Hilfe-Seite anzeigen

## Konfiguration

Die Konfiguration erfolgt über den seriellen Port oder das Web-Interface im `installer` Ordner:

- WiFi SSID und Passwort
- LNbits Server WebSocket-URL
- LNURL für Zahlungen
- Display-Orientierung (horizontal/vertikal)

## PlatformIO-Projekt

Dieses Projekt ist für PlatformIO konfiguriert und basiert auf dem Arduino-Framework für ESP32-S3.

### Benötigte Bibliotheken

- ArduinoJson
- OneButton
- WebSockets
- TFT_eSPI
- QRCode

### Build und Upload

```bash
platformio run --target upload
```

---

**Lightning ZapBox** - Kompakt, einfach, Bitcoin-powered! ⚡
