#pragma once
#include "app/services/PublishService.h"
#include "hardware/DoorContactModule.h"
#include "hardware/DoorLockModule.h"
#include "hardware/IHardwareModule.h"
#include "models/AppState.h"

#include <Arduino.h>
#include <Servo.h>

class DoorHardware final : public IHardwareModule
{
  public:
    DoorHardware(
        Servo& servo, uint8_t ledPin, uint8_t servoPin, uint8_t contactPin, bool contactActiveLow,
        uint32_t contactDebounceMs, bool contactUsePullup = true
    );

    void
    begin(AppContext& ctx) override;

    void
    loop(AppContext& ctx) override;

    void
    requestUnlock(const String& method);

    void
    requestLock(const String& reason);

    bool
    isDoorOpen() const;

  private:
    void
    onDoorContactChanged_(bool isOpen);

    AppContext* ctx_{nullptr};

    DoorLockModule lock_;
    DoorContactModule contact_;
};