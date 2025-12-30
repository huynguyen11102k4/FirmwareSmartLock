#include "app/services/KeypadService.h"

#include <Arduino.h>

#include "models/PasscodeTemp.h"
#include "utils/TimeUtils.h"

#include "config/HardwarePins.h"

namespace
{
  static constexpr byte ROWS = 4;
  static constexpr byte COLS = 4;

  static char KEYS[ROWS][COLS] = {
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'}};

  static byte ROW_PINS[ROWS] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};
  static byte COL_PINS[COLS] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};
} 

KeypadService::KeypadService(AppState &appState,
                             PasscodeRepository &passRepo,
                             PublishService &publish,
                             void *ctx,
                             UnlockFn onUnlock)
    : appState_(appState),
      passRepo_(passRepo),
      publish_(publish),
      ctx_(ctx),
      onUnlock_(onUnlock),
      keypad_(makeKeymap(KEYS), ROW_PINS, COL_PINS, ROWS, COLS)
{
}

void KeypadService::begin() {}

bool KeypadService::checkPIN_(const String& pin) {
  const String master = passRepo_.getMaster();
  if (master.length() >= 4 && pin == master) {
    publish_.publishLog("unlock", "master_pin", pin);
    return true;
  }

  if (passRepo_.hasTemp()) {
    PasscodeTemp t = passRepo_.getTemp();

    if (t.isExpired(TimeUtils::nowSeconds())) {
      passRepo_.clearTemp();
      publish_.publishPasscodeList();
      return false;
    }

    if (pin == t.code) {
      publish_.publishLog("unlock", "temp_pin", pin);
      passRepo_.clearTemp();
      publish_.publishPasscodeList();
      return true;
    }
  }

  publish_.publishLog("wrong_pin", "pin", pin);
  return false;
}

void KeypadService::loop() {
  if (appState_.pinAuth.isLockedOut()) return;

  const char k = keypad_.getKey();
  if (!k) return;

  if (k == '*') {
    appState_.pinAuth.clearBuffer();
    return;
  }

  if (k == '#') {
    const String pin = appState_.pinAuth.getBuffer();

    if (checkPIN_(pin)) {
      appState_.pinAuth.recordSuccess();
      if (onUnlock_) onUnlock_(ctx_, "pin");
    } else {
      const bool lockedOut = appState_.pinAuth.recordFailedAttempt();
      if (lockedOut) {
        publish_.publishLog(
          "pin_lockout",
          "security",
          "Locked for " + String(appState_.pinAuth.remainingLockoutSeconds()) + "s"
        );
      }
    }

    appState_.pinAuth.clearBuffer();
    return;
  }

  if (k >= '0' && k <= '9') {
    appState_.pinAuth.appendDigit(k);
  }
}