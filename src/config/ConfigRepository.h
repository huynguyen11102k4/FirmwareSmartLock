#pragma once
#include "AppConfig.h"

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
