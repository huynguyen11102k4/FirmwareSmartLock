#pragma once
#include "config/AppPaths.h"
#include "models/PasscodeTemp.h"

#include <Arduino.h>

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

  private:
    String master_;
    PasscodeTemp temp_;
    bool hasTemp_{false};

    static constexpr const char* PATH = AppPaths::PASSCODES_JSON;
};
