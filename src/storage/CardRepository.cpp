#include "storage/CardRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>

bool
CardRepository::load()
{
    cards_.clear();

    if (!FileSystem::exists(PATH))
        return true;

    const String json = FileSystem::readFile(PATH);
    DynamicJsonDocument doc(512);

    if (!JsonUtils::deserialize(json, doc))
        return false;

    if (doc.containsKey(AppJsonKeys::CARDS) && doc[AppJsonKeys::CARDS].is<JsonArray>())
    {
        for (JsonVariant v : doc[AppJsonKeys::CARDS].as<JsonArray>())
        {
            cards_.push_back(v.as<String>());
        }
    }

    return true;
}

bool
CardRepository::save()
{
    DynamicJsonDocument doc(512);
    JsonArray arr = doc.createNestedArray(AppJsonKeys::CARDS);

    for (const auto& c : cards_)
        arr.add(c);

    return FileSystem::writeFile(PATH, JsonUtils::serialize(doc));
}

bool
CardRepository::exists(const String& uid) const
{
    for (const auto& c : cards_)
    {
        if (c == uid)
            return true;
    }
    return false;
}

bool
CardRepository::add(const String& uid)
{
    if (exists(uid))
        return false;

    cards_.push_back(uid);
    return save();
}

bool
CardRepository::remove(const String& uid)
{
    for (auto it = cards_.begin(); it != cards_.end(); ++it)
    {
        if (*it == uid)
        {
            cards_.erase(it);
            return save();
        }
    }
    return false;
}

const std::vector<String>&
CardRepository::list() const
{
    return cards_;
}

bool
CardRepository::isEmpty() const
{
    return cards_.empty();
}

size_t
CardRepository::size() const
{
    return cards_.size();
}
