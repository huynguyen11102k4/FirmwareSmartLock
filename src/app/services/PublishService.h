#pragma once
#include "models/AppState.h"
#include "storage/CardRepository.h"
#include "storage/PasscodeRepository.h"

#include <Arduino.h>

class PublishService
{
  public:
    PublishService(AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo);

    void
    publishState(const String& state, const String& reason = "");

    void
    publishLog(const String& ev, const String& method, const String& detail = "");

    void
    publishBattery(int percent);

    void
    publishPasscodeList();

    void
    publishICCardList();

    void
    publishInfo(int batteryPercent, int version);

  private:
    AppState& appState_;
    PasscodeRepository& passRepo_;
    CardRepository& cardRepo_;
};