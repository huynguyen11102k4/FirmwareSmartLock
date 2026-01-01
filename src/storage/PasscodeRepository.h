#pragma once
#include "config/AppPaths.h"
#include "models/PasscodeTemp.h"

#include <Arduino.h>
#include <vector>

struct PasscodeItem
{
    String code;
    String type; // "permanent" | "one_time" | "timed"
    bool hasValidFrom{false};
    bool hasValidTo{false};
    long validFrom{0};
    long validTo{0};

    static PasscodeItem
    fromJson(const JsonVariantConst& v)
    {
        PasscodeItem out;
        out.code = v["code"] | "";
        out.type = v["type"] | "";
        if (!v["validFrom"].isNull())
        {
            out.hasValidFrom = true;
            out.validFrom = v["validFrom"] | 0;
        }
        if (!v["validTo"].isNull())
        {
            out.hasValidTo = true;
            out.validTo = v["validTo"] | 0;
        }
        return out;
    }

    void
    toJson(JsonObject obj) const
    {
        obj["code"] = code;
        obj["type"] = type;
        if (hasValidFrom)
            obj["validFrom"] = validFrom;
        else
            obj["validFrom"] = nullptr;
        if (hasValidTo)
            obj["validTo"] = validTo;
        else
            obj["validTo"] = nullptr;
    }
};

class PasscodeRepository
{
  public:
    bool
    load();

    String
    getMaster() const;

    bool
    setMaster(const String& pass);

    bool
    hasTemp() const;

    PasscodeTemp
    getTemp() const;

    bool
    setTemp(const PasscodeTemp& temp);

    bool
    clearTemp();

    const std::vector<PasscodeItem>&
    listItems() const;
    bool
    setItems(const std::vector<PasscodeItem>& items, long ts);

    long
    ts() const;
    void
    setTs(long ts);

  private:
    String master_;
    PasscodeTemp temp_;
    bool hasTemp_{false};

    std::vector<PasscodeItem> items_;
    long ts_{0};

    static constexpr const char* PATH = AppPaths::PASSCODES_JSON;

    static size_t
    calcDocCapacity(const String& json);
    bool
    saveAll();
};
