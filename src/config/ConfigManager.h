#pragma once
#include "config/AppConfig.h"
#include "config/ConfigRepository.h"

class ConfigManager
{
  public:
    bool
    load();

    void
    save();

    bool
    isProvisioned() const;

    const AppConfig&
    get() const;

    bool
    updateFromBle(const AppConfig& cfg);

  private:
    AppConfig config;
    ConfigRepository repo;
};