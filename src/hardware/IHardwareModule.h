#pragma once
#include <Arduino.h>

struct AppContext;

class IHardwareModule {
public:
  virtual ~IHardwareModule() = default;
  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
};