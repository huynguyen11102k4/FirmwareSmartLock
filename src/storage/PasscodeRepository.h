#pragma once
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
    String _master;
    PasscodeTemp _temp;
    bool _hasTemp = false;

    static constexpr const char* PATH = "/passcode.json";
};
