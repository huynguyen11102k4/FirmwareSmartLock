#pragma once
#include "app/services/PublishService.h"
#include "models/AppState.h"
#include "storage/PasscodeRepository.h"

#include <Arduino.h>
#include <Keypad.h>

class KeypadService
{
  public:
    using RequestUnlockFn = void (*)(void* ctx, const String& method);

    KeypadService(
        AppState& appState, PasscodeRepository& passRepo, PublishService& publish, void* ctx,
        RequestUnlockFn requestUnlockFn
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

    void* ctx_{nullptr};
    RequestUnlockFn onUnlock_{nullptr};

    Keypad keypad_;
};
