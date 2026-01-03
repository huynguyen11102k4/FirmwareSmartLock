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
    return !code.isEmpty();
}

// items_ chỉ chứa one_time / timed
bool
isTypeValid(const String& type)
{
    return type == "one_time" || type == "timed";
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
    master_.clear();
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
        JsonObject obj = doc[AppJsonKeys::PASSCODES_TEMP].as<JsonObject>();

        temp_ = Passcode::fromJson(obj);
        hasTemp_ = isCodeValid(temp_.code);
    }

    if (doc.containsKey(AppJsonKeys::TS))
        ts_ = doc[AppJsonKeys::TS] | 0;

    if (doc.containsKey(AppJsonKeys::PASSCODES) && doc[AppJsonKeys::PASSCODES].is<JsonArray>())
    {
        JsonArray arr = doc[AppJsonKeys::PASSCODES].as<JsonArray>();

        for (JsonVariantConst v : arr)
        {
            if (!v.is<JsonObject>())
                continue;

            JsonObjectConst obj = v.as<JsonObjectConst>();
            Passcode p = Passcode::fromJson(obj);

            p.code.trim();
            p.type.trim();

            if (!isCodeValid(p.code))
                continue;

            if (!isTypeValid(p.type))
                continue;

            items_.push_back(p);
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

const Passcode&
PasscodeRepository::getTemp() const
{
    return temp_;
}

bool
PasscodeRepository::setTemp(const Passcode& temp)
{
    temp_ = temp;
    hasTemp_ = isCodeValid(temp.code);
    return saveAll();
}

bool
PasscodeRepository::clearTemp()
{
    hasTemp_ = false;
    return saveAll();
}

const std::vector<Passcode>&
PasscodeRepository::listItems() const
{
    return items_;
}

bool
PasscodeRepository::setItems(const std::vector<Passcode>& items, long ts)
{
    items_.clear();
    items_.reserve(items.size());

    for (auto p : items)
    {
        p.code.trim();
        p.type.trim();

        if (!isCodeValid(p.code))
            continue;

        if (!isTypeValid(p.type))
            continue;

        items_.push_back(p);
    }

    ts_ = ts;
    return saveAll();
}

bool
PasscodeRepository::addItem(const Passcode& p)
{
    Passcode c = p;
    c.code.trim();
    c.type.trim();

    if (!isCodeValid(c.code))
        return false;

    if (!isTypeValid(c.type))
        return false;

    items_.push_back(c);
    return saveAll();
}

bool
PasscodeRepository::removeItemByCode(const String& code)
{
    bool removed = false;

    items_.erase(
        std::remove_if(
            items_.begin(), items_.end(),
            [&](const Passcode& p)
            {
                if (p.code == code)
                {
                    removed = true;
                    return true;
                }
                return false;
            }
        ),
        items_.end()
    );

    if (removed)
        return saveAll();

    return false;
}

bool
PasscodeRepository::findItemByCode(const String& code, Passcode& out) const
{
    for (const auto& p : items_)
    {
        if (p.code == code)
        {
            out = p;
            return true;
        }
    }
    return false;
}

bool
PasscodeRepository::validateAndConsume(const String& code, long now)
{
    for (size_t i = 0; i < items_.size(); ++i)
    {
        Passcode& p = items_[i];

        if (p.code != code)
            continue;

        if (p.isExpired(now))
        {
            items_.erase(items_.begin() + i);
            saveAll();
            return false;
        }

        if (!p.isEffective(now))
            return false;

        // ===== one_time =====
        if (p.type == "one_time")
        {
            items_.erase(items_.begin() + i);
            saveAll();
            return true;
        }

        if (p.type == "timed")
        {
            return true;
        }

        return false;
    }

    return false;
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

    JsonArray arr = doc.createNestedArray(AppJsonKeys::PASSCODES);

    for (const auto& p : items_)
    {
        JsonObject o = arr.createNestedObject();
        p.toJson(o);
    }

    return FileSystem::writeFileAtomic(PATH, JsonUtils::serialize(doc));
}
