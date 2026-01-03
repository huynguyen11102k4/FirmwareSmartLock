#pragma once
#include <Arduino.h>

namespace Topics
{
inline String
passcodes(const String& base)
{
    return base + "/passcodes";
}

inline String
passcodesReq(const String& base)
{
    return base + "/passcodes/request";
}

inline String
iccards(const String& base)
{
    return base + "/iccards";
}

inline String
iccardsReq(const String& base)
{
    return base + "/iccards/request";
}

inline String
control(const String& base)
{
    return base + "/control";
}

inline String
info(const String& base)
{
    return base + "/info";
}

inline String
state(const String& base)
{
    return base + "/state";
}

inline String
log(const String& base)
{
    return base + "/log";
}

inline String
battery(const String& base)
{
    return base + "/battery";
}

inline String
batteryReq(const String& base)
{
    return base + "/battery/request";
}

inline String
passcodesList(const String& base)
{
    return base + "/passcodeslist";
}

inline String
iccardsList(const String& base)
{
    return base + "/iccardslist";
}

inline String
iccardsStatus(const String& base)
{
    return base + "/iccards/status";
}

inline String
passcodesError(const String& base)
{
    return base + "/passcodes/error";
}
} // namespace Topics
