#pragma once
#include <Arduino.h>

struct RuntimeFlags
{
    bool learningMode = false;
    bool maintenance = false;

    bool bleActive = false;

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
