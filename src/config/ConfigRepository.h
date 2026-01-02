#pragma once
#include "config/AppConfig.h"
#include "config/MqttDefaults.h"

class ConfigRepository
{
  public:
    bool
    load(AppConfig& cfg);

    bool
    save(const AppConfig& cfg);

    bool
    exists() const;
};
