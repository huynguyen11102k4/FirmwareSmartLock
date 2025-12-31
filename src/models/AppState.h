#pragma once
#include "models/DeviceState.h"
#include "models/DoorLockState.h"
#include "models/PinAuthState.h"
#include "models/RuntimeFlags.h"
#include "models/SwipeAddState.h"

#include <Arduino.h>

struct AppState
{
    String macAddress = "";
    String mqttTopicPrefix = "";
    String defaultMqttTopicPrefix = "door";

    SwipeAddState swipeAdd;
    PinAuthState pinAuth;
    DoorLockState doorLock;
    RuntimeFlags runtimeFlags;
    DeviceState deviceState;

    void
    init(const String& mac)
    {
        macAddress = mac;

        String mac2 = mac;
        mac2.replace(":", "");
        mac2.toLowerCase();

        defaultMqttTopicPrefix = "door/" + mac2;
        mqttTopicPrefix = defaultMqttTopicPrefix;

        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();
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

    void
    reset()
    {
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        runtimeFlags.reset();
    }
};
