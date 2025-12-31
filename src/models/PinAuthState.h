#pragma once
#include <Arduino.h>

struct PinAuthState
{
    String buffer = "";          // Buffer chứa PIN đang nhập
    int failedCount = 0;         // Số lần nhập sai liên tiếp
    uint32_t lockoutUntilMs = 0; // Thời điểm hết lockout (millis)

    static constexpr int MAX_PIN_LENGTH = 10;
    static constexpr int MAX_FAILED_ATTEMPTS = 5;
    static constexpr uint32_t LOCKOUT_DURATION_MS = 30000; // 30s

    void
    clearBuffer()
    {
        buffer = "";
    }

    bool
    appendDigit(char digit)
    {
        if (buffer.length() >= MAX_PIN_LENGTH)
        {
            return false;
        }
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
        return millis() < lockoutUntilMs;
    }

    bool
    recordFailedAttempt()
    {
        failedCount++;

        if (failedCount >= MAX_FAILED_ATTEMPTS)
        {
            lockoutUntilMs = millis() + LOCKOUT_DURATION_MS;
            failedCount = 0; // Reset counter
            return true;     // Locked out
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
        return (lockoutUntilMs - millis()) / 1000;
    }

    void
    reset()
    {
        buffer = "";
        failedCount = 0;
        lockoutUntilMs = 0;
    }
};