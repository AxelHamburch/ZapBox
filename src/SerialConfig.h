#pragma once

struct KeyValue
{
    String key;
    String value;
};

void configOverSerialPort(String wifiSSID, String wifiPass);
void executeConfig(String wifiSSID, String wifiPass);
void executeCommand(String commandName, String commandData);
void removeFile(String path);
void appendToFile(String path, String data);
void readFile(String path);
KeyValue extractKeyValue(String s);