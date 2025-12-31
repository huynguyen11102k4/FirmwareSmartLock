#pragma once
#include "DeviceState.h"
#include "DoorLockState.h"
#include "PinAuthState.h"
#include "RuntimeFlags.h"
#include "SwipeAddState.h"

#include <Arduino.h>

struct AppState
{
    // MQTT / device identity
    String macAddress = "";
    String mqttTopicPrefix = "";
    String defaultMqttTopicPrefix = "door/";

    // Component runtime states
    SwipeAddState swipeAdd;
    PinAuthState pinAuth;
    DoorLockState doorLock;
    RuntimeFlags runtimeFlags;
    DeviceState deviceState;

    // Periodic task timers
    uint32_t lastBatteryPublishMs = 0;
    uint32_t lastSyncMs = 0;

    static constexpr uint32_t BATTERY_PUBLISH_INTERVAL_MS = 60000; // 1 min
    static constexpr uint32_t SYNC_INTERVAL_MS = 300000;           // 5 min

    void
    init(const String& mac)
    {
        macAddress = mac;

        // Generate default topic from MAC
        String mac2 = mac;
        mac2.replace(":", "");
        mac2.toLowerCase();
        defaultMqttTopicPrefix = "door/" + mac2.substring(0, 6);

        mqttTopicPrefix = defaultMqttTopicPrefix;

        // Initialize component states
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();

        lastBatteryPublishMs = 0;
        lastSyncMs = 0;
    }

    void
    setMqttTopicPrefix(const String& topic)
    {
        if (topic.length() > 0)
        {
            mqttTopicPrefix = topic;
        }
        else
        {
            mqttTopicPrefix = defaultMqttTopicPrefix;
        }
    }

    bool
    shouldPublishBattery() const
    {
        return (millis() - lastBatteryPublishMs) >= BATTERY_PUBLISH_INTERVAL_MS;
    }

    void
    markBatteryPublished()
    {
        lastBatteryPublishMs = millis();
    }

    bool
    shouldSync() const
    {
        return (millis() - lastSyncMs) >= SYNC_INTERVAL_MS;
    }

    void
    markSynced()
    {
        lastSyncMs = millis();
    }

    void
    reset()
    {
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();
        lastBatteryPublishMs = 0;
        lastSyncMs = 0;
    }
};