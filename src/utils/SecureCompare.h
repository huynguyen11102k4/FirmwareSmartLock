#pragma once
#include <Arduino.h>

class SecureCompare
{
  public:
    // Constant-time string comparison to prevent timing attacks
    static bool
    safeEquals(const String& a, const String& b)
    {
        // Always compare full length to prevent early exit timing leak
        const size_t maxLen = max(a.length(), b.length());

        volatile uint8_t result = 0;

        // XOR all bytes - result will be 0 only if all match
        for (size_t i = 0; i < maxLen; i++)
        {
            uint8_t charA = (i < a.length()) ? a[i] : 0;
            uint8_t charB = (i < b.length()) ? b[i] : 0;
            result |= charA ^ charB;
        }

        // Length mismatch check (also constant-time)
        result |= (a.length() ^ b.length());

        return result == 0;
    }

    // Overload for C-strings
    static bool
    safeEquals(const char* a, const char* b)
    {
        if (!a || !b)
            return false;

        return safeEquals(String(a), String(b));
    }
};