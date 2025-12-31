#pragma once
#include "app/services/PublishService.h"
#include "config/LockConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "storage/CardRepository.h"

#include <Arduino.h>
#include <MFRC522.h>

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
    String
    getUID_();

    bool
    detectCollision_();

    AppState& appState_;
    CardRepository& cardRepo_;
    PublishService& publish_;
    DoorHardware& door_;
    const LockConfig& lockConfig_;

    MFRC522 mfrc522_;
    uint32_t lastReadMs_{0};
};
