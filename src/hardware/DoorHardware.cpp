#include "hardware/DoorHardware.h"

#include "config/LockConfig.h"

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

    if (!isOpen && !ctx_->app.doorLock.isLocked())
    {
        const uint32_t delayMs = ctx_->lock.autoRelockDelayMs;

        if (delayMs == 0)
        {
            lock_.lock(*ctx_, "door_closed");
        }
        else
        {
            ctx_->app.doorLock.rearmAutoRelock(delayMs);
            ctx_->publish.publishLog("relock_scheduled", "door_closed", String(delayMs) + "ms");
        }
    }
}