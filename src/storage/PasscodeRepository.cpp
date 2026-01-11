#include "storage/PasscodeRepository.h"

#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"

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

    if (doc.containsKey(AppJsonKeys::TS))
        ts_ = doc[AppJsonKeys::TS] | 0;

    if (doc.containsKey(AppJsonKeys::PASSCODES_MASTER))
    {
        master_ = doc[AppJsonKeys::PASSCODES_MASTER] | "";
        master_.trim();
    }

    if (doc.containsKey(AppJsonKeys::PASSCODES_TEMP) &&
        doc[AppJsonKeys::PASSCODES_TEMP].is<JsonObject>())
    {
        temp_ = Passcode::fromJson(
            doc[AppJsonKeys::PASSCODES_TEMP].as<JsonObjectConst>()
        );
        hasTemp_ = isCodeValid(temp_.code);
    }

    if (doc.containsKey("items") &&
        doc["items"].is<JsonArray>())
    {
        JsonArray arr = doc["items"].as<JsonArray>();
        items_.reserve(arr.size());

        for (JsonVariantConst v : arr)
        {
            if (!v.is<JsonObject>())
                continue;

            Passcode p = Passcode::fromJson(v.as<JsonObjectConst>());
            p.code.trim();
            p.type.trim();

            if (!isCodeValid(p.code))
                continue;

            if (!isTypeValid(p.type))
                continue;

            items_.push_back(p);
        }
    }

    tsMillisAtLoad_ = millis();
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

uint64_t
PasscodeRepository::ts() const
{
    return ts_;
}

void
PasscodeRepository::setTs(uint64_t ts)
{
    ts_ = ts;
    tsMillisAtLoad_ = millis();
}

uint64_t 
PasscodeRepository::nowSecondsFallback() const
{
    if (ts_ > 0)
    {
        const uint32_t deltaMs = millis() - tsMillisAtLoad_;
        return (uint64_t)ts_ + (deltaMs / 1000);
    }

    return TimeUtils::nowSeconds();
}

bool
PasscodeRepository::saveAll()
{
    const char* TAG = "PASSCODE_SAVE";

    Logger::info(TAG, "==== saveAll() BEGIN ====");
    Logger::info(TAG, "master='%s'", master_.c_str());
    Logger::info(TAG, "ts=%llu", (unsigned long long)ts_);
    Logger::info(TAG, "items count=%u", (unsigned)items_.size());

    for (size_t i = 0; i < items_.size(); ++i)
    {
        const auto& p = items_[i];
        Logger::info(
            TAG,
            "item[%u]: code='%s', type='%s', effectiveAt=%llu, expireAt=%llu",
            (unsigned)i,
            p.code.c_str(),
            p.type.c_str(),
            (unsigned long long)p.effectiveAt,
            (unsigned long long)p.expireAt
        );
    }

    const size_t est = kMinCap + items_.size() * 128;
    DynamicJsonDocument doc(est > kMaxCap ? kMaxCap : est);

    doc[AppJsonKeys::PASSCODES_MASTER] = master_;
    doc[AppJsonKeys::TS] = (uint64_t)ts_;

    JsonArray arr = doc.createNestedArray(AppJsonKeys::PASSCODES);
    for (const auto& p : items_)
    {
        JsonObject o = arr.createNestedObject();
        o["code"] = p.code;
        o["type"] = p.type;
        o["effectiveAt"] = (uint64_t)p.effectiveAt;
        o["expireAt"] = (uint64_t)p.expireAt;
    }

    String json = JsonUtils::serialize(doc);
    Logger::info(TAG, "json length=%u", (unsigned)json.length());

    const unsigned maxShow = 256;
    if (json.length() <= maxShow)
    {
        Logger::info(TAG, "json='%s'", json.c_str());
    }
    else
    {
        Logger::info(
            TAG,
            "json(head %u)='%s...'",
            maxShow,
            json.substring(0, maxShow).c_str()
        );
    }

    const bool ok = FileSystem::writeFileAtomic(PATH, json);

    Logger::info(TAG, "writeFileAtomic -> %d", (int)ok);
    Logger::info(TAG, "==== saveAll() END ====");

    return ok;
}
