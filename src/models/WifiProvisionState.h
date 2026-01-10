#pragma once
#include <Arduino.h>

struct WifiProvisionState
{
    bool waitingForConnection = false;
    uint8_t connectionAttempts = 0;
    uint32_t lastAttemptMs = 0;
    static constexpr uint8_t MAX_ATTEMPTS = 5;
    static constexpr uint32_t ATTEMPT_INTERVAL_MS = 5000;

    void
    startWaiting()
    {
        waitingForConnection = true;
        connectionAttempts = 0;
        lastAttemptMs = millis();
    }

    void
    reset()
    {
        waitingForConnection = false;
        connectionAttempts = 0;
        lastAttemptMs = 0;
    }

    bool
    shouldRetry() const
    {
        if (!waitingForConnection)
            return false;

        if (connectionAttempts >= MAX_ATTEMPTS)
            return false;

        return (uint32_t)(millis() - lastAttemptMs) >= ATTEMPT_INTERVAL_MS;
    }

    void
    recordAttempt()
    {
        connectionAttempts++;
        lastAttemptMs = millis();
    }

    bool
    hasExceededMaxAttempts() const
    {
        return connectionAttempts >= MAX_ATTEMPTS;
    }

    uint8_t
    getRemainingAttempts() const
    {
        return (connectionAttempts < MAX_ATTEMPTS) ? (MAX_ATTEMPTS - connectionAttempts) : 0;
    }
};