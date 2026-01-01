#pragma once

#include "app/services/PublishService.h"
#include "hardware/IHardwareModule.h"
#include "models/AppState.h"

#include <Arduino.h>
#include <Servo.h>

class DoorLockModule final : public IHardwareModule
{
  public:
    DoorLockModule(Servo& servo, uint8_t ledPin, uint8_t servoPin);

    void
    unlock(AppContext& ctx, const String& method);

    void
    lock(AppContext& ctx, const String& reason);

    void
    onDoorContactChanged(bool isOpen);

    void
    begin(AppContext& ctx) override;

    void
    loop(AppContext& ctx) override;

  private:
    void
    handleAutoRelock_(AppContext& ctx);

    Servo& servo_;
    uint8_t ledPin_;
    uint8_t servoPin_;
    bool isDoorContactOpen_{false};
};