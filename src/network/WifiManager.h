#pragma once
#include "config/AppConfig.h"

#include <WiFi.h>

class WifiManager
{
  public:
    static void
    begin(const AppConfig& cfg);
    static void
    loop();

    static bool
    connected();
    static String
    ip();

  private:
    static bool configured;
};
