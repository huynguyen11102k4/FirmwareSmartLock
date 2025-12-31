#include "hardware/DoorContactModule.h"

DoorContactModule::DoorContactModule(uint8_t pin, bool activeLow, uint32_t debounceMs)
    : pin_(pin), activeLow_(activeLow), debounceMs_(debounceMs)
{
}

void
DoorContactModule::setCallback(DoorContactCallback cb)
{
    cb_ = cb;
}

bool
DoorContactModule::readRaw_() const
{
    int v = digitalRead(pin_);
    bool open = (v == HIGH);
    if (activeLow_)
        open = !open;
    return open;
}

bool
DoorContactModule::isOpen() const
{
    return isOpen_;
}

void
DoorContactModule::begin(AppContext&)
{
    pinMode(pin_, INPUT_PULLUP);
    isOpen_ = readRaw_();
    lastRawOpen_ = isOpen_;
    lastChangeMs_ = millis();
}

void
DoorContactModule::loop(AppContext&)
{
    const bool rawOpen = readRaw_();
    if (rawOpen != lastRawOpen_)
    {
        lastRawOpen_ = rawOpen;
        lastChangeMs_ = millis();
        return;
    }

    if (rawOpen == isOpen_)
        return;

    if (millis() - lastChangeMs_ >= debounceMs_)
    {
        isOpen_ = rawOpen;
        if (cb_)
            cb_(isOpen_);
    }
}