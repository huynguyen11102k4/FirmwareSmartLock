#pragma once

#include <Arduino.h>
#include <MFRC522.h>

class AppState;
class CardRepository;
class PublishService;
class DoorHardware;
struct LockConfig;

class RfidService
{
  public:
    RfidService(
        AppState& appState, CardRepository& cardRepo, PublishService& publish, DoorHardware& door,
        const LockConfig& lockConfig
    );

    void
    begin();
    void
    loop();

  private:
    enum class ScanState : uint8_t
    {
        Idle,
        Trying,
        Held
    };

    String
    getUID_();
    bool
    detectCollision_();

    bool
    isAnyCardPresent_();
    bool
    tryReadUidOnce_(String& outUid);
    void
    cleanupPcd_();

  private:
    AppState& appState_;
    CardRepository& cardRepo_;
    PublishService& publish_;
    DoorHardware& door_;
    const LockConfig& lockConfig_;

    MFRC522 mfrc522_;
    uint32_t lastReadMs_ = 0;

    ScanState scanState_ = ScanState::Idle;
    uint32_t presenceLastSeenMs_ = 0;
    uint32_t lastAttemptMs_ = 0;
    uint16_t failCount_ = 0;
    String heldUid_;
};
