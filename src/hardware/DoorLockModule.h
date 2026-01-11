#pragma once

#include <Arduino.h>
#include <Servo.h>

struct AppContext;

class DoorLockModule
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
    begin(AppContext& ctx);

    void
    loop(AppContext& ctx);

  private:
    uint32_t autoRelockAtMs_ = 0;
    bool autoRelockArmed_ = false;

    void
    handleAutoRelock_(AppContext& ctx);

    Servo& servo_;
    uint8_t ledPin_;
    uint8_t servoPin_;
    bool isDoorContactOpen_{false};

    bool ledBlinking_ = false;
    unsigned long ledBlinkIntervalMs_ = 1000;
    unsigned long ledLastToggleMs_ = 0;
    bool ledState_ = false;

};