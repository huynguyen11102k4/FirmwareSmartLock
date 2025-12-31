#pragma once
#include <Arduino.h>

enum class CommandType : uint8_t
{
    NONE,
    UNLOCK,
    LOCK,
    ADD_CARD,
    REMOVE_CARD,
    SET_PASSCODE,
    SET_TEMP_PASSCODE,
    OTA
};

struct Command
{
    CommandType type = CommandType::NONE;
    String source;
    String payload; // JSON string

    bool
    isValid() const
    {
        return type != CommandType::NONE;
    }

    static Command
    none()
    {
        return {};
    }
};
