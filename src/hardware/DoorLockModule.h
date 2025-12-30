#pragma once
#include <Arduino.h>
#include <Servo.h>
#include "hardware/IHardwareModule.h"

class DoorLockModule final : public IHardwareModule {
public:
  DoorLockModule(Servo& servo, uint8_t ledPin, uint8_t servoPin);

  void beginUnlock(AppContext& ctx, const String& method);
  void forceLock(AppContext& ctx, const String& reason);

  void setDoorOpen(bool open);

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;

private:
  void serviceAutoRelock_(AppContext& ctx);

  Servo& servo_;
  uint8_t ledPin_;
  uint8_t servoPin_;
  bool doorOpen_{false};
};