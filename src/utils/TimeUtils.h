#pragma once
#include <Arduino.h>

class TimeUtils
{
public:
    static uint32_t nowMillis();
    static uint64_t nowSeconds();

    static bool isExpired(uint32_t startMillis, uint32_t timeoutMs);

    static String formatTime(uint64_t epoch);
};
