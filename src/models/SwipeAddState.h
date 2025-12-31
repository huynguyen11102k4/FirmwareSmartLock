#pragma once
#include <Arduino.h>

struct SwipeAddState
{
    String firstSwipeUid = "";
    uint32_t timeoutMs = 0;

    void
    start(uint32_t timeoutDurationMs)
    {
        firstSwipeUid = "";
        timeoutMs = millis() + timeoutDurationMs;
    }

    void
    recordFirstSwipe(const String& uid, uint32_t timeoutDurationMs)
    {
        firstSwipeUid = uid;
        timeoutMs = millis() + timeoutDurationMs;
    }

    bool
    isTimeout() const
    {
        if (timeoutMs == 0)
            return false;

        return (int32_t)(millis() - timeoutMs) >= 0;
    }

    bool
    hasFirstSwipe() const
    {
        return firstSwipeUid.length() > 0;
    }

    bool
    matchesFirstSwipe(const String& uid) const
    {
        return hasFirstSwipe() && firstSwipeUid == uid;
    }

    void
    reset()
    {
        firstSwipeUid = "";
        timeoutMs = 0;
    }

    uint32_t
    remainingSeconds() const
    {
        if (timeoutMs == 0)
            return 0;

        const int32_t msLeft = (int32_t)(timeoutMs - millis());
        if (msLeft <= 0)
            return 0;

        return (uint32_t)(msLeft / 1000);
    }
};