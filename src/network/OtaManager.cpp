#include "OtaManager.h"
#include "utils/Logger.h"

bool OtaManager::started = false;

void OtaManager::begin(const String& hostname) {
  if (started) return;
  started = true;

  ArduinoOTA.setHostname(hostname.c_str());

  ArduinoOTA
    .onStart([]() {
      Logger::info("OTA", "Start");
    })
    .onEnd([]() {
      Logger::info("OTA", "End");
    })
    .onError([](ota_error_t err) {
      Logger::error("OTA", "Error %d", err);
    });

  ArduinoOTA.begin();
  Logger::info("OTA", "Ready");
}

void OtaManager::loop() {
  if (started) {
    ArduinoOTA.handle();
  }
}
