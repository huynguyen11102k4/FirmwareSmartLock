#pragma once
#include "config/AppPaths.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct CardItem
{
    String uid;
    String name;

    static CardItem
    fromJson(const JsonVariantConst& v)
    {
        CardItem out;
        out.uid = v["uid"] | "";
        out.name = v["name"] | "";
        return out;
    }

    void
    toJson(JsonObject obj) const
    {
        obj["uid"] = uid;
        obj["name"] = name;
    }
};
class CardRepository
{
  public:
    bool
    load();

    bool
    save();

    bool
    exists(const String& uid) const;

    bool
    add(const String& uid);

    bool
    add(const String& uid, const String& name);

    bool
    updateName(const String& uid, const String& name);

    bool
    remove(const String& uid);

    const std::vector<CardItem>&
    list() const;

    bool
    isEmpty() const;

    size_t
    size() const;

    uint64_t
    ts() const;
    
    void
    setTs(uint64_t ts);

  private:
    std::vector<CardItem> cards_;
    uint64_t ts_{0};

    static constexpr const char* PATH = AppPaths::CARDS_JSON;

    static size_t
    calcDocCapacity(const String& json);

    bool
    saveInternal();
};