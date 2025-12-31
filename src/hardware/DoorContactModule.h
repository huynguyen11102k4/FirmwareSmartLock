#pragma once
#include "hardware/IHardwareModule.h"

#include <Arduino.h>

using DoorContactCallback = void (*)(bool isOpen);

class DoorContactModule final : public IHardwareModule
{
  public:
    DoorContactModule(uint8_t pin, bool activeLow, uint32_t debounceMs);

    void
    setCallback(DoorContactCallback cb);

    bool
    isOpen() const;
    void
    begin(AppContext& ctx) override;
    void
    loop(AppContext& ctx) override;

  private:
    bool
    readRaw_() const;

    uint8_t pin_;
    bool activeLow_;
    uint32_t debounceMs_;

    bool isOpen_{false};
    bool lastRawOpen_{false};
    uint32_t lastChangeMs_{0};

    DoorContactCallback cb_{nullptr};
};
