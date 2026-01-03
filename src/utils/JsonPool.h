#pragma once
#include <ArduinoJson.h>

class JsonPool
{
  public:
    enum class Size
    {
        SMALL = 256,
        MEDIUM = 512,
        LARGE = 1024,
        XLARGE = 2048
    };

    static DynamicJsonDocument&
    acquire(Size size);

    static DynamicJsonDocument&
    acquireSmall();

    static DynamicJsonDocument&
    acquireMedium();

    static DynamicJsonDocument&
    acquireLarge();

    static DynamicJsonDocument&
    acquireXLarge();

    static void
    releaseAll();

  private:
    static DynamicJsonDocument small_;
    static DynamicJsonDocument medium_;
    static DynamicJsonDocument large_;
    static DynamicJsonDocument xlarge_;
};