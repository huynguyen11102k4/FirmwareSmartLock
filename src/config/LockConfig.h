#pragma once
#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <Arduino.h>

struct LockConfig
{
    uint32_t unlockDurationMs = 5000;
    uint32_t autoRelockDelayMs = 15000;

    int maxFailedAttempts = 5;
    uint32_t lockoutDurationMs = 30000;
    int minPinLength = 4;
    int maxPinLength = 10;

    uint32_t rfidDebounceMs = 2000;
    uint32_t swipeAddTimeoutMs = 60000;

    uint32_t batteryPublishIntervalMs = 60000;
    float batteryMinVoltage = 3.3f;
    float batteryMaxVoltage = 4.2f;

    uint32_t syncIntervalMs = 300000;

    uint32_t mqttReconnectBaseMs = 1000;
    uint32_t mqttReconnectMaxMs = 60000;
    uint32_t wifiReconnectDelayMs = 5000;

    static constexpr const char* CONFIG_PATH = "/lock_config.json";

    bool
    loadFromFile()
    {
        if (!FileSystem::exists(CONFIG_PATH))
            return false;

        const String json = FileSystem::readFile(CONFIG_PATH);
        DynamicJsonDocument doc(2048);

        if (!JsonUtils::deserialize(json, doc))
            return false;

        unlockDurationMs = doc["unlockDurationMs"] | unlockDurationMs;
        autoRelockDelayMs = doc["autoRelockDelayMs"] | autoRelockDelayMs;

        maxFailedAttempts = doc["maxFailedAttempts"] | maxFailedAttempts;
        lockoutDurationMs = doc["lockoutDurationMs"] | lockoutDurationMs;
        minPinLength = doc["minPinLength"] | minPinLength;
        maxPinLength = doc["maxPinLength"] | maxPinLength;

        rfidDebounceMs = doc["rfidDebounceMs"] | rfidDebounceMs;
        swipeAddTimeoutMs = doc["swipeAddTimeoutMs"] | swipeAddTimeoutMs;

        batteryPublishIntervalMs = doc["batteryPublishIntervalMs"] | batteryPublishIntervalMs;
        batteryMinVoltage = doc["batteryMinVoltage"] | batteryMinVoltage;
        batteryMaxVoltage = doc["batteryMaxVoltage"] | batteryMaxVoltage;

        syncIntervalMs = doc["syncIntervalMs"] | syncIntervalMs;

        mqttReconnectBaseMs = doc["mqttReconnectBaseMs"] | mqttReconnectBaseMs;
        mqttReconnectMaxMs = doc["mqttReconnectMaxMs"] | mqttReconnectMaxMs;
        wifiReconnectDelayMs = doc["wifiReconnectDelayMs"] | wifiReconnectDelayMs;

        return true;
    }

    bool
    saveToFile() const
    {
        DynamicJsonDocument doc(2048);

        doc["unlockDurationMs"] = unlockDurationMs;
        doc["autoRelockDelayMs"] = autoRelockDelayMs;

        doc["maxFailedAttempts"] = maxFailedAttempts;
        doc["lockoutDurationMs"] = lockoutDurationMs;
        doc["minPinLength"] = minPinLength;
        doc["maxPinLength"] = maxPinLength;

        doc["rfidDebounceMs"] = rfidDebounceMs;
        doc["swipeAddTimeoutMs"] = swipeAddTimeoutMs;

        doc["batteryPublishIntervalMs"] = batteryPublishIntervalMs;
        doc["batteryMinVoltage"] = batteryMinVoltage;
        doc["batteryMaxVoltage"] = batteryMaxVoltage;

        doc["syncIntervalMs"] = syncIntervalMs;

        doc["mqttReconnectBaseMs"] = mqttReconnectBaseMs;
        doc["mqttReconnectMaxMs"] = mqttReconnectMaxMs;
        doc["wifiReconnectDelayMs"] = wifiReconnectDelayMs;

        return FileSystem::writeFile(CONFIG_PATH, JsonUtils::serialize(doc));
    }

    void
    setDefaults()
    {
        *this = LockConfig();
    }
};
