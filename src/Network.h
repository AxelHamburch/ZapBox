#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WebSocketsClient.h>

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

// Network connectivity checks
bool checkInternetConnectivity();
bool checkServerReachability();

// WiFi monitoring and recovery
void checkWiFiStatus();
void checkAndReconnectWiFi();

#endif // NETWORK_H
