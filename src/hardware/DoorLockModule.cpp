#include "hardware/DoorLockModule.h"

#include "models/DeviceState.h"
#include "models/MqttContract.h"

static constexpr uint8_t LOCK_ANGLE = 0;
static constexpr uint8_t UNLOCK_ANGLE = 90;

DoorLockModule::DoorLockModule(Servo& servo, uint8_t ledPin, uint8_t servoPin)
    : servo_(servo), ledPin_(ledPin), servoPin_(servoPin)
{
}

void
DoorLockModule::onDoorContactChanged(bool isOpen)
{
    isDoorContactOpen_ = isOpen;
}

void
DoorLockModule::begin(AppContext&)
{
    pinMode(ledPin_, OUTPUT);
    digitalWrite(ledPin_, LOW);

    servo_.attach(servoPin_);
    servo_.write(LOCK_ANGLE);
}

void
DoorLockModule::unlock(AppContext& ctx, const String& method)
{
    digitalWrite(ledPin_, HIGH);
    servo_.write(UNLOCK_ANGLE);

    ctx.app.doorLock.unlock();
    ctx.app.deviceState.setDoorState(DoorState::UNLOCKED);

    ctx.publish.publishState(MqttDoorState::UNLOCKED, method);
    ctx.publish.publishLog(MqttDoorEvent::UNLOCK, method, "");
}

void
DoorLockModule::lock(AppContext& ctx, const String& reason)
{
    servo_.write(LOCK_ANGLE);
    digitalWrite(ledPin_, LOW);

    ctx.app.doorLock.lock();
    ctx.app.deviceState.setDoorState(DoorState::LOCKED);

    ctx.publish.publishState(MqttDoorState::LOCKED, reason);
    ctx.publish.publishLog(MqttDoorEvent::LOCK, reason, "");
}

void
DoorLockModule::handleAutoRelock_(AppContext& ctx)
{
    if (isDoorContactOpen_)
        return;

    if (!ctx.app.doorLock.shouldAutoRelock())
        return;

    servo_.write(LOCK_ANGLE);
    digitalWrite(ledPin_, LOW);

    ctx.app.doorLock.lock();
    ctx.app.deviceState.setDoorState(DoorState::LOCKED);

    ctx.publish.publishState(MqttDoorState::LOCKED, MqttSource::AUTO);
    ctx.publish.publishLog(MqttDoorEvent::LOCK, MqttSource::AUTO, "");
}

void
DoorLockModule::loop(AppContext& ctx)
{
    handleAutoRelock_(ctx);
}