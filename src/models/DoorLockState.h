#pragma once
#include <Arduino.h>

struct DoorLockState
{
    enum class State : uint8_t
    {
        LOCKED,
        UNLOCKED
    };

    State state = State::LOCKED;
    uint32_t relockAtMs = 0;

    bool
    isLocked() const
    {
        return state == State::LOCKED;
    }

    bool
    isUnlocked() const
    {
        return state == State::UNLOCKED;
    }

    void
    unlock(uint32_t durationMs)
    {
        state = State::UNLOCKED;
        relockAtMs = millis() + durationMs;
    }

    void
    rearmAutoRelock(uint32_t delayMs)
    {
        if (state != State::UNLOCKED)
            return;
        relockAtMs = millis() + delayMs;
    }

    void
    lock()
    {
        state = State::LOCKED;
        relockAtMs = 0;
    }

    bool
    shouldAutoRelock() const
    {
        if (state != State::UNLOCKED)
            return false;

        return (int32_t)(millis() - relockAtMs) >= 0;
    }

    uint32_t
    remainingUnlockSeconds() const
    {
        if (state != State::UNLOCKED)
            return 0;

        const int32_t msLeft = (int32_t)(relockAtMs - millis());
        if (msLeft <= 0)
            return 0;

        return (uint32_t)(msLeft / 1000);
    }

    const char*
    getStateString() const
    {
        return (state == State::LOCKED) ? "locked" : "unlocked";
    }
};