#pragma once
#include "config/AppConfig.h"

class NetworkManager
{
  public:
    static void
    begin(const AppConfig& cfg, const String& clientId);

    static void
    loop();

    static bool
    online();

};
