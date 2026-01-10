#pragma once
#include "models/DeviceState.h"
#include "models/DoorLockState.h"
#include "models/PinAuthState.h"
#include "models/RuntimeFlags.h"
#include "models/SwipeAddState.h"
#include "models/WifiProvisionState.h"

#include <Arduino.h>

struct AppState
{
    String macAddress = "";
    String mqttTopicPrefix = "";
    String defaultMqttTopicPrefix = "";

    String doorCode = "";
    String doorName = "";

    SwipeAddState swipeAdd;
    PinAuthState pinAuth;
    DoorLockState doorLock;
    RuntimeFlags runtimeFlags;
    DeviceState deviceState;
    WifiProvisionState wifiProvision;

    void init(const String& mac)
    {
        macAddress = mac;

        // Inline DeviceIdentity logic
        String m = mac;
        m.replace(":", "");
        m.toLowerCase();
        defaultMqttTopicPrefix = m;
        mqttTopicPrefix = defaultMqttTopicPrefix;

        // Door code: random 6 digits
        uint32_t r = esp_random();
        uint32_t code = r % 1000000;
        char buf[7];
        snprintf(buf, sizeof(buf), "%06lu", code);
        doorCode = String(buf);

        // Door name: Door-{last 4 chars of MAC}
        m.toUpperCase();
        const String tail = (m.length() >= 4) ? m.substring(m.length() - 4) : m;
        doorName = "Door-" + tail;

        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();
        wifiProvision.reset();  // ADD THIS
    }

    void setMqttTopicPrefix(const String& topic)
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

    void reset()
    {
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();
        wifiProvision.reset();
    }
};