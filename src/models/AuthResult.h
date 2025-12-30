#pragma once
#include <Arduino.h>

enum class AuthResultType : uint8_t
{
    SUCCESS,
    FAIL,
    EXPIRED,
    DENIED
};

struct AuthResult
{
    AuthResultType result;
    String reason; // optional (log / debug)
};
