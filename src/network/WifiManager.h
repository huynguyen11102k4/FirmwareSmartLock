#pragma once
#include <WiFi.h>
#include "config/AppConfig.h"

class WifiManager {
public:
  static void begin(const AppConfig& cfg);
  static void loop();

  static bool connected();
  static String ip();

private:
  static bool configured;
};
