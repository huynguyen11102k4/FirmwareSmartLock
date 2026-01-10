#include "hardware/DoorHardware.h"

#include "config/LockConfig.h"
#include "utils/Logger.h"

#define TAG "DOOR_HW"

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
    Logger::info(TAG, "Initializing DoorHardware");

    ctx_ = &ctx;

    lock_.begin(ctx);
    Logger::info(TAG, "DoorLockModule initialized");

    contact_.setCallback([this](bool isOpen) { onDoorContactChanged_(isOpen); });
    contact_.begin(ctx);

    Logger::info(
        TAG, "DoorContact initialized (initial state: %s)", contact_.isOpen() ? "OPEN" : "CLOSED"
    );

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
    {
        Logger::warn(TAG, "requestUnlock ignored (ctx is null)");
        return;
    }

    Logger::info(TAG, "Unlock requested (method=%s)", method.c_str());
    lock_.unlock(*ctx_, method);
}

void
DoorHardware::requestLock(const String& reason)
{
    if (!ctx_)
    {
        Logger::warn(TAG, "requestLock ignored (ctx is null)");
        return;
    }

    Logger::info(TAG, "Lock requested (reason=%s)", reason.c_str());
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
    {
        Logger::warn(TAG, "Door contact changed but ctx is null");
        return;
    }

    Logger::info(TAG, "Door contact changed: %s", isOpen ? "OPEN" : "CLOSED");

    lock_.onDoorContactChanged(isOpen);

    // Door closed while currently unlocked → consider auto-relock
    if (!isOpen && !ctx_->app.doorLock.isLocked())
    {
        const uint32_t delayMs = ctx_->lock.autoRelockDelayMs;

        if (delayMs == 0)
        {
            Logger::info(TAG, "Auto-relock immediately (delay=0)");
            lock_.lock(*ctx_, "door_closed");
        }
        else
        {
            Logger::info(TAG, "Auto-relock scheduled after %d ms", (int)delayMs);

            ctx_->app.doorLock.rearmAutoRelock(delayMs);
            ctx_->publish.publishLog("RelockScheduled", "Device", String(delayMs) + "ms");
        }
    }
}
