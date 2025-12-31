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
    return config.isValid();
}

const AppConfig&
ConfigManager::get() const
{
    return config;
}

bool
ConfigManager::updateFromBle(const AppConfig& cfg)
{
    if (!cfg.isValid())
        return false;

    config = cfg;
    return repo.save(config);
}
