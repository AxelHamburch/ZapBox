#!/usr/bin/env python3
"""
Test-Script für ZapBox Serial Config
Testet die serielle Kommunikation mit dem ESP32
"""

import serial
import time
import sys

# Passe den COM-Port an
COM_PORT = "COM28"  # Ändere dies zu deinem Port
BAUD_RATE = 115200

def test_serial_connection():
    print(f"Verbinde mit {COM_PORT} bei {BAUD_RATE} baud...")
    
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=2)
        time.sleep(2)  # Warte auf Verbindung
        
        print("Verbindung hergestellt!")
        print("\n--- Sende /hello Befehl ---")
        ser.write(b"/hello\n")
        ser.flush()
        
        # Lese Antwort
        time.sleep(0.5)
        while ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"<< {line}")
        
        print("\n--- Teste /file-read config.json ---")
        ser.write(b"/file-read config.json\n")
        ser.flush()
        
        time.sleep(0.5)
        while ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"<< {line}")
        
        print("\n--- Teste /file-remove und /file-append ---")
        test_config = '[{"name":"ssid","value":"TestWiFi"},{"name":"wifipassword","value":"TestPass123"}]'
        
        ser.write(b"/file-remove test.json\n")
        ser.flush()
        time.sleep(0.2)
        
        cmd = f"/file-append test.json {test_config}\n"
        ser.write(cmd.encode('utf-8'))
        ser.flush()
        
        time.sleep(0.5)
        while ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"<< {line}")
        
        print("\n--- Lese Test-Datei ---")
        ser.write(b"/file-read test.json\n")
        ser.flush()
        
        time.sleep(0.5)
        while ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"<< {line}")
        
        print("\n--- Test abgeschlossen ---")
        print("Drücke Ctrl+C zum Beenden oder warte auf weitere Nachrichten...")
        
        # Warte auf weitere Nachrichten
        try:
            while True:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"<< {line}")
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nBeendet durch Benutzer")
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"Fehler beim Öffnen des Ports: {e}")
        print(f"\nVerfügbare Ports:")
        from serial.tools import list_ports
        for port in list_ports.comports():
            print(f"  {port.device} - {port.description}")
        sys.exit(1)
    except Exception as e:
        print(f"Fehler: {e}")
        sys.exit(1)

if __name__ == "__main__":
    test_serial_connection()
