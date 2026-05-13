/**
 * test_c3_relay.cpp — Step-by-step debug for ESP32-C3-21-1
 * Adds features from main firmware one by one to find the crash point.
 * Build env: esp32-c3-test
 *
 * HOW TO READ: last printed step = crash happens in the NEXT step.
 */
#include <Arduino.h>
#include <FFat.h>
#include <WiFi.h>

// ESP32-C3 native USB CDC: wait for host to connect (max 3 seconds)
static void waitForSerial() {
    unsigned long t = millis();
    while (!Serial && (millis() - t) < 3000) delay(50);
}

void setup() {
    // ── STEP 0: Serial (confirmed working) ─────────────────────────
    Serial.setRxBufferSize(2048);   // same as main firmware — before begin()
    Serial.begin(115200);
    waitForSerial();
    Serial.println("\n=== C3 STEP-BY-STEP DEBUG ===");
    Serial.print("STEP 0 OK — Serial up at ms="); Serial.println(millis());

    // ── STEP 1: GPIO init (power + LEDs) ───────────────────────────
    Serial.println("STEP 1 — GPIO power/LED pins...");
    // GPIO 15 = SPI Flash WP on ESP32-C3 — SKIP (PIN_POWER_ON=-1 in main firmware)
    Serial.println("  GPIO 15 SKIPPED (SPI flash pin on C3!)");
    pinMode(21, OUTPUT); digitalWrite(21, LOW);    // PIN_LED_BUTTON_LED
    Serial.println("  GPIO 21 OK (PIN_LED_BUTTON_LED)");
    pinMode(2,  OUTPUT); digitalWrite(2,  LOW);    // PIN_ONBOARD_LED
    Serial.println("  GPIO  2 OK (PIN_ONBOARD_LED)");
    Serial.println("STEP 1 OK");

    // ── STEP 2: Relay / SSR GPIOs ───────────────────────────────────
    Serial.println("STEP 2 — Relay/SSR GPIOs...");
    pinMode(4, OUTPUT); digitalWrite(4, LOW);      // PIN_RELAY
    Serial.println("  GPIO  4 OK (PIN_RELAY)");
    pinMode(5, OUTPUT); digitalWrite(5, LOW);      // PIN_SSR
    Serial.println("  GPIO  5 OK (PIN_SSR)");
    Serial.println("STEP 2 OK");

    // ── STEP 3: FFat ────────────────────────────────────────────────
    Serial.println("STEP 3 — FFat.begin(FORMAT_ON_FAIL)...");
    bool fat = FFat.begin(true);
    Serial.print("STEP 3 "); Serial.println(fat ? "OK" : "FAILED (formatted)");

    // ── STEP 4: WiFi mode (no connect) ──────────────────────────────
    Serial.println("STEP 4 — WiFi.mode(WIFI_STA)...");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    Serial.println("STEP 4 OK");

    // ── STEP 5: FreeRTOS task ────────────────────────────────────────
    Serial.println("STEP 5 — xTaskCreatePinnedToCore...");
    xTaskCreatePinnedToCore(
        [](void*) { vTaskDelete(nullptr); },  // task that exits immediately
        "dbg", 2048, nullptr, 1, nullptr, 0);
    Serial.println("STEP 5 OK");

    // ── STEP 6: WiFi.begin with empty SSID ──────────────────────────
    Serial.println("STEP 6 — WiFi.begin(\"\")...");
    WiFi.begin("", "");
    delay(200);
    Serial.println("STEP 6 OK");

    // ── ALL STEPS PASSED ─────────────────────────────────────────────
    Serial.println("\n=== ALL STEPS PASSED — starting loop ===");
    digitalWrite(4, LOW); digitalWrite(5, LOW);
}

void loop() {
    Serial.print("[ALIVE] ms="); Serial.println(millis());
    digitalWrite(4, HIGH); delay(300);
    digitalWrite(4, LOW);  delay(300);
    digitalWrite(5, HIGH); delay(300);
    digitalWrite(5, LOW);  delay(300);
}
