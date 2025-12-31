#include "hardware/DoorContactModule.h"

DoorContactModule::DoorContactModule(
    uint8_t pin, bool activeLow, uint32_t debounceMs, bool usePullup
)
    : pin_(pin), activeLow_(activeLow), debounceMs_(debounceMs), usePullup_(usePullup)
{
}

void
DoorContactModule::setCallback(DoorContactCallback cb)
{
    cb_ = std::move(cb);
}

bool
DoorContactModule::isOpen() const
{
    return isOpen_;
}

bool
DoorContactModule::readOpenRaw_() const
{
    const int v = digitalRead(pin_);

    bool open = (v == HIGH);
    if (activeLow_)
        open = !open;

    return open;
}

void
DoorContactModule::begin(AppContext&)
{
    pinMode(pin_, usePullup_ ? INPUT_PULLUP : INPUT);

    isOpen_ = readOpenRaw_();
    lastRawOpen_ = isOpen_;
    lastChangeMs_ = millis();
}

void
DoorContactModule::loop(AppContext&)
{
    const bool rawOpen = readOpenRaw_();

    if (rawOpen != lastRawOpen_)
    {
        lastRawOpen_ = rawOpen;
        lastChangeMs_ = millis();
        return;
    }

    if (rawOpen == isOpen_)
        return;

    if ((uint32_t)(millis() - lastChangeMs_) < debounceMs_)
        return;

    isOpen_ = rawOpen;
    if (cb_)
        cb_(isOpen_);
}
