#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct PasscodeTemp
{
    String code;
    uint64_t expireAt;

    bool
    isExpired(uint64_t now) const
    {
        return now >= expireAt;
    }

    void
    toJson(JsonObject obj) const
    {
        obj["code"] = code;
        obj["expireAt"] = expireAt;
    }

    static PasscodeTemp
    fromJson(const JsonObject& obj)
    {
        PasscodeTemp p;
        p.code = obj["code"] | "";
        p.expireAt = obj["expireAt"] | 0;
        return p;
    }
};