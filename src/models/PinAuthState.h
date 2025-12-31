#pragma once
#include <Arduino.h>

struct PinAuthState
{
    String buffer = "";
    int failedCount = 0;
    uint32_t lockoutUntilMs = 0;

    void
    clearBuffer()
    {
        buffer = "";
    }

    bool
    appendDigit(char digit, int maxLen)
    {
        if ((int)buffer.length() >= maxLen)
            return false;

        buffer += digit;
        return true;
    }

    String
    getBuffer() const
    {
        return buffer;
    }

    bool
    isLockedOut() const
    {
        if (lockoutUntilMs == 0)
            return false;

        return (int32_t)(millis() - lockoutUntilMs) < 0;
    }

    bool
    recordFailedAttempt(int maxFailedAttempts, uint32_t lockoutDurationMs)
    {
        failedCount++;

        if (failedCount >= maxFailedAttempts)
        {
            lockoutUntilMs = millis() + lockoutDurationMs;
            failedCount = 0;
            return true;
        }

        return false;
    }

    void
    recordSuccess()
    {
        failedCount = 0;
        lockoutUntilMs = 0;
        clearBuffer();
    }

    uint32_t
    remainingLockoutSeconds() const
    {
        if (!isLockedOut())
            return 0;

        const int32_t msLeft = (int32_t)(lockoutUntilMs - millis());
        if (msLeft <= 0)
            return 0;

        return (uint32_t)(msLeft / 1000);
    }

    void
    reset()
    {
        buffer = "";
        failedCount = 0;
        lockoutUntilMs = 0;
    }
};
