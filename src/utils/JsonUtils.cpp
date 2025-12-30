#include "JsonUtils.h"

bool JsonUtils::deserialize(const String &json, JsonDocument &doc)
{
    DeserializationError err = deserializeJson(doc, json);
    return !err;
}

String JsonUtils::serialize(const JsonDocument &doc)
{
    String out;
    serializeJson(doc, out);
    return out;
}
