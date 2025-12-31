#include "storage/PasscodeRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>

bool
PasscodeRepository::load()
{
    hasTemp_ = false;

    if (!FileSystem::exists(PATH))
        return true;

    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(FileSystem::readFile(PATH), doc))
        return false;

    master_ = doc["master"] | "";

    if (doc.containsKey("temp") && doc["temp"].is<JsonObject>())
    {
        temp_ = PasscodeTemp::fromJson(doc["temp"]);
        hasTemp_ = true;
    }

    return true;
}

String
PasscodeRepository::getMaster() const
{
    return master_;
}

bool
PasscodeRepository::setMaster(const String& pass)
{
    master_ = pass;

    DynamicJsonDocument doc(512);
    doc["master"] = master_;

    if (hasTemp_)
    {
        JsonObject t = doc.createNestedObject("temp");
        temp_.toJson(t);
    }

    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool
PasscodeRepository::hasTemp() const
{
    return hasTemp_;
}

PasscodeTemp
PasscodeRepository::getTemp() const
{
    return temp_;
}

bool
PasscodeRepository::setTemp(const PasscodeTemp& temp)
{
    temp_ = temp;
    hasTemp_ = true;

    DynamicJsonDocument doc(512);
    doc["master"] = master_;

    JsonObject t = doc.createNestedObject("temp");
    temp_.toJson(t);

    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool
PasscodeRepository::clearTemp()
{
    hasTemp_ = false;

    DynamicJsonDocument doc(512);
    doc["master"] = master_;

    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}