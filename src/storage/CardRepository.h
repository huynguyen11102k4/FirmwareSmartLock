#pragma once
#include "config/AppPaths.h"

#include <Arduino.h>
#include <vector>

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
    remove(const String& uid);

    const std::vector<String>&
    list() const;

    bool
    isEmpty() const;

    size_t
    size() const;

  private:
    std::vector<String> cards_;
    static constexpr const char* PATH = AppPaths::CARDS_JSON;
};