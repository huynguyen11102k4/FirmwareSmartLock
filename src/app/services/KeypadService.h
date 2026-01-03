#pragma once
#include "app/services/PublishService.h"
#include "config/LockConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "storage/PasscodeRepository.h"

#include <Arduino.h>
#include <Keypad.h>

class KeypadService
{
  public:
    KeypadService(
        AppState& appState, PasscodeRepository& passRepo, PublishService& publish,
        DoorHardware& door, const LockConfig& lockConfig
    );

    void
    begin();

    void
    loop();

  private:
    bool
    checkPIN_(const String& pin);

    AppState& appState_;
    PasscodeRepository& passRepo_;
    PublishService& publish_;
    DoorHardware& door_;
    const LockConfig& lockConfig_;

    Keypad keypad_;
};
