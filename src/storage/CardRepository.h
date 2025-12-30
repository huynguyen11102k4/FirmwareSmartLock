#pragma once
#include <Arduino.h>
#include <vector>

class CardRepository
{
public:
    bool load();
    bool save();

    bool exists(const String &uid) const;
    bool add(const String &uid);
    bool remove(const String &uid);

private:
    std::vector<String> _cards;
    static constexpr const char *PATH = "/cards.json";
};
