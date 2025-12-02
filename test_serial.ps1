# Test-Script für ZapBox Serial Config
# Testet die serielle Kommunikation mit dem ESP32

$ComPort = "COM28"  # Ändere dies zu deinem Port
$BaudRate = 115200

Write-Host "Verbinde mit $ComPort bei $BaudRate baud..." -ForegroundColor Cyan

try {
    # Erstelle SerialPort-Objekt
    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = $ComPort
    $port.BaudRate = $BaudRate
    $port.Parity = [System.IO.Ports.Parity]::None
    $port.DataBits = 8
    $port.StopBits = [System.IO.Ports.StopBits]::One
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    
    $port.Open()
    Start-Sleep -Seconds 2
    
    Write-Host "Verbindung hergestellt!" -ForegroundColor Green
    
    # Sende /hello Befehl
    Write-Host "`n--- Sende /hello Befehl ---" -ForegroundColor Yellow
    $port.WriteLine("/hello")
    Start-Sleep -Milliseconds 500
    
    # Lese Antwort
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    # Teste /file-read config.json
    Write-Host "`n--- Teste /file-read config.json ---" -ForegroundColor Yellow
    $port.WriteLine("/file-read config.json")
    Start-Sleep -Milliseconds 500
    
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    # Teste /file-remove und /file-append
    Write-Host "`n--- Teste /file-remove und /file-append ---" -ForegroundColor Yellow
    $testConfig = '[{"name":"ssid","value":"TestWiFi"},{"name":"wifipassword","value":"TestPass123"}]'
    
    $port.WriteLine("/file-remove test.json")
    Start-Sleep -Milliseconds 200
    
    $port.WriteLine("/file-append test.json $testConfig")
    Start-Sleep -Milliseconds 500
    
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    # Lese Test-Datei
    Write-Host "`n--- Lese Test-Datei ---" -ForegroundColor Yellow
    $port.WriteLine("/file-read test.json")
    Start-Sleep -Milliseconds 500
    
    while ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host "<< $line" -ForegroundColor Gray
    }
    
    Write-Host "`n--- Test abgeschlossen ---" -ForegroundColor Green
    Write-Host "Drücke Ctrl+C zum Beenden oder warte auf weitere Nachrichten..." -ForegroundColor Cyan
    
    # Warte auf weitere Nachrichten
    try {
        while ($true) {
            if ($port.BytesToRead -gt 0) {
                $line = $port.ReadLine()
                Write-Host "<< $line" -ForegroundColor Gray
            }
            Start-Sleep -Milliseconds 100
        }
    } catch {
        Write-Host "`nBeendet durch Benutzer" -ForegroundColor Yellow
    }
    
    $port.Close()
    
} catch {
    Write-Host "Fehler: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nVerfügbare Ports:" -ForegroundColor Cyan
    [System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object {
        Write-Host "  $_" -ForegroundColor Gray
    }
    exit 1
}
