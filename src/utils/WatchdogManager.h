#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>

class WatchdogManager
{
  public:
    static void
    begin(uint32_t timeoutSeconds = 30);

    static void
    feed();

    static uint32_t
    timeSinceLastFeed();

    static void
    disable();

  private:
    static bool enabled_;
    static uint32_t lastFeed_;
};