# How to Flash Firmware with Binary Files

This guide shows how to flash the ZapBox firmware using only the three binary files: `bootloader.bin`, `firmware.bin`, and `partitions.bin`.

## Method 1: Using esptool.py (Command Line)

### Installation
```bash
pip install esptool
```

### Flash Command
```bash
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash -z \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

### Important Notes
- **Port:** Replace `COM3` with your actual port:
  - Windows: `COM3`, `COM4`, etc.
  - Linux/Mac: `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.
- **Baud Rate:** `921600` is faster, use `115200` if you experience issues
- **Addresses:** Must be exact for ESP32-S3:
  - `0x0` = Bootloader
  - `0x8000` = Partition Table
  - `0x10000` = Firmware (64KB offset)

### Optional: Erase Flash First
```bash
esptool.py --chip esp32s3 --port COM3 erase_flash
```

## Method 2: Web-Based Tool (No Installation Required)

1. Open: [https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/)
2. Connect your device via USB
3. Click **Connect** and select your port
4. Add the three files with their addresses:
   - `0x0` → `bootloader.bin`
   - `0x8000` → `partitions.bin`
   - `0x10000` → `firmware.bin`
5. Click **Program**

## Troubleshooting

- **Port not found:** Install [CP210x drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **Connection failed:** Hold **BOOT** button while connecting USB
- **Verification failed:** Try lower baud rate (`115200`)
- **Permission denied (Linux):** Run `sudo usermod -a -G dialout $USER` and reboot

## Alternative: Use ZapBox Web Installer

The easiest method is using the official [ZapBox Web Installer](https://zapbox.space) which handles everything automatically.
