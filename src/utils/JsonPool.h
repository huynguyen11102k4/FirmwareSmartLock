#pragma once
#include <ArduinoJson.h>

// Pre-allocated JSON buffers to prevent heap fragmentation
class JsonPool
{
  public:
    enum class Size
    {
        SMALL = 256,  // Simple messages
        MEDIUM = 512, // Config, commands
        LARGE = 1024, // Card lists
        XLARGE = 2048 // Full state dumps
    };

    // Get a buffer of specified size
    static DynamicJsonDocument&
    acquire(Size size)
    {
        switch (size)
        {
            case Size::SMALL:
                small_.clear();
                return small_;
            case Size::MEDIUM:
                medium_.clear();
                return medium_;
            case Size::LARGE:
                large_.clear();
                return large_;
            case Size::XLARGE:
                xlarge_.clear();
                return xlarge_;
            default:
                medium_.clear();
                return medium_;
        }
    }

    // Convenience methods
    static DynamicJsonDocument&
    acquireSmall()
    {
        return acquire(Size::SMALL);
    }

    static DynamicJsonDocument&
    acquireMedium()
    {
        return acquire(Size::MEDIUM);
    }

    static DynamicJsonDocument&
    acquireLarge()
    {
        return acquire(Size::LARGE);
    }

    static DynamicJsonDocument&
    acquireXLarge()
    {
        return acquire(Size::XLARGE);
    }

    // Release (explicit clear)
    static void
    releaseAll()
    {
        small_.clear();
        medium_.clear();
        large_.clear();
        xlarge_.clear();
    }

  private:
    static DynamicJsonDocument small_;
    static DynamicJsonDocument medium_;
    static DynamicJsonDocument large_;
    static DynamicJsonDocument xlarge_;
};

DynamicJsonDocument JsonPool::small_(256);
DynamicJsonDocument JsonPool::medium_(512);
DynamicJsonDocument JsonPool::large_(1024);
DynamicJsonDocument JsonPool::xlarge_(2048);