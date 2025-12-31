#include "hardware/DoorLockModule.h"

#include "models/AppState.h"
#include "models/DeviceState.h"
#include "models/DoorLockState.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"

static constexpr uint8_t LOCK_ANGLE = 0;
static constexpr uint8_t UNLOCK_ANGLE = 90;

struct AppContext
{
    AppState* app;
    void (*publishState)(const String&, const String&);
    void (*publishLog)(const String&, const String&, const String&);
};

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

    ctx.app->doorLock.unlock();
    ctx.app->deviceState.setDoorState(DoorState::UNLOCKED);

    ctx.publishState(MqttDoorState::UNLOCKED, method);
    ctx.publishLog(MqttDoorEvent::UNLOCK, method, "");
}

void
DoorLockModule::lock(AppContext& ctx, const String& method)
{
    servo_.write(LOCK_ANGLE);
    digitalWrite(ledPin_, LOW);

    ctx.app->doorLock.lock();
    ctx.app->deviceState.setDoorState(DoorState::LOCKED);

    ctx.publishState(MqttDoorState::LOCKED, method);
    ctx.publishLog(MqttDoorEvent::LOCK, method, "");
}

void
DoorLockModule::handleAutoRelock_(AppContext& ctx)
{
    if (isDoorContactOpen_)
        return;

    if (ctx.app->doorLock.shouldAutoRelock())
    {
        servo_.write(LOCK_ANGLE);
        digitalWrite(ledPin_, LOW);

        ctx.app->doorLock.lock();
        ctx.app->deviceState.setDoorState(DoorState::LOCKED);

        ctx.publishState(MqttDoorState::LOCKED, MqttSource::AUTO);
        ctx.publishLog(MqttDoorEvent::LOCK, MqttSource::AUTO, "");
    }
}

void
DoorLockModule::loop(AppContext& ctx)
{
    handleAutoRelock_(ctx);
}