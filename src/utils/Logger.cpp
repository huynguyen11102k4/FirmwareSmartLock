#include "utils/Logger.h"

#include <stdarg.h>

LogLevel Logger::level_ = LogLevel::INFO;

void
Logger::begin(uint32_t baud)
{
    Serial.begin(baud);
}

void
Logger::setLevel(LogLevel level)
{
    level_ = level;
}

void
Logger::log(LogLevel level, const char* tag, const char* fmt, va_list args)
{
    if (level > level_)
        return;

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    const char* lvl = (level == LogLevel::ERROR) ? "E"
        : (level == LogLevel::WARN)              ? "W"
        : (level == LogLevel::INFO)              ? "I"
                                                 : "D";

    Serial.printf("[%s][%s] %s\n", lvl, tag, buffer);
}

#define LOG_IMPL(fn, lvl)                                                                          \
    void Logger::fn(const char* tag, const char* fmt, ...)                                         \
    {                                                                                              \
        va_list args;                                                                              \
        va_start(args, fmt);                                                                       \
        log(lvl, tag, fmt, args);                                                                  \
        va_end(args);                                                                              \
    }

LOG_IMPL(error, LogLevel::ERROR)
LOG_IMPL(warn, LogLevel::WARN)
LOG_IMPL(info, LogLevel::INFO)
LOG_IMPL(debug, LogLevel::DEBUG)
