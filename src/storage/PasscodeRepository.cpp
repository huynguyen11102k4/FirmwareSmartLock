#include "storage/PasscodeRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>

namespace
{
constexpr size_t kMinCap = 1024;
constexpr size_t kMaxCap = 32768;

bool
isCodeValid(const String& code)
{
    return code.length() > 0;
}

bool
isTypeValid(const String& type)
{
    return type == "permanent" || type == "one_time" || type == "timed";
}
} // namespace

size_t
PasscodeRepository::calcDocCapacity(const String& json)
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
PasscodeRepository::load()
{
    hasTemp_ = false;
    items_.clear();
    ts_ = 0;

    if (!FileSystem::exists(PATH))
        return true;

    const String json = FileSystem::readFile(PATH);
    DynamicJsonDocument doc(calcDocCapacity(json));
    if (!JsonUtils::deserialize(json, doc))
        return false;

    master_ = doc[AppJsonKeys::PASSCODES_MASTER] | "";

    if (doc.containsKey(AppJsonKeys::PASSCODES_TEMP) &&
        doc[AppJsonKeys::PASSCODES_TEMP].is<JsonObject>())
    {
        temp_ = PasscodeTemp::fromJson(doc[AppJsonKeys::PASSCODES_TEMP]);
        hasTemp_ = true;
    }

    if (doc.containsKey(AppJsonKeys::TS))
        ts_ = doc[AppJsonKeys::TS] | 0;

    if (doc.containsKey(AppJsonKeys::PASSCODES) && doc[AppJsonKeys::PASSCODES].is<JsonArray>())
    {
        for (JsonVariantConst v : doc[AppJsonKeys::PASSCODES].as<JsonArray>())
        {
            if (!v.is<JsonObject>())
                continue;

            auto it = PasscodeItem::fromJson(v);
            it.code.trim();
            it.type.trim();

            if (!isCodeValid(it.code))
                continue;

            if (!isTypeValid(it.type))
                continue;

            items_.push_back(it);
        }
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
    return saveAll();
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
    return saveAll();
}

bool
PasscodeRepository::clearTemp()
{
    hasTemp_ = false;
    return saveAll();
}

const std::vector<PasscodeItem>&
PasscodeRepository::listItems() const
{
    return items_;
}

bool
PasscodeRepository::setItems(const std::vector<PasscodeItem>& items, long ts)
{
    items_.clear();
    items_.reserve(items.size());

    for (auto it : items)
    {
        it.code.trim();
        it.type.trim();
        if (!isCodeValid(it.code))
            continue;
        if (!isTypeValid(it.type))
            continue;

        items_.push_back(it);
    }

    ts_ = ts;
    return saveAll();
}

long
PasscodeRepository::ts() const
{
    return ts_;
}

void
PasscodeRepository::setTs(long ts)
{
    ts_ = ts;
}

bool
PasscodeRepository::saveAll()
{
    const size_t est = kMinCap + items_.size() * 96;
    DynamicJsonDocument doc(est > kMaxCap ? kMaxCap : est);

    doc[AppJsonKeys::PASSCODES_MASTER] = master_;
    doc[AppJsonKeys::TS] = ts_;

    if (hasTemp_)
    {
        JsonObject t = doc.createNestedObject(AppJsonKeys::PASSCODES_TEMP);
        temp_.toJson(t);
    }

    JsonArray items = doc.createNestedArray(AppJsonKeys::PASSCODES);
    for (const auto& p : items_)
    {
        JsonObject o = items.createNestedObject();
        p.toJson(o);
    }

    return FileSystem::writeFileAtomic(PATH, JsonUtils::serialize(doc));
}