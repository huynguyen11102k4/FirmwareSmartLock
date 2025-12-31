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
    uint32_t relockAtMs = 0; // Thời điểm tự động khóa lại (millis)

    static constexpr uint32_t DEFAULT_UNLOCK_DURATION_MS = 5000; // 5s

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
    unlock(uint32_t durationMs = DEFAULT_UNLOCK_DURATION_MS)
    {
        state = State::UNLOCKED;
        relockAtMs = millis() + durationMs;
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
        {
            return false;
        }
        return millis() >= relockAtMs;
    }

    uint32_t
    remainingUnlockSeconds() const
    {
        if (state != State::UNLOCKED)
            return 0;
        if (millis() >= relockAtMs)
            return 0;
        return (relockAtMs - millis()) / 1000;
    }

    const char*
    getStateString() const
    {
        return (state == State::LOCKED) ? "locked" : "unlocked";
    }
};