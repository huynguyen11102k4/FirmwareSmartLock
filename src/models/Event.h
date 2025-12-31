#pragma once
#include <ArduinoJson.h>

enum class EventType : uint8_t
{
    AUTH_REQUEST,
    AUTH_RESULT,
    COMMAND_RECEIVED,
    DOOR_OPENED,
    DOOR_CLOSED,
    NETWORK_CHANGED
};

struct Event
{
    EventType type;
    void* data;
};