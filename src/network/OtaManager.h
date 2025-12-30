#pragma once
#include <ArduinoOTA.h>

class OtaManager {
public:
  static void begin(const String& hostname);
  static void loop();

private:
  static bool started;
};
