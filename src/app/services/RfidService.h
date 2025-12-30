#pragma once
#include <Arduino.h>
#include <MFRC522.h>
#include <vector>

#include "models/AppState.h"
#include "storage/CardRepository.h"
#include "app/services/PublishService.h"

class RfidService {
public:
  using SyncFn = void (*)(void* ctx);
  using UnlockFn = void (*)(void* ctx, const String& method);

  RfidService(AppState& appState,
              CardRepository& cardRepo,
              std::vector<String>& iccardsCache,
              PublishService& publish,
              void* ctx,
              SyncFn syncCache,
              UnlockFn onUnlock);

  void begin();
  void loop();

private:
  String getUID_();

  AppState& appState_;
  CardRepository& cardRepo_;
  std::vector<String>& iccardsCache_;
  PublishService& publish_;

  void* ctx_{nullptr};
  SyncFn syncCache_{nullptr};
  UnlockFn onUnlock_{nullptr};

  MFRC522 mfrc522_;
};