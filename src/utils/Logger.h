#pragma once
#include <Arduino.h>

enum class LogLevel
{
    ERROR = 0,
    WARN,
    INFO,
    DEBUG
};

class Logger
{
  public:
    static void
    begin(uint32_t baud = 115200);

    static void
    error(const char* tag, const char* fmt, ...);
    static void
    warn(const char* tag, const char* fmt, ...);
    static void
    info(const char* tag, const char* fmt, ...);
    static void
    debug(const char* tag, const char* fmt, ...);

    static void
    setLevel(LogLevel level);

  private:
    static LogLevel _level;
    static void
    log(LogLevel level, const char* tag, const char* fmt, va_list args);
};
