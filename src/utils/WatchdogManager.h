#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>

class WatchdogManager
{
  public:
    static void
    begin(uint32_t timeoutSeconds = 30)
    {
        esp_task_wdt_init(timeoutSeconds, true);
        esp_task_wdt_add(NULL);
        lastFeed_ = millis();
        enabled_ = true;
    }

    static void
    feed()
    {
        if (!enabled_)
            return;
        esp_task_wdt_reset();
        lastFeed_ = millis();
    }

    static uint32_t
    timeSinceLastFeed()
    {
        return millis() - lastFeed_;
    }

    static void
    disable()
    {
        if (enabled_)
        {
            esp_task_wdt_delete(NULL);
            esp_task_wdt_deinit();
            enabled_ = false;
        }
    }

  private:
    static bool enabled_;
    static uint32_t lastFeed_;
};

bool WatchdogManager::enabled_ = false;
uint32_t WatchdogManager::lastFeed_ = 0;