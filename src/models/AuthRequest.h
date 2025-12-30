#pragma once
#include <Arduino.h>

enum class AuthMethod : uint8_t
{
    RFID,
    KEYPAD,
    BLE,
    MQTT
};

struct AuthRequest
{
    AuthMethod method;
    String credential;  // UID / passcode / token
    uint64_t timestamp; // epoch seconds
};
