#pragma once

#include "app/AppContext.h"

#include <Arduino.h>
#include <functional>

struct AppContext;

class DoorContactModule 
{
  public:
    using DoorContactCallback = std::function<void(bool isOpen)>;

    DoorContactModule(uint8_t pin, bool activeLow, uint32_t debounceMs, bool usePullup = true);

    void
    setCallback(DoorContactCallback cb);

    bool
    isOpen() const;

    void
    begin(AppContext& ctx);

    void
    loop(AppContext& ctx);

  private:
    bool
    readOpenRaw_() const;

    uint8_t pin_;
    bool activeLow_;
    uint32_t debounceMs_;
    bool usePullup_;

    bool isOpen_{false};
    bool lastRawOpen_{false};
    uint32_t lastChangeMs_{0};

    DoorContactCallback cb_{};
};
