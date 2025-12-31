#pragma once
#include "app/services/PublishService.h"
#include "models/AppState.h"
#include "storage/CardRepository.h"
#include "storage/PasscodeRepository.h"

#include <Arduino.h>
#include <vector>

class MqttService
{
  public:
    using SyncFn = void (*)(void* ctx);
    using UnlockFn = void (*)(void* ctx, const String& method);
    using LockFn = void (*)(void* ctx, const String& reason);
    using BatteryFn = int (*)(void* ctx);

    MqttService(
        AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo,
        std::vector<String>& iccardsCache, PublishService& publish, void* ctx,
        SyncFn syncIccardsCache, BatteryFn readBatteryPercent, UnlockFn onUnlock, LockFn onLock
    );

    void
    attachCallback();
    void
    onConnected(int infoVersion);

  private:
    static void
    callbackThunk(char* topic, byte* payload, unsigned int length);
    void
    dispatch_(const String& topicStr, const String& payloadStr);

    void
    handlePasscodesTopic_(const String& payloadStr);
    void
    handleIccardsTopic_(const String& payloadStr);
    void
    handleControlTopic_(const String& payloadStr);

    AppState& appState_;
    PasscodeRepository& passRepo_;
    CardRepository& cardRepo_;
    std::vector<String>& iccardsCache_;
    PublishService& publish_;

    void* ctx_{nullptr};
    SyncFn syncIccardsCache_{nullptr};
    BatteryFn readBatteryPercent_{nullptr};
    UnlockFn onUnlock_{nullptr};
    LockFn onLock_{nullptr};

    static MqttService* s_instance_;
};
