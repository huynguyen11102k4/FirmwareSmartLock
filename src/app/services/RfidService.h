#pragma once
#include "app/services/PublishService.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "storage/CardRepository.h"

#include <Arduino.h>
#include <MFRC522.h>
#include <vector>

class RfidService
{
  public:
    using SyncFn = void (*)(void* ctx);

    RfidService(
        AppState& appState, CardRepository& cardRepo, std::vector<String>& iccardsCache,
        PublishService& publish, void* ctx, SyncFn syncCache, DoorHardware& door
    );

    void
    begin();

    void
    loop();

  private:
    String
    getUID_();

    AppState& appState_;
    CardRepository& cardRepo_;
    std::vector<String>& iccardsCache_;
    PublishService& publish_;

    void* ctx_{nullptr};
    SyncFn syncCache_{nullptr};

    DoorHardware& door_;

    MFRC522 mfrc522_;
};