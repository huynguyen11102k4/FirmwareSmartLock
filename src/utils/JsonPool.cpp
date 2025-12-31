#include "utils/JsonPool.h"

DynamicJsonDocument JsonPool::small_(256);
DynamicJsonDocument JsonPool::medium_(512);
DynamicJsonDocument JsonPool::large_(1024);
DynamicJsonDocument JsonPool::xlarge_(2048);

DynamicJsonDocument&
JsonPool::acquire(Size size)
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

DynamicJsonDocument&
JsonPool::acquireSmall()
{
    return acquire(Size::SMALL);
}

DynamicJsonDocument&
JsonPool::acquireMedium()
{
    return acquire(Size::MEDIUM);
}

DynamicJsonDocument&
JsonPool::acquireLarge()
{
    return acquire(Size::LARGE);
}

DynamicJsonDocument&
JsonPool::acquireXLarge()
{
    return acquire(Size::XLARGE);
}

void
JsonPool::releaseAll()
{
    small_.clear();
    medium_.clear();
    large_.clear();
    xlarge_.clear();
}