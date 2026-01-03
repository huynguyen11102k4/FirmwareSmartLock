#include "utils/TimeUtils.h"

#include <time.h>

uint32_t
TimeUtils::nowMillis()
{
    return millis();
}

uint64_t
TimeUtils::nowSeconds()
{
    time_t now;
    time(&now);
    return (uint64_t)now;
}

bool
TimeUtils::isExpired(uint32_t startMillis, uint32_t timeoutMs)
{
    return (uint32_t)(millis() - startMillis) >= timeoutMs;
}

String
TimeUtils::formatTime(uint64_t epoch)
{
    const time_t t = (time_t)epoch;
    struct tm* tm_info = localtime(&t);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return String(buffer);
}