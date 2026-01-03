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

    /* ================= MASTER ================= */
    String
    getMaster() const;
    bool
    setMaster(const String& pass);

    /* ================= TEMP ================= */
    bool
    hasTemp() const;
    const Passcode&
    getTemp() const;
    bool
    setTemp(const Passcode& temp);
    bool
    clearTemp();

    /* ================= STORED (one_time / timed) ================= */
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

    /* ================= META ================= */
    long
    ts() const;
    void
    setTs(long ts);

  private:
    String master_;

    Passcode temp_;
    bool hasTemp_{false};

    std::vector<Passcode> items_;
    long ts_{0};

    static constexpr const char* PATH = AppPaths::PASSCODES_JSON;

    static size_t
    calcDocCapacity(const String& json);
    bool
    saveAll();
};
