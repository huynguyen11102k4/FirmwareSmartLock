#pragma once
#include <Arduino.h>
#include <WebServer.h>

#include "models/AppState.h"

class HttpService {
public:
  using BatteryFn = int (*)(void* ctx);

  HttpService(AppState& appState, void* ctx, BatteryFn readBatteryPercent);

  void begin();
  void loop();

private:
  void handleInfo_();
  void handleNotFound_();

  AppState& appState_;
  void* ctx_{nullptr};
  BatteryFn readBatteryPercent_{nullptr};

  WebServer server_;
};
