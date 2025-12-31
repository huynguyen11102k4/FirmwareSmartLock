#pragma once
#include <Arduino.h>

struct SwipeAddState
{
    String firstSwipeUid = ""; // UID của lần quẹt đầu tiên
    uint32_t timeoutMs = 0;    // Thời điểm timeout (millis)

    void
    start(uint32_t timeoutDuration = 60000)
    {
        firstSwipeUid = "";
        timeoutMs = millis() + timeoutDuration;
    }

    void
    recordFirstSwipe(const String& uid)
    {
        firstSwipeUid = uid;
        timeoutMs = millis() + 60000; // Reset timeout
    }

    bool
    isTimeout() const
    {
        return millis() > timeoutMs;
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
        if (isTimeout())
            return 0;
        return (timeoutMs - millis()) / 1000;
    }
};