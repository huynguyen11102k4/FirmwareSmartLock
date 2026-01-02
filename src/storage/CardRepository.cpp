#include "storage/CardRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>
namespace
{
constexpr size_t kMinCap = 1024;
constexpr size_t kMaxCap = 32768;

bool
isUidValid(const String& uid)
{
    return uid.length() > 0;
}
} // namespace

size_t
CardRepository::calcDocCapacity(const String& json)
{
    const size_t len = static_cast<size_t>(json.length());
    size_t cap = static_cast<size_t>(len * 13 / 10) + 1024;
    if (cap < kMinCap)
        cap = kMinCap;
    if (cap > kMaxCap)
        cap = kMaxCap;
    return cap;
}

bool
CardRepository::load()
{
    cards_.clear();
    ts_ = 0;

    if (!FileSystem::exists(PATH))
        return true;

    const String json = FileSystem::readFile(PATH);
    DynamicJsonDocument doc(calcDocCapacity(json));

    if (!JsonUtils::deserialize(json, doc))
        return false;

    if (doc.containsKey(AppJsonKeys::TS))
        ts_ = doc[AppJsonKeys::TS] | 0;

    if (doc.containsKey(AppJsonKeys::CARDS) && doc[AppJsonKeys::CARDS].is<JsonArray>())
    {
        for (JsonVariant v : doc[AppJsonKeys::CARDS].as<JsonArray>())
        {
            if (!v.is<JsonObject>())
                continue;

            CardItem item = CardItem::fromJson(v);
            item.uid.trim();
            if (!isUidValid(item.uid))
                continue;

            cards_.push_back(item);
        }
    }

    return true;
}

bool
CardRepository::save()
{
    return saveInternal();
}

bool
CardRepository::saveInternal()
{
    const size_t est = kMinCap + cards_.size() * 64;
    DynamicJsonDocument doc(est > kMaxCap ? kMaxCap : est);

    doc[AppJsonKeys::TS] = ts_;
    JsonArray cardsArray = doc.createNestedArray(AppJsonKeys::CARDS);

    for (const auto& c : cards_)
    {
        JsonObject o = cardsArray.createNestedObject();
        c.toJson(o);
    }

    return FileSystem::writeFileAtomic(PATH, JsonUtils::serialize(doc));
}

bool
CardRepository::exists(const String& uid) const
{
    for (const auto& c : cards_)
    {
        if (c.uid == uid)
            return true;
    }
    return false;
}

bool
CardRepository::add(const String& uid)
{
    return add(uid, "");
}

bool
CardRepository::add(const String& uid, const String& name)
{
    String clean = uid;
    clean.trim();
    if (!isUidValid(clean))
        return false;

    if (exists(clean))
        return false;

    cards_.push_back(CardItem{clean, name});
    return saveInternal();
}

bool
CardRepository::updateName(const String& uid, const String& name)
{
    for (auto& c : cards_)
    {
        if (c.uid == uid)
        {
            c.name = name;
            return saveInternal();
        }
    }
    return false;
}

bool
CardRepository::remove(const String& uid)
{
    for (auto it = cards_.begin(); it != cards_.end(); ++it)
    {
        if (it->uid == uid)
        {
            cards_.erase(it);
            return saveInternal();
        }
    }
    return false;
}

const std::vector<CardItem>&
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

long
CardRepository::ts() const
{
    return ts_;
}

void
CardRepository::setTs(long ts)
{
    ts_ = ts;
}
