#include "app/services/KeypadService.h"

#include "config/HardwarePins.h"
#include "models/PasscodeTemp.h"
#include "utils/Logger.h"
#include "utils/SecureCompare.h"
#include "utils/TimeUtils.h"

#include <Arduino.h>

#define KEYPAD_DEBUG 1

namespace
{
static constexpr byte ROWS = 4;
static constexpr byte COLS = 4;

static char KEYS[ROWS][COLS] = {
    {'D', 'C', 'B', 'A'}, {'#', '9', '6', '3'}, {'0', '8', '5', '2'}, {'*', '7', '4', '1'}
};

static byte ROW_PINS[ROWS] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};

static byte COL_PINS[COLS] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};
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
    Logger::info("KEYPAD", "=== KEYPAD SERVICE INIT ===");

    const String master = passRepo_.getMaster();
    Logger::info("KEYPAD", "MASTER PIN loaded: '%s' (len=%d)", master.c_str(), master.length());

    if (passRepo_.hasTemp())
    {
        const PasscodeTemp t = passRepo_.getTemp();
        Logger::info(
            "KEYPAD", "TEMP PIN loaded: '%s' (expire=%ld, now=%ld)", t.code.c_str(), t.expireAt,
            TimeUtils::nowSeconds()
        );
    }
    else
    {
        Logger::info("KEYPAD", "NO TEMP PIN loaded");
    }

    Logger::info("KEYPAD", "===========================");
}

bool
KeypadService::checkPIN_(const String& pin)
{
    Logger::info("KEYPAD", "Checking PIN: %s", pin.c_str());

    const String master = passRepo_.getMaster();

    if (master.length() >= (size_t)lockConfig_.minPinLength &&
        SecureCompare::safeEquals(pin, master))
    {
        Logger::info("KEYPAD", "PIN matched MASTER");
        Logger::info("KEYPAD", "UNLOCK by master PIN");
        return true;
    }

    if (passRepo_.hasTemp())
    {
        const PasscodeTemp t = passRepo_.getTemp();

        if (t.isExpired(TimeUtils::nowSeconds()))
        {
            Logger::info("KEYPAD", "Temp PIN expired: %s", t.code.c_str());
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
            return false;
        }

        if (SecureCompare::safeEquals(pin, t.code))
        {
            Logger::info("KEYPAD", "PIN matched TEMP: %s", pin.c_str());
            Logger::info("KEYPAD", "UNLOCK by temp PIN");
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
            return true;
        }
    }

    Logger::info("KEYPAD", "PIN mismatch: %s", pin.c_str());
    return false;
}

void
KeypadService::loop()
{
    if (appState_.pinAuth.isLockedOut())
    {
        const char k = keypad_.getKey();
        if (k)
        {
            Logger::info("KEYPAD", "Key '%c' ignored (LOCKOUT)", k);
        }
        return;
    }

    const char k = keypad_.getKey();
    if (!k)
        return;

    if (k == '*')
    {
        Logger::info("KEYPAD", "PIN buffer cleared by user");
        appState_.pinAuth.clearBuffer();
        return;
    }

    if (k == '#')
    {
        const String pin = appState_.pinAuth.getBuffer();

        Logger::info("KEYPAD", "PIN SUBMIT: %s", pin.c_str());

        if ((int)pin.length() < lockConfig_.minPinLength)
        {
            Logger::info(
                "KEYPAD", "PIN too short (%d < %d)", pin.length(), lockConfig_.minPinLength
            );

            const bool lockedOut = appState_.pinAuth.recordFailedAttempt(
                lockConfig_.maxFailedAttempts, lockConfig_.lockoutDurationMs
            );

            Logger::info("KEYPAD", "PIN auth failed (length)");

            if (lockedOut)
            {
                Logger::info(
                    "KEYPAD", "LOCKOUT triggered (%lds remaining)",
                    appState_.pinAuth.remainingLockoutSeconds()
                );
            }

            appState_.pinAuth.clearBuffer();
            return;
        }

        if (checkPIN_(pin))
        {
            Logger::info("KEYPAD", "PIN auth SUCCESS");
            appState_.pinAuth.recordSuccess();
            door_.requestUnlock("pin");
        }
        else
        {
            Logger::info("KEYPAD", "PIN auth FAILED");

            const bool lockedOut = appState_.pinAuth.recordFailedAttempt(
                lockConfig_.maxFailedAttempts, lockConfig_.lockoutDurationMs
            );

            if (lockedOut)
            {
                Logger::info(
                    "KEYPAD", "LOCKOUT triggered (%lds remaining)",
                    appState_.pinAuth.remainingLockoutSeconds()
                );
            }
        }

        appState_.pinAuth.clearBuffer();
        return;
    }

    if (k >= '0' && k <= '9')
    {
        appState_.pinAuth.appendDigit(k, lockConfig_.maxPinLength);
        Logger::info("KEYPAD", "Digit appended: %c", k);
        return;
    }

    Logger::info("KEYPAD", "Non-numeric key ignored: %c", k);
}
