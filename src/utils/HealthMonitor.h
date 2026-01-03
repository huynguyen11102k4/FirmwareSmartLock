#pragma once
#include "utils/Logger.h"

#include <Arduino.h>
#include <esp_system.h>

struct SystemHealth
{
    size_t freeHeap;
    size_t minFreeHeap;
    size_t heapSize;
    uint32_t uptime;
    float cpuFreqMHz;
    uint8_t cpuTemp;
};

class HealthMonitor
{
  public:
    static SystemHealth
    getHealth()
    {
        SystemHealth health;

        health.freeHeap = ESP.getFreeHeap();
        health.minFreeHeap = ESP.getMinFreeHeap();
        health.heapSize = ESP.getHeapSize();
        health.uptime = millis() / 1000;
        health.cpuFreqMHz = ESP.getCpuFreqMHz();

#ifdef SOC_TEMP_SENSOR_SUPPORTED
        health.cpuTemp = temperatureRead();
#else
        health.cpuTemp = 0;
#endif

        return health;
    }

    static void
    logHealth()
    {
        const SystemHealth h = getHealth();
        Logger::info(
            "HEALTH", "Heap: %d/%d (min: %d), Uptime: %ds, CPU: %.0fMHz", (int)h.freeHeap,
            (int)h.heapSize, (int)h.minFreeHeap, (int)h.uptime, h.cpuFreqMHz
        );

        if (h.cpuTemp > 0)
        {
            Logger::info("HEALTH", "CPU Temp: %dC", (int)h.cpuTemp);
        }
    }

    static bool
    isHealthy()
    {
        const SystemHealth h = getHealth();

        if (h.freeHeap < 15000)
        {
            Logger::warn("HEALTH", "Low heap: %d bytes", (int)h.freeHeap);
            return false;
        }

        if (h.cpuTemp > 80)
        {
            Logger::warn("HEALTH", "High temperature: %dC", (int)h.cpuTemp);
            return false;
        }

        return true;
    }
};