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
void initWiFiEventHandler();
void checkWiFiStatus();
void checkAndReconnectWiFi();

// ─── NFC Bolt Card (ENABLE_NFC=1 only) ───────────────────────────────────────
// Called by NFCBoltCard.cpp when a valid LNURLW is read from a Bolt Card.
// Sends: {"event":"lnurlw", "lnurlw":"lnurlw://...", "pin":<activePin>}
// The server (bitcoinswitch_extension) must handle this event:
//   1. Create a Lightning invoice for <pin>
//   2. Call the LNURLW callback with the invoice
//   3. Send the normal "paid" WS event when settled
#ifdef ENABLE_NFC
void nfcLnurlwReceived(const String &lnurlw);
// Called from main.cpp when the user has entered 4 PIN digits.
// HTTP POSTs the PIN to the zapbox_extension so it can complete the LNURLW callback.
void sendPinSubmit(const String &sessionId, const String &pin);
#endif

// ─── Authy (LNURL-auth) teach session ────────────────────────────────────────
// Open / close a PIN-protected teach session on the zapbox_extension. The teach
// endpoint answers synchronously (unlike the WS-relayed payment PIN).
bool submitTeachPin(const String &pin);
void stopTeachSession();

#endif // NETWORK_H
