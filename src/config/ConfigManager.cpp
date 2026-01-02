#include "config/ConfigManager.h"

bool
ConfigManager::load()
{
    config.clear();
    return repo.load(config);
}

void
ConfigManager::save()
{
    repo.save(config);
}

bool
ConfigManager::isProvisioned() const
{
    return config.isProvisionedWifiOnly();
}

const AppConfig&
ConfigManager::get() const
{
    return config;
}

bool
ConfigManager::updateFromBle(const AppConfig& cfg)
{
    if (!cfg.hasWifi())
        return false;

    config.wifiSsid = cfg.wifiSsid;
    config.wifiPass = cfg.wifiPass;

    return repo.save(config);
}
