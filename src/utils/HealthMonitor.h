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

// Temperature (if available)
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
        SystemHealth h = getHealth();

        Logger::info(
            "HEALTH", "Heap: %d/%d (min: %d), Uptime: %ds, CPU: %.0fMHz", h.freeHeap, h.heapSize,
            h.minFreeHeap, h.uptime, h.cpuFreqMHz
        );

        if (h.cpuTemp > 0)
        {
            Logger::info("HEALTH", "CPU Temp: %d°C", h.cpuTemp);
        }
    }

    static bool
    isHealthy()
    {
        SystemHealth h = getHealth();

        // Check heap
        if (h.freeHeap < 15000) // Less than 15KB
        {
            Logger::warn("HEALTH", "Low heap: %d bytes", h.freeHeap);
            return false;
        }

        // Check temperature (if available)
        if (h.cpuTemp > 80) // Over 80°C
        {
            Logger::warn("HEALTH", "High temperature: %d°C", h.cpuTemp);
            return false;
        }

        return true;
    }

    static String
    getHealthJson()
    {
        SystemHealth h = getHealth();

        String json = "{";
        json += "\"freeHeap\":" + String(h.freeHeap) + ",";
        json += "\"minFreeHeap\":" + String(h.minFreeHeap) + ",";
        json += "\"heapSize\":" + String(h.heapSize) + ",";
        json += "\"uptime\":" + String(h.uptime) + ",";
        json += "\"cpuFreqMHz\":" + String(h.cpuFreqMHz) + ",";
        json += "\"cpuTemp\":" + String(h.cpuTemp);
        json += "}";

        return json;
    }

    static void
    performGarbageCollection()
    {
        // Force heap cleanup if needed
        if (ESP.getFreeHeap() < 20000)
        {
            Logger::info("HEALTH", "Performing garbage collection...");
            // Can add custom cleanup here
            yield();
        }
    }
};