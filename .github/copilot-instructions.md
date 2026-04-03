---
name: zapbox-firmware
description: "ZapBox is a Lightning-controlled switching device firmware built with PlatformIO for ESP32-S3 (T-Display-S3) and ESP32 headless variants. Core technologies: Arduino, FreeRTOS, REST API, WebSocket, Lightning Network."
---

# ZapBox Firmware Development

## Quick Start

### Build
```bash
# Build for T-Display-S3 (default with display)
platformio run

# Build headless variant (ESP32 Dev)
platformio run -e esp32dev

# Clean and rebuild
platformio run --target clean
platformio run
```

### Upload/Flash
```bash
# Auto-detect COM port and flash
platformio run -t upload

# Monitor serial output (115200 baud, with exception decoder)
platformio device monitor
```

## Project Structure

| Component | File(s) | Responsibility |
|-----------|---------|-----------------|
| **State Machine** | `src/DeviceState.h`, `src/GlobalState.h` | Central state tracking: INITIALIZING → CONNECTING_WIFI → READY → RECEIVING_PAYMENT |
| **Display/UI** | `src/Display.h/.cpp`, `src/UI.h/.cpp` | Screen rendering (QR, ticker, config screens); FreeRTOS mutex for thread-safe SPI |
| **Input Handling** | `src/Input.h/.cpp`, `src/Navigation.h/.cpp` | OneButton multi-click detection (1=Help, 2=Report, 4=Config) |
| **Payment Logic** | `src/Payment.h/.cpp` | LNURL generation, Lightning QR codes, invoice tracking |
| **Network** | `src/Network.h/.cpp` | WiFi + WebSocket listener for LNbits "paid" events |
| **API/External** | `src/API.h/.cpp` | Fetch device config from LNbits, Bitcoin price/block from APIs |
| **Hardware Control** | `src/ServoControl.h/.cpp`, `src/NFCPN532.h/.cpp` | Multi-relay control, NFC Bolt Card reader (PN532) |
| **Config** | `src/SerialConfig.h`, `src/PinConfig.h` | WiFi setup via serial, GPIO pin definitions |

## Architecture Patterns

### State-Driven Design
```
main.cpp (FreeRTOS tasks)
  ├─ Loop: ~100ms tick cycle
  └─ GlobalState (centralized config)
       └─ StateManager (device FSM)
            ├─ Navigation (buttons/touch)
            ├─ Network/API (external services)
            └─ Display (event-driven UI updates)
```

**Key Principle**: All logic branches on `GlobalState::currentState`. Display updates are **event-driven** — triggered by state transitions, not polled.

### Thread Safety
- **Display rendering** protected by FreeRTOS mutex (`initDisplayMutex()`)
- **SPI bus** serialized between Core 0 and Core 1
- **WebSocket events** queued from interrupt context, processed in main loop

## Development Conventions

### Logging
```cpp
// Controlled by build flags: LOG_ENABLE, LOG_LEVEL
LOG_ERROR("Failed: %s", reason);    // Always shown
LOG_WARN("Config warning");          // If LOG_LEVEL >= WARN
LOG_INFO("WiFi connected");          // If LOG_LEVEL >= INFO  
LOG_DEBUG("Tick %d", millis());      // If LOG_LEVEL >= DEBUG
```

### Build Configuration
Edit `platformio.ini` → `[common]` section:
- `PRODUCT_TIMEOUT`: Time (ms) before UI auto-return timeout (default: 30000)
- `BTCTICKER_TIMEOUT`: Ticker display duration (default: 10000)
- `LOG_ENABLE`: 1=on, 0=off (saves memory)
- `LOG_LEVEL`: Error < Warn < Info < Debug

### Version Tracking
- Format: `vBLOCKHEIGHT` (e.g., `v943376`)
- Update version in `platformio.ini` `[common]` → `build_flags_common`
- Get current Bitcoin block height: `https://mempool.space/api/blocks/tip/height`

### State Naming
All device states use explicit enum names in `DeviceState.h`:
```cpp
enum class DeviceState {
  INITIALIZING, CONNECTING_WIFI, READY, RECEIVING_PAYMENT, ERROR_CRITICAL, ...
};
```

## Key Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| ArduinoJson | ^7.2.1 | JSON parsing (config, API responses) |
| OneButton | ^2.6.1 | Debounced multi-click button handling |
| WebSockets | ^2.6.1 | WebSocket client for LNbits real-time events |
| QRCode (ricmoo) | GitHub | QR code generation for LNURL |
| TFT_eSPI | ^2.5.43 | Display driver (ST7789 for T-Display-S3) |
| Adafruit-PN532-NTAG424 | GitHub | NFC reader (Bolt Card support) |
| ESP32Servo | ^3.0.6 | Multi-channel relay/PWM control |

## Configuration via Serial

On first boot, the device enters configuration mode via serial:
- Enter WiFi SSID/password
- Enter LNbits server URL
- Enter device ID / switch string
- Exit config (4-click button or serial command)

Logs all outputs at **115200 baud** with ESP32 exception decoder enabled.

## Feature Flags (compile-time)

Toggle features in `platformio.ini`:
- `ENABLE_DISPLAY=1`: Show display on ST7789 (T-Display-S3)
- `ENABLE_DISPLAY=0`: Headless variant (status LED only, ESP32)
- `ENABLE_NFC=1`: Enable PN532 NFC reader (Bolt Card support)
- `ENABLE_NFC_TEST=1`: Display NFC test screen w/ reader diagnostics

## Common Issues & Fixes

| Issue | Fix |
|-------|-----|
| **Compilation fails: `ArduinoJson.h not found`** | Run `platformio lib install` to download all dependencies |
| **Serial monitor shows gibberish** | Verify baud rate is **115200** in `platformio.ini` [common] |
| **Display doesn't update during payment** | Check FreeRTOS mutex initialization; review `initDisplayMutex()` |
| **WebSocket connection timeout** | Verify WiFi is connected; check LNbits server URL in config |
| **NFC reader not detected** | Verify PN532 I2C address (0x48); check `ENABLE_NFC` flag |

## Testing

**Current approach**: Hardware-in-loop validation
- No traditional unit tests (PlatformIO test framework available but not in use)
- **Hardware variants**: Test on both T-Display-S3 (display) and ESP32 Dev (headless)
- **Logging-based**: Serial logs enable field inspection of state machines, API responses

**Test coverage**:
- State machine transitions (manual inspection via logs)
- JSON parsing (ArduinoJson validation on real API responses)
- WebSocket event handling (monitor "paid" event processing)

## Hardware Variants

### T-Display-S3 (Full-featured)
- **Display**: 170×320 ST7789 (8-bit parallel, SPI via TFT_eSPI)
- **Buttons**: 2 physical buttons + touch support
- **Flash**: 16MB (configured in `partitions_16mb.csv`)
- **Build env**: `lilygo-t-display-s3` (default)

### ESP32 Dev (Headless)
- **No display** — uses `DisplayStubs.cpp` (no-op implementations)
- **Status LED**: GPIO-controlled indicator
- **Flash**: 4MB (configured in `partitions_4mb.csv`)
- **Build env**: `esp32dev`
- **Use case**: Embedded relay control without UI

## Related Docs

- [README.md](../README.md) — Project overview, use cases
- [FIRMWARE.md](../FIRMWARE.md) — Detailed firmware features & release notes
- [zapbox.space](https://zapbox.space/) — User documentation & examples
- [LNbits Zapbox Extension](../zapbox_extension/) — Backend/payment server

