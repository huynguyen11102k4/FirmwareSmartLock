#include "app/services/RfidService.h"

#include "app/services/PublishService.h"
#include "app/services/Topics.h"
#include "config/HardwarePins.h"
#include "config/LockConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "network/MqttManager.h"
#include "storage/CardRepository.h"
#include "utils/Logger.h"
#include "utils/TimeUtils.h"

#include <SPI.h>

namespace
{
String
defaultCardNameNext(const CardRepository& repo)
{
    return "ICCard" + String((int)(repo.size() + 1));
}
} // namespace

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

#if defined(ESP32)
    SPI.setFrequency(1000000);
#endif

    mfrc522_.PCD_Init();
    delay(50);

    mfrc522_.PCD_AntennaOn();
    mfrc522_.PCD_SetAntennaGain(mfrc522_.RxGain_max);
    mfrc522_.PCD_DumpVersionToSerial();
}

void
RfidService::cleanupPcd_()
{
    mfrc522_.PICC_HaltA();
    mfrc522_.PCD_StopCrypto1();
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
        Logger::info("RFID", "Collision detected (multiple cards)");
        return true;
    }
    return false;
}

bool
RfidService::isAnyCardPresent_()
{
    byte atqa[2] = {0, 0};
    byte atqaSize = sizeof(atqa);

    const MFRC522::StatusCode st = mfrc522_.PICC_WakeupA(atqa, &atqaSize);
    const bool present = (st == MFRC522::STATUS_OK || st == MFRC522::STATUS_COLLISION);

    cleanupPcd_();
    return present;
}

bool
RfidService::tryReadUidOnce_(String& outUid)
{
    if (!mfrc522_.PICC_ReadCardSerial())
        return false;

    outUid = getUID_();
    return true;
}

void
RfidService::loop()
{
    static uint32_t lastLoopMs = 0;
    if (millis() - lastLoopMs < 30)
        return;
    lastLoopMs = millis();

    if (appState_.runtimeFlags.swipeAddMode && appState_.swipeAdd.isTimeout())
    {
        appState_.runtimeFlags.swipeAddMode = false;
        appState_.swipeAdd.reset();

        publish_.publishLog("HandleCardFailed", "SwipeAdd", "Hết thời gian chờ quét thẻ");
    }

    constexpr uint32_t kRemoveGraceMs = 400;
    constexpr uint32_t kAttemptIntervalMs = 30;
    constexpr uint16_t kReinitEveryFails = 25;

    // Post-success debounce (chỉ áp dụng khi đang Idle)
    const uint32_t debounceMs = lockConfig_.rfidDebounceMs;
    if (scanState_ == ScanState::Idle && (uint32_t)(millis() - lastReadMs_) < debounceMs)
    {
        return;
    }

    const bool presentNow = isAnyCardPresent_();
    if (presentNow)
        presenceLastSeenMs_ = millis();

    switch (scanState_)
    {
        case ScanState::Idle:
        {
            if (!presentNow)
                return;

            scanState_ = ScanState::Trying;
            lastAttemptMs_ = 0;
            failCount_ = 0;
            heldUid_ = "";
            return;
        }

        case ScanState::Trying:
        {
            if (!presentNow && (uint32_t)(millis() - presenceLastSeenMs_) > kRemoveGraceMs)
            {
                scanState_ = ScanState::Idle;
                return;
            }

            if ((uint32_t)(millis() - lastAttemptMs_) < kAttemptIntervalMs)
                return;
            lastAttemptMs_ = millis();

            if (detectCollision_())
            {
                cleanupPcd_();
                return;
            }

            String uid;
            if (!tryReadUidOnce_(uid))
            {
                failCount_++;
                cleanupPcd_();

                if (failCount_ % kReinitEveryFails == 0)
                {
                    mfrc522_.PCD_Init();
                    delay(10);
                    mfrc522_.PCD_AntennaOn();
                    mfrc522_.PCD_SetAntennaGain(mfrc522_.RxGain_max);
                }
                return;
            }

            lastReadMs_ = millis();
            Logger::info("RFID", "Card detected UID=%s", uid.c_str());

            if (appState_.runtimeFlags.swipeAddMode)
            {
                if (!appState_.swipeAdd.hasFirstSwipe())
                {
                    Logger::info("RFID", "Swipe-add first card: %s", uid.c_str());

                    appState_.swipeAdd.recordFirstSwipe(uid, lockConfig_.swipeAddTimeoutMs);
                }
                else if (appState_.swipeAdd.matchesFirstSwipe(uid))
                {
                    Logger::info("RFID", "Swipe-add confirmed: %s", uid.c_str());

                    const String name = defaultCardNameNext(cardRepo_);
                    if (cardRepo_.add(uid, name))
                    {
                        cardRepo_.setTs((uint64_t)TimeUtils::nowSeconds());
                        Logger::info("RFID", "Card added to repository: %s", uid.c_str());
                        publish_.publishLog("CardAdded", "SwipeAdd", "Thêm Card thành công");
                        publish_.publishICCardList();
                    }

                    appState_.runtimeFlags.swipeAddMode = false;
                    appState_.swipeAdd.reset();
                }
                else
                {
                    Logger::info("RFID", "Swipe-add failed (UID mismatch): %s", uid.c_str());

                    appState_.runtimeFlags.swipeAddMode = false;
                    appState_.swipeAdd.reset();

                    publish_.publishLog("HandleCardFailed", "SwipeAdd", "Thêm thất bại do quét không cùng thẻ");
                }

                cleanupPcd_();
                scanState_ = ScanState::Held;
                heldUid_ = uid;
                return;
            }

            // --- Normal auth flow ---
            if (cardRepo_.exists(uid))
            {
                Logger::info("RFID", "Card AUTH SUCCESS: %s", uid.c_str());
                door_.requestUnlock("Card");
            }
            else
            {
                Logger::info("RFID", "Card AUTH FAILED: %s", uid.c_str());
            }

            cleanupPcd_();

            scanState_ = ScanState::Held;
            heldUid_ = uid;
            return;
        }

        case ScanState::Held:
        {
            if (presentNow)
                return;

            if ((uint32_t)(millis() - presenceLastSeenMs_) <= kRemoveGraceMs)
                return;

            scanState_ = ScanState::Idle;
            heldUid_ = "";
            return;
        }
    }
}
