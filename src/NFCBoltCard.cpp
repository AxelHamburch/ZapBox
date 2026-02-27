/**
 * @file NFCBoltCard.cpp
 * @brief NFC Bolt Card reader implementation for ZapBox (PN532 + NTAG424 DNA)
 *
 * See NFCBoltCard.h for full documentation.
 */

#ifdef ENABLE_NFC

#include "NFCBoltCard.h"
#include "PinConfig.h"
#include "GlobalState.h"
#include "Log.h"

#include <Wire.h>
#include <Adafruit_PN532_NTAG424.h>

// ─── Module-private state ────────────────────────────────────────────────────

static Adafruit_PN532 *s_nfc = nullptr;
static TaskHandle_t    s_taskHandle = nullptr;

// ─── FreeRTOS task ───────────────────────────────────────────────────────────

/**
 * Main NFC task loop.
 *
 * Continuously waits for a card using readPassiveTargetID().
 * Because PIN_NFC_IRQ is passed to the Adafruit_PN532 constructor, the library
 * internally polls digitalRead(GPIO1) instead of querying the I2C bus.
 * This means the I2C bus is idle during the entire wait period and is only
 * briefly used (~5 ms) when a card is actually detected and its NDEF file read.
 */
static void nfc_task_code(void *pvParams)
{
    uint8_t uid[7]      = {0};
    uint8_t uidLength   = 0;
    uint8_t fileBuf[512];

    LOG_INFO("NFC", "Bolt Card task started – waiting for cards");

    while (true)
    {
        // Block until a card is detected (up to 30 s per attempt).
        bool found = s_nfc->readPassiveTargetID(
            PN532_MIFARE_ISO14443A, uid, &uidLength, 30000);

        if (!found)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        LOG_INFO("NFC", String("Card detected – UID length: ") + String(uidLength));

        // Accept only 4- or 7-byte UIDs (ISO14443A standard lengths).
        if (uidLength != 4 && uidLength != 7)
        {
            LOG_WARN("NFC", "Incompatible card (unexpected UID length)");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Give the PN532 time to finish the ISO14443A activation sequence
        // before sending NTAG424 APDUs. Without this pause the PN532 is still
        // busy and I2C requestFrom() returns Error -1.
        // 150 ms is enough for even the slowest cards on the shared I2C bus.
        vTaskDelay(pdMS_TO_TICKS(150));

        // Verify the card is an NTAG424 DNA (required for Bolt Card).
        if (!s_nfc->ntag424_isNTAG424())
        {
            LOG_WARN("NFC", "Not an NTAG424 card – Bolt Card requires NTAG424 DNA");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Read the NDEF file from the NTAG424.
        // Retry up to 5 times with 100 ms between attempts – the shared I2C bus
        // (Touch + PN532) and PN532 internal timing can occasionally delay reads.
        uint8_t bytesRead = 0;
        for (int attempt = 1; attempt <= 5 && bytesRead == 0; attempt++)
        {
            bytesRead = s_nfc->ntag424_ISOReadFile(fileBuf, sizeof(fileBuf) - 1);
            if (bytesRead == 0 && attempt < 5)
            {
                LOG_WARN("NFC", String("NTAG424 read attempt ") + attempt + " returned 0 bytes – retrying...");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        if (bytesRead == 0)
        {
            LOG_WARN("NFC", "NTAG424 read returned 0 bytes after 5 attempts");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        fileBuf[bytesRead] = '\0'; // Null-terminate for safe String conversion.

        // Validate LNURLW prefix – Bolt Cards always start with "lnurlw://".
        if (strncmp("lnurlw://", (char *)fileBuf, 9) != 0)
        {
            LOG_WARN("NFC", String("Card does not contain LNURLW (got: ") +
                                String((char *)fileBuf).substring(0, 20) + "...)");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        String lnurlw = String((char *)fileBuf);
        LOG_INFO("NFC", String("✓ LNURLW read: ") + lnurlw.substring(0, 40) + "...");

        // Notify network layer \u2013 sets nfcPaymentPending, shows PENDING NFC screen,
        // and sends the HTTP POST. Flag stays true until QR screen is restored.
        nfcLnurlwReceived(lnurlw);
        // Simple debounce: prevent immediate re-detection of the same card.
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool nfcBoltCardInit()
{
    LOG_INFO("NFC", "Initializing PN532 Bolt Card reader...");
    LOG_INFO("NFC", "  I2C: SDA=GPIO18, SCL=GPIO17  (shared with Touch controller)");
    LOG_INFO("NFC", String("  IRQ: GPIO") + String(PIN_NFC_IRQ) + " (active LOW, INPUT_PULLUP)");
    LOG_INFO("NFC", "  I2C address: 0x24");

    // Quick I2C probe: check if anything responds at PN532 address 0x24.
    // beginTransmission / endTransmission returns 0 only if a device ACKs.
    // This avoids 20+ seconds of getFirmwareVersion() retries when no PN532 is connected.
    // Only call Wire.begin() if the bus is not already running (touch.begin() initializes it).
    // Calling Wire.begin() on an already-running bus can disrupt ongoing I2C operations.
    #if !ENABLE_DISPLAY
    Wire.begin(18, 17); // headless: no touch controller initializes Wire, so do it here
    #endif
    Wire.beginTransmission(0x24);
    uint8_t probeResult = Wire.endTransmission();
    if (probeResult != 0) {
        LOG_INFO("NFC", String("No device at I2C address 0x24 (probe result: ") + String(probeResult) + ") – PN532 not connected");
        LOG_INFO("NFC", "  → Bolt Card feature disabled, normal operation unaffected");
        return false;
    }
    LOG_INFO("NFC", "I2C probe OK – device found at 0x24");

    // Wire was already initialized by TouchCST816S::begin() on GPIO17/18.
    // Create the PN532 instance using the shared Wire bus.
    // Passing PIN_NFC_IRQ makes the library use digitalRead() for card detection
    // instead of I2C polling – critical for not blocking the shared I2C bus.
    s_nfc = new Adafruit_PN532(PIN_NFC_IRQ, /* rst= */ -1, &Wire);

    if (!s_nfc->begin())
    {
        LOG_ERROR("NFC", "PN532 begin() failed – hardware not found or I2C error");
        LOG_ERROR("NFC", "  → Bolt Card feature disabled, normal operation unaffected");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Verify PN532 firmware version.
    uint32_t versiondata = s_nfc->getFirmwareVersion();
    if (!versiondata)
    {
        LOG_ERROR("NFC", "PN532 firmware version not readable");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    LOG_INFO("NFC", String("Found PN532 chip v") +
                        String((versiondata >> 24) & 0xFF) + ", firmware v" +
                        String((versiondata >> 16) & 0xFF) + "." +
                        String((versiondata >> 8) & 0xFF));

    // Configure PN532 Security Access Module for passive card reading.
    if (!s_nfc->SAMConfig())
    {
        LOG_ERROR("NFC", "SAMConfig failed");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Create NFC task BEFORE attaching the interrupt so s_taskHandle is valid
    // when the ISR first fires.
    BaseType_t res = xTaskCreatePinnedToCore(
        nfc_task_code,
        "NFCBoltCard",
        8192,          // Stack: NTAG424 operations need more than a basic PN532 task
        nullptr,
        1,             // Priority 1 – equal to Task1 (button/touch handler)
        &s_taskHandle,
        0              // Core 0 – same as Task1 and the main loop task
    );

    if (res != pdTRUE || s_taskHandle == nullptr)
    {
        LOG_ERROR("NFC", "Failed to create NFC FreeRTOS task");
        delete s_nfc;
        s_nfc = nullptr;
        return false;
    }

    // Pull-up on IRQ pin (library sets INPUT_PULLUP in begin(), but be explicit).
    pinMode(PIN_NFC_IRQ, INPUT_PULLUP);

    LOG_INFO("NFC", "✓ Bolt Card reader ready");
    return true;
}

#endif // ENABLE_NFC
