#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

// Global "one TLS connection at a time" gate.
//
// Two tasks open TLS connections to the same host:443 independently:
//   Core 1 (main loop)  – WebSocket beginSSL() / auto-reconnect, label + BTC fetches
//   Core 0 (NFC task)   – the Bolt Card LNURLW POST in nfcLnurlwReceived()
//
// On a well-behaved router that's fine. On routers with a small/slow
// connection-tracking table (the Fritzbox this was traced on) a second port-443
// handshake opened while another one is in flight gets silently dropped, and
// both sides then sit in retransmit backoff — the WebSocket dies, the NFC POST
// hangs, and the library's 1 s auto-reconnect turns it into a handshake storm.
// The same effect is already worked around at boot with a hard-coded 3 s gap
// between the TCP server probe and the first WebSocket handshake (main.cpp).
//
// This gate generalises that workaround:
//   - Only one TLS connection is established at a time, across both cores.
//   - After a connection is released, NET_TLS_SETTLE_MS must pass before the
//     next handshake starts, so the router can release its conntrack entry.
//
// It deliberately does NOT gate reads/writes on an already-established session
// (a plain webSocket.loop() on a live connection opens nothing and must never
// be delayed) — only the operations that open a new connection.
//
// The mutex is recursive: fetchSwitchLabels() is called from inside
// webSocketEvent(), which may already run under the gate.

// Minimum quiet time between one TLS connection closing and the next opening.
#ifndef NET_TLS_SETTLE_MS
#define NET_TLS_SETTLE_MS 1000
#endif

// Call once in setup(), before WiFi comes up.
void netTlsInit();

// Acquire the TLS slot, waiting up to timeoutMs. Returns false on timeout, in
// which case the caller must NOT open a connection. `who` is only for logging.
bool netTlsTake(const char *who, uint32_t timeoutMs);

// Non-blocking acquire. Returns false if another task holds the slot or the
// settle window hasn't elapsed yet — used by the WebSocket auto-reconnect path,
// where "try again next loop iteration" is the correct behaviour.
bool netTlsTryTake(const char *who);

// Release the slot and start the settle window.
void netTlsGive();

// RAII wrapper for functions with several exit points — releases the slot on
// every return path. Check held() before opening a connection.
class NetTlsGuard {
public:
    NetTlsGuard(const char *who, uint32_t timeoutMs) : _held(netTlsTake(who, timeoutMs)) {}
    ~NetTlsGuard() { if (_held) netTlsGive(); }
    bool held() const { return _held; }
    NetTlsGuard(const NetTlsGuard &)            = delete;
    NetTlsGuard &operator=(const NetTlsGuard &) = delete;
private:
    bool _held;
};
