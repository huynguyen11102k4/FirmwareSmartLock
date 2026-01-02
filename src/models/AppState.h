#pragma once
#include "models/DeviceState.h"
#include "models/DoorLockState.h"
#include "models/PinAuthState.h"
#include "models/RuntimeFlags.h"
#include "models/SwipeAddState.h"
#include "utils/DeviceIdentity.h"

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

    void
    init(const String& mac)
    {
        macAddress = mac;

        defaultMqttTopicPrefix = DeviceIdentity::makeTopicPrefix(mac);
        mqttTopicPrefix = defaultMqttTopicPrefix;

        doorCode = DeviceIdentity::makeDoorCode(mac);
        doorName = DeviceIdentity::makeDefaultName(mac);

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
