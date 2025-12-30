#include "CardRepository.h"
#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

bool CardRepository::load()
{
    _cards.clear();
    if (!FileSystem::exists(PATH))
        return true;

    String json = FileSystem::readFile(PATH);
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(json, doc))
        return false;

    for (JsonVariant v : doc["cards"].as<JsonArray>())
    {
        _cards.push_back(v.as<String>());
    }
    return true;
}

bool CardRepository::save()
{
    DynamicJsonDocument doc(512);
    JsonArray arr = doc.createNestedArray("cards");
    for (auto &c : _cards)
        arr.add(c);
    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool CardRepository::exists(const String &uid) const
{
    for (auto &c : _cards)
        if (c == uid)
            return true;
    return false;
}

bool CardRepository::add(const String &uid)
{
    if (exists(uid))
        return false;
    _cards.push_back(uid);
    return save();
}

bool CardRepository::remove(const String &uid)
{
    for (auto it = _cards.begin(); it != _cards.end(); ++it)
    {
        if (*it == uid)
        {
            _cards.erase(it);
            return save();
        }
    }
    return false;
}
