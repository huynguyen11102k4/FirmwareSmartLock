#include "app/services/RfidService.h"

#include "app/services/Topics.h"
#include "config/HardwarePins.h"
#include "network/MqttManager.h"
#include "utils/Logger.h"

#include <SPI.h>

RfidService::RfidService(
    AppState& appState,
    CardRepository& cardRepo,
    PublishService& publish,
    DoorHardware& door,
    const LockConfig& lockConfig
)
    : appState_(appState)
    , cardRepo_(cardRepo)
    , publish_(publish)
    , door_(door)
    , lockConfig_(lockConfig)
    , mfrc522_(SS_PIN, RST_PIN)
{
}

void
RfidService::begin()
{
    Logger::info("RFID", "=== RFID SERVICE INIT ===");

    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
    mfrc522_.PCD_Init();

    Logger::info("RFID", "MFRC522 initialized");

    Logger::info(
        "RFID",
        "Stored cards count: %d",
        cardRepo_.size()
    );

    Logger::info(
        "RFID",
        "Swipe-add mode: %s",
        appState_.runtimeFlags.swipeAddMode ? "ON" : "OFF"
    );

    Logger::info("RFID", "=========================");
}

String
RfidService::getUID_()
{
    String uid;
    uid.reserve(2 * mfrc522_.uid.size);

    for (byte i = 0; i < mfrc522_.uid.size; i++)
    {
        if (mfrc522_.uid.uidByte[i] < 0x10)
            uid += "0";
        uid += String(mfrc522_.uid.uidByte[i], HEX);
    }

    uid.toUpperCase();
    return uid;
}

bool
RfidService::detectCollision_()
{
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);

    const MFRC522::StatusCode status =
        mfrc522_.PICC_RequestA(bufferATQA, &bufferSize);

    if (status == MFRC522::STATUS_COLLISION)
    {
        Logger::info("RFID", "Collision detected (multiple cards)");
        return true;
    }
    return false;
}

void
RfidService::loop()
{
    if (appState_.runtimeFlags.swipeAddMode &&
        appState_.swipeAdd.isTimeout())
    {
        Logger::info("RFID", "Swipe-add timeout");

        appState_.runtimeFlags.swipeAddMode = false;
        appState_.swipeAdd.reset();

        MqttManager::publish(
            Topics::iccardsStatus(appState_.mqttTopicPrefix),
            "swipe_add_timeout"
        );
    }

    const uint32_t debounceMs = lockConfig_.rfidDebounceMs;
    if ((uint32_t)(millis() - lastReadMs_) < debounceMs)
        return;

    if (!mfrc522_.PICC_IsNewCardPresent())
        return;

    if (detectCollision_())
    {
        delay(100);
        return;
    }

    if (!mfrc522_.PICC_ReadCardSerial())
    {
        Logger::info("RFID", "Failed to read card serial");
        return;
    }

    lastReadMs_ = millis();
    const String uid = getUID_();

    Logger::info("RFID", "Card detected UID=%s", uid.c_str());

    // --- Swipe add flow ---
    if (appState_.runtimeFlags.swipeAddMode)
    {
        if (!appState_.swipeAdd.hasFirstSwipe())
        {
            Logger::info("RFID", "Swipe-add first card: %s", uid.c_str());

            appState_.swipeAdd.recordFirstSwipe(
                uid,
                lockConfig_.swipeAddTimeoutMs
            );

            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix),
                "swipe_first_detected"
            );
        }
        else if (appState_.swipeAdd.matchesFirstSwipe(uid))
        {
            Logger::info("RFID", "Swipe-add confirmed: %s", uid.c_str());

            if (cardRepo_.add(uid))
            {
                Logger::info("RFID", "Card added to repository: %s", uid.c_str());
                publish_.publishICCardList();
            }

            appState_.runtimeFlags.swipeAddMode = false;
            appState_.swipeAdd.reset();

            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix),
                "swipe_add_success"
            );
        }
        else
        {
            Logger::info(
                "RFID",
                "Swipe-add failed (UID mismatch): %s",
                uid.c_str()
            );

            appState_.runtimeFlags.swipeAddMode = false;
            appState_.swipeAdd.reset();

            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix),
                "swipe_add_failed"
            );
        }
    }

    // --- Normal auth flow ---
    if (cardRepo_.exists(uid))
    {
        Logger::info("RFID", "Card AUTH SUCCESS: %s", uid.c_str());
        door_.requestUnlock("card");
    }
    else if (cardRepo_.isEmpty())
    {
        Logger::info(
            "RFID",
            "Card repo empty → auto-add card: %s",
            uid.c_str()
        );

        if (cardRepo_.add(uid))
        {
            publish_.publishICCardList();
        }
    }
    else
    {
        Logger::info("RFID", "Card AUTH FAILED: %s", uid.c_str());
    }

    mfrc522_.PICC_HaltA();
    mfrc522_.PCD_StopCrypto1();
}
