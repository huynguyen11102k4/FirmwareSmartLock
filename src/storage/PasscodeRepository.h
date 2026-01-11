#pragma once
#include "config/AppPaths.h"
#include "models/PasscodeTemp.h"

#include <Arduino.h>
#include <vector>

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
    const Passcode&
    getTemp() const;
    bool
    setTemp(const Passcode& temp);
    bool
    clearTemp();

    const std::vector<Passcode>&
    listItems() const;
    bool
    setItems(const std::vector<Passcode>& items, long ts);
    bool
    addItem(const Passcode& p);
    bool
    removeItemByCode(const String& code);
    bool
    findItemByCode(const String& code, Passcode& out) const;

    bool
    validateAndConsume(const String& code, long now);

    uint64_t 
    nowSecondsFallback() const;


    uint64_t
    ts() const;
    
    void
    setTs(uint64_t ts);

  private:
    String master_;

    Passcode temp_;
    bool hasTemp_{false};

    std::vector<Passcode> items_;
    uint64_t ts_{0};

    uint32_t tsMillisAtLoad_ = 0;

    static constexpr const char* PATH = AppPaths::PASSCODES_JSON;

    static size_t
    calcDocCapacity(const String& json);
    bool
    saveAll();
};
