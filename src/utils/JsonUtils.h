#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class JsonUtils
{
  public:
    static bool
    deserialize(const String& json, JsonDocument& doc);

    static String
    serialize(const JsonDocument& doc);

    template <typename T>
    static T
    get(JsonDocument& doc, const char* key, T defaultValue)
    {
        if (!doc.containsKey(key))
            return defaultValue;
        return doc[key].as<T>();
    }
};
