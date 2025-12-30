#pragma once
#include <Arduino.h>
#include <vector>

#include "models/AppState.h"
#include "storage/PasscodeRepository.h"

class PublishService
{
public:
    PublishService(AppState &appState,
                   PasscodeRepository &passRepo,
                   std::vector<String> &iccardsCache);

    void publishState(const String &state, const String &reason = "");
    void publishLog(const String &ev, const String &method, const String &detail = "");
    void publishBattery(int percent);

    void publishPasscodeList();
    void publishICCardList();

    void publishInfo(int batteryPercent, int version);

private:
    AppState &appState_;
    PasscodeRepository &passRepo_;
    std::vector<String> &iccardsCache_;
};