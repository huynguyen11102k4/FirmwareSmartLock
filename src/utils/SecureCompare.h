#pragma once
#include <Arduino.h>

class SecureCompare
{
  public:
    static bool
    safeEquals(const String& a, const String& b)
    {
        const size_t maxLen = max(a.length(), b.length());

        volatile uint8_t result = 0;

        for (size_t i = 0; i < maxLen; i++)
        {
            const uint8_t charA = (i < a.length()) ? a[i] : 0;
            const uint8_t charB = (i < b.length()) ? b[i] : 0;
            result |= (uint8_t)(charA ^ charB);
        }

        result |= (uint8_t)(a.length() ^ b.length());
        return result == 0;
    }

    static bool
    safeEquals(const char* a, const char* b)
    {
        if (!a || !b)
            return false;

        return safeEquals(String(a), String(b));
    }
};
