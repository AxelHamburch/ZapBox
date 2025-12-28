# ZapBox Refactoring Plan

## Analyse main.cpp (~3300 Zeilen)

### Aktuelle Struktur-Probleme

1. **Zu viele globale Variablen** (50+)
   - Netzwerk-State: WiFi, WebSocket, Server, Internet
   - Button/Touch-State: Touch, physical buttons, external button
   - Display-State: Orientierung, Theme, Screensaver
   - Config-Data: SSID, Password, LNURL, Schwellwerte
   - Feature-Flags: BTC-Ticker, Multi-Control, Special Mode
   - Error-State: 4 Error-Counter, Flags für verschiedene Modi

2. **Riesige loop() Funktion** (~1000 Zeilen)
   - Duplicate Logic für Touch/Button/Navigation
   - Verschachtelte Bedingungen für verschiedene Modi
   - Schwer zu debuggen und zu erweitern

3. **Config-Handling verstreut**
   - readFiles() in main.cpp
   - configMode() in main.cpp
   - SerialConfig in separater Datei
   - Keine zentrale Config-Verwaltung

4. **State-Management chaotisch**
   - 10+ Bool-Flags für Modi: inConfigMode, inHelpMode, inReportMode, onErrorScreen, screensaverActive, deepSleepActive, btcTickerActive, onProductSelectionScreen, setupComplete, initializationActive
   - Schwer nachzuvollziehen, in welchem State das System ist
   - Potenzielle Race-Conditions bei State-Übergängen

5. **Display-Logic teilweise in main.cpp**
   - Obwohl Display.cpp/h existiert, sind viele Screen-Updates direkt in main.cpp
   - showQRScreen(), btctickerScreen(), etc. sind in Display.cpp
   - Aber Logic für Screen-Switching ist in main.cpp

## Refactoring-Prioritäten

### ✅ Höchste Priorität

#### 1. State-Machine abstrahieren
**Problem:** 10+ Bool-Flags machen State-Tracking chaotisch
**Lösung:** DeviceState Enum + zentrale State-Management-Funktion
**Aufwand:** Mittel (~4-6 Stunden)
**Nutzen:** Drastisch verbesserte Lesbarkeit, weniger Bugs, einfachere Erweiterung

#### 2. Globale Variablen strukturieren
**Problem:** 50+ globale Variablen ohne Struktur
**Lösung:** Structs für zusammengehörige Daten
**Aufwand:** Niedrig (~2-3 Stunden)
**Nutzen:** Bessere Code-Organisation, einfacher zu verstehen

#### 3. Loop() aufteilen
**Problem:** 1000 Zeilen mit viel Duplicate-Logic
**Lösung:** Separate Funktionen für Touch/Button/Network/Display-Updates
**Aufwand:** Hoch (~6-8 Stunden)
**Nutzen:** Einfacher zu debuggen, wartbarer Code

### ✅ Mittlere Priorität

#### 4. LNURL-Generation centralisieren
**Problem:** In 3 Funktionen verteilt (generateLNURL, updateLightningQR, navigateToNextProduct)
**Lösung:** Single Responsibility für LNURL-Handling
**Aufwand:** Niedrig (~1-2 Stunden)
**Nutzen:** Weniger Duplicate Code

#### 5. Error-Handling abstrahieren
**Problem:** Error-Priority-Logic verteilt über checkAndReconnectWiFi, loop, etc.
**Lösung:** ErrorManager-Klasse mit centralem Priority-Handling
**Aufwand:** Mittel (~3-4 Stunden)
**Nutzen:** Konsistente Error-Behandlung

#### 6. Config-Verwaltung centralisieren
**Problem:** readFiles(), configMode(), SerialConfig verstreut
**Lösung:** ConfigManager-Klasse
**Aufwand:** Mittel (~3-4 Stunden)
**Nutzen:** Einfacher zu testen, wiederverwendbar

### ✅ Niedrige Priorität

#### 7. Helper-Funktionen auslagern
**Problem:** getValue(), encodeBech32() in main.cpp
**Lösung:** Utils.cpp/h erstellen
**Aufwand:** Sehr niedrig (~30 Minuten)
**Nutzen:** Saubere Code-Organisation

#### 8. Konstanten zentralisieren
**Problem:** Timeouts, Debounce-Zeiten, etc. im Code verstreut
**Lösung:** Config.h mit allen Konstanten
**Aufwand:** Sehr niedrig (~30 Minuten)
**Nutzen:** Einfacher zu konfigurieren

## Empfohlene Reihenfolge

1. **Start mit State-Machine** (Punkt 1)
   - Ersetzt sofort 10+ Flags
   - Macht nachfolgende Schritte einfacher
   - Relativ isoliert, geringe Breaking-Change-Gefahr

2. **Dann Globale Variablen strukturieren** (Punkt 2)
   - Baut auf State-Machine auf
   - Macht Code lesbarer für weitere Refactorings

3. **Loop() aufteilen** (Punkt 3)
   - Nutzt neue State-Machine und Structs
   - Größter Nutzen für Wartbarkeit

4. **Rest nach Bedarf**
   - Wenn neue Features kommen oder Bugs auftreten

## Risiken & Mitigation

**Risiko:** Breaking Changes in stabiler Firmware
**Mitigation:** 
- Schritt-für-Schritt vorgehen
- Nach jedem Schritt testen
- Git-Branches für Refactoring nutzen
- Alte Version als Backup behalten

**Risiko:** Zeitaufwand für komplettes Refactoring
**Mitigation:**
- Nur kritische Teile zuerst (State-Machine)
- Rest nur bei Bedarf
- Kann auch später gemacht werden

## Nächste Schritte

Wenn Refactoring gewünscht:
1. Branch erstellen: `git checkout -b refactor/state-machine`
2. State-Machine implementieren (siehe detaillierte Anleitung in separater Sektion)
3. Testen auf Hardware
4. Merge nach main wenn stabil
