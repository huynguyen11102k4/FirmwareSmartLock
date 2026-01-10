#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>

class WatchdogManager
{
  public:
    static void begin(uint32_t timeoutSeconds = 30)
    {
        esp_task_wdt_init(timeoutSeconds, true);
        esp_task_wdt_add(NULL);
    }

    static void feed()
    {
        esp_task_wdt_reset();
    }

    static void disable()
    {
        esp_task_wdt_delete(NULL);
        esp_task_wdt_deinit();
    }
};