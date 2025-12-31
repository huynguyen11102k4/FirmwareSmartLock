#include "PasscodeRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

bool
PasscodeRepository::load()
{
    if (!FileSystem::exists(PATH))
        return true;

    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(FileSystem::readFile(PATH), doc))
        return false;

    _master = doc["master"] | "";

    if (doc.containsKey("temp"))
    {
        _temp = PasscodeTemp::fromJson(doc["temp"]);
        _hasTemp = true;
    }
    return true;
}

String
PasscodeRepository::getMaster() const
{
    return _master;
}

bool
PasscodeRepository::setMaster(const String& pass)
{
    _master = pass;

    DynamicJsonDocument doc(512);
    doc["master"] = _master;
    if (_hasTemp)
    {
        JsonObject t = doc.createNestedObject("temp");
        _temp.toJson(t);
    }
    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool
PasscodeRepository::hasTemp() const
{
    return _hasTemp;
}

PasscodeTemp
PasscodeRepository::getTemp() const
{
    return _temp;
}

bool
PasscodeRepository::setTemp(const PasscodeTemp& temp)
{
    _temp = temp;
    _hasTemp = true;

    DynamicJsonDocument doc(512);
    doc["master"] = _master;
    JsonObject t = doc.createNestedObject("temp");
    _temp.toJson(t);
    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool
PasscodeRepository::clearTemp()
{
    _hasTemp = false;

    DynamicJsonDocument doc(512);
    doc["master"] = _master;
    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}
