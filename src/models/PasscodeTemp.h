#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct Passcode
{
    String   code;
    String   type;         // "one_time" | "timed"
    uint64_t effectiveAt;  // unix seconds
    uint64_t expireAt;     // 0 = không hết hạn

    bool
    isEffective(uint64_t now) const
    {
        return effectiveAt == 0 || now >= effectiveAt;
    }

    bool
    isExpired(uint64_t now) const
    {
        return expireAt > 0 && now >= expireAt;
    }

    bool
    isValid(uint64_t now) const
    {
        return isEffective(now) && !isExpired(now);
    }

    void
    toJson(JsonObject obj) const
    {
        obj["code"]        = code;
        obj["type"]        = type;
        obj["effectiveAt"] = effectiveAt;
        obj["expireAt"]    = expireAt;
    }

    static Passcode
    fromJson(const JsonObjectConst& obj)
    {
        Passcode p;
        p.code        = obj["code"] | "";
        p.type        = obj["type"] | "";
        p.effectiveAt = (uint64_t)(obj["effectiveAt"] | 0);
        p.expireAt    = (uint64_t)(obj["expireAt"] | 0);
        return p;
    }
};
