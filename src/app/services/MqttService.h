#pragma once
#include "app/services/PublishService.h"
#include "config/LockConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "storage/CardRepository.h"
#include "storage/PasscodeRepository.h"

#include <Arduino.h>

class MqttService
{
  public:
    MqttService(
        AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo,
        PublishService& publish, const LockConfig& lockConfig, DoorHardware& door
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
    PublishService& publish_;
    const LockConfig& lockConfig_;
    DoorHardware& door_;

    static MqttService* s_instance_;
};
