#include "app/services/RfidService.h"

#include "app/services/Topics.h"
#include "config/HardwarePins.h"
#include "network/MqttManager.h"

#include <SPI.h>

RfidService::RfidService(
    AppState& appState, CardRepository& cardRepo, PublishService& publish, DoorHardware& door,
    const LockConfig& lockConfig
)
    : appState_(appState), cardRepo_(cardRepo), publish_(publish), door_(door),
      lockConfig_(lockConfig), mfrc522_(SS_PIN, RST_PIN)
{
}

void
RfidService::begin()
{
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
    mfrc522_.PCD_Init();
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

    const MFRC522::StatusCode status = mfrc522_.PICC_RequestA(bufferATQA, &bufferSize);
    if (status == MFRC522::STATUS_COLLISION)
    {
        publish_.publishLog("rfid_collision", "warning", "Multiple cards detected");
        return true;
    }
    return false;
}

void
RfidService::loop()
{
    if (appState_.runtimeFlags.swipeAddMode && appState_.swipeAdd.isTimeout())
    {
        appState_.runtimeFlags.swipeAddMode = false;
        appState_.swipeAdd.reset();
        MqttManager::publish(Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_add_timeout");
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
        return;

    lastReadMs_ = millis();
    const String uid = getUID_();

    if (appState_.runtimeFlags.swipeAddMode)
    {
        if (!appState_.swipeAdd.hasFirstSwipe())
        {
            appState_.swipeAdd.recordFirstSwipe(uid, lockConfig_.swipeAddTimeoutMs);
            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_first_detected"
            );
        }
        else if (appState_.swipeAdd.matchesFirstSwipe(uid))
        {
            if (cardRepo_.add(uid))
            {
                publish_.publishICCardList();
            }

            appState_.runtimeFlags.swipeAddMode = false;
            appState_.swipeAdd.reset();
            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_add_success"
            );
            publish_.publishLog("card_added", "swipe", uid);
        }
        else
        {
            appState_.runtimeFlags.swipeAddMode = false;
            appState_.swipeAdd.reset();
            MqttManager::publish(
                Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_add_failed"
            );
        }
    }

    if (cardRepo_.exists(uid))
    {
        door_.requestUnlock("card");
    }
    else if (cardRepo_.isEmpty())
    {
        if (cardRepo_.add(uid))
        {
            publish_.publishICCardList();
        }
    }

    publish_.publishLog("card_scan", "card", uid);

    mfrc522_.PICC_HaltA();
    mfrc522_.PCD_StopCrypto1();
}