#pragma once
#include <Arduino.h>

struct RuntimeFlags
{
    // Learning/Training modes
    bool learningMode = false;
    bool maintenance = false;

    // BLE state
    bool bleActive = false;

    // Swipe-to-add mode (thay vì để trong AppState)
    bool swipeAddMode = false;

    void
    reset()
    {
        learningMode = false;
        maintenance = false;
        bleActive = false;
        swipeAddMode = false;
    }

    bool
    hasActiveMode() const
    {
        return learningMode || maintenance || bleActive || swipeAddMode;
    }
};