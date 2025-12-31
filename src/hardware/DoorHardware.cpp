#include "hardware/DoorHardware.h"

DoorHardware::DoorHardware(
    Servo& servo, uint8_t ledPin, uint8_t servoPin, uint8_t contactPin, bool contactActiveLow,
    uint32_t contactDebounceMs, bool contactUsePullup
)
    : lock_(servo, ledPin, servoPin),
      contact_(contactPin, contactActiveLow, contactDebounceMs, contactUsePullup)
{
}

void
DoorHardware::begin(AppContext& ctx)
{
    ctx_ = &ctx;

    lock_.begin(ctx);

    contact_.setCallback([this](bool isOpen) { onDoorContactChanged_(isOpen); });
    contact_.begin(ctx);

    // Sync initial state into lock module
    lock_.onDoorContactChanged(contact_.isOpen());
}

void
DoorHardware::loop(AppContext& ctx)
{
    contact_.loop(ctx);
    lock_.loop(ctx);
}

void
DoorHardware::requestUnlock(const String& method)
{
    if (!ctx_)
        return;
    lock_.unlock(*ctx_, method);
}

void
DoorHardware::requestLock(const String& reason)
{
    if (!ctx_)
        return;
    lock_.lock(*ctx_, reason);
}

bool
DoorHardware::isDoorOpen() const
{
    return contact_.isOpen();
}

void
DoorHardware::onDoorContactChanged_(bool isOpen)
{
    if (!ctx_)
        return;

    lock_.onDoorContactChanged(isOpen);

    // Rule: door CLOSED (magnet touched / button pressed) => LOCK immediately
    if (!isOpen && !ctx_->app.doorLock.isLocked())
    {
        lock_.lock(*ctx_, "door_closed");
    }
}