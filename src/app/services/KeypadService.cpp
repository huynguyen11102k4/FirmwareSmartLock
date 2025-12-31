#include "app/services/KeypadService.h"

#include "config/HardwarePins.h"
#include "models/PasscodeTemp.h"
#include "utils/SecureCompare.h"
#include "utils/TimeUtils.h"

#include <Arduino.h>

namespace
{
static constexpr byte ROWS = 4;
static constexpr byte COLS = 4;

static char KEYS[ROWS][COLS] = {
    {'1', '2', '3', 'A'}, {'4', '5', '6', 'B'}, {'7', '8', '9', 'C'}, {'*', '0', '#', 'D'}
};

static byte ROW_PINS[ROWS] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};
static byte COL_PINS[COLS] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};

String
maskPinForLog(const String& pin)
{
    if (pin.isEmpty())
        return "****";
    const int keep = min(2, (int)pin.length());
    return pin.substring(0, keep) + "****";
}
} // namespace

KeypadService::KeypadService(
    AppState& appState, PasscodeRepository& passRepo, PublishService& publish, DoorHardware& door,
    const LockConfig& lockConfig
)
    : appState_(appState), passRepo_(passRepo), publish_(publish), door_(door),
      lockConfig_(lockConfig), keypad_(makeKeymap(KEYS), ROW_PINS, COL_PINS, ROWS, COLS)
{
}

void
KeypadService::begin()
{
}

bool
KeypadService::checkPIN_(const String& pin)
{
    const String master = passRepo_.getMaster();

    if (master.length() >= (size_t)lockConfig_.minPinLength &&
        SecureCompare::safeEquals(pin, master))
    {
        publish_.publishLog("unlock", "master_pin", "****");
        return true;
    }

    if (passRepo_.hasTemp())
    {
        const PasscodeTemp t = passRepo_.getTemp();

        if (t.isExpired(TimeUtils::nowSeconds()))
        {
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
            return false;
        }

        if (SecureCompare::safeEquals(pin, t.code))
        {
            publish_.publishLog("unlock", "temp_pin", "****");
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
            return true;
        }
    }

    publish_.publishLog("wrong_pin", "pin", maskPinForLog(pin));
    return false;
}

void
KeypadService::loop()
{
    if (appState_.pinAuth.isLockedOut())
        return;

    const char k = keypad_.getKey();
    if (!k)
        return;

    if (k == '*')
    {
        appState_.pinAuth.clearBuffer();
        return;
    }

    if (k == '#')
    {
        const String pin = appState_.pinAuth.getBuffer();

        if ((int)pin.length() < lockConfig_.minPinLength)
        {
            const bool lockedOut = appState_.pinAuth.recordFailedAttempt(
                lockConfig_.maxFailedAttempts, lockConfig_.lockoutDurationMs
            );

            publish_.publishLog("wrong_pin", "pin", maskPinForLog(pin));
            if (lockedOut)
            {
                publish_.publishLog(
                    "pin_lockout", "security",
                    "Locked for " + String(appState_.pinAuth.remainingLockoutSeconds()) + "s"
                );
            }

            appState_.pinAuth.clearBuffer();
            return;
        }

        if (checkPIN_(pin))
        {
            appState_.pinAuth.recordSuccess();
            door_.requestUnlock("pin");
        }
        else
        {
            const bool lockedOut = appState_.pinAuth.recordFailedAttempt(
                lockConfig_.maxFailedAttempts, lockConfig_.lockoutDurationMs
            );

            if (lockedOut)
            {
                publish_.publishLog(
                    "pin_lockout", "security",
                    "Locked for " + String(appState_.pinAuth.remainingLockoutSeconds()) + "s"
                );
            }
        }

        appState_.pinAuth.clearBuffer();
        return;
    }

    if (k >= '0' && k <= '9')
    {
        appState_.pinAuth.appendDigit(k, lockConfig_.maxPinLength);
    }
}
