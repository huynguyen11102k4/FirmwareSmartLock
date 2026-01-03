#include "utils/WatchdogManager.h"

bool WatchdogManager::enabled_ = false;
uint32_t WatchdogManager::lastFeed_ = 0;

void
WatchdogManager::begin(uint32_t timeoutSeconds)
{
    esp_task_wdt_init(timeoutSeconds, true);
    esp_task_wdt_add(NULL);
    lastFeed_ = millis();
    enabled_ = true;
}

void
WatchdogManager::feed()
{
    if (!enabled_)
        return;

    esp_task_wdt_reset();
    lastFeed_ = millis();
}

uint32_t
WatchdogManager::timeSinceLastFeed()
{
    return (uint32_t)(millis() - lastFeed_);
}

void
WatchdogManager::disable()
{
    if (enabled_)
    {
        esp_task_wdt_delete(NULL);
        esp_task_wdt_deinit();
        enabled_ = false;
    }
}
