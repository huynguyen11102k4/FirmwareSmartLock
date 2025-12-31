#pragma once
#include <ArduinoOTA.h>

class OtaManager
{
  public:
    static void
    begin(const String& hostname);

    static void
    loop();

    static bool
    isUpdateInProgress();

    static void
    verifyFirmware();

  private:
    static bool started;
};
