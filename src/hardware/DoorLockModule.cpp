#include "hardware/DoorLockModule.h"

#include "app/AppContext.h"
#include "config/LockConfig.h"
#include "models/DeviceState.h"
#include "models/MqttContract.h"
#include "utils/Logger.h"

#include "models/AppState.h"
#include "app/services/PublishService.h"

static constexpr uint8_t LOCK_ANGLE = 0;
static constexpr uint8_t UNLOCK_ANGLE = 90;

#define TAG "DOOR_LOCK"

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
    Logger::info(TAG, "UNLOCK requested (method=%s)", method.c_str());

    servo_.write(UNLOCK_ANGLE);

    autoRelockAtMs_ = millis() + 15000;
    autoRelockArmed_ = true;

    ledBlinking_ = true;
    ledLastToggleMs_ = 0;
    ledState_ = false;
    digitalWrite(ledPin_, LOW);

    ctx.app.doorLock.unlock(15000);
    ctx.app.deviceState.setDoorState(DoorState::UNLOCKED);

    ctx.publish.publishState(MqttDoorState::UNLOCKED, method);
    ctx.publish.publishLog(MqttDoorEvent::DOOR_UNLOCKED, method, "");
}


void
DoorLockModule::lock(AppContext& ctx, const String& reason)
{
    Logger::info(TAG, "LOCK requested (reason=%s)", reason.c_str());

    servo_.write(LOCK_ANGLE);

    ledBlinking_ = false;
    ledState_ = false;
    digitalWrite(ledPin_, LOW);

    ctx.app.doorLock.lock();
    ctx.app.deviceState.setDoorState(DoorState::LOCKED);

    ctx.publish.publishState(MqttDoorState::LOCKED, reason);
    ctx.publish.publishLog(MqttDoorEvent::DOOR_LOCKED, reason, "");
}

void
DoorLockModule::handleAutoRelock_(AppContext& ctx)
{
    if (isDoorContactOpen_)
    {
        Logger::info(TAG, "AutoRelock SKIPPED: door is OPEN");
        return;
    }

    if (!ctx.app.doorLock.shouldAutoRelock())
    {
        Logger::debug(TAG, "AutoRelock not due yet");
        return;
    }

    Logger::info(TAG, "AutoRelock EXECUTE");

    servo_.write(LOCK_ANGLE);
    digitalWrite(ledPin_, LOW);

    Logger::info(TAG, "Servo moved to LOCK by AUTO, LED OFF");

    ctx.app.doorLock.lock();
    ctx.app.deviceState.setDoorState(DoorState::LOCKED);

    ctx.publish.publishState(MqttDoorState::LOCKED, MqttSource::AUTO);
    ctx.publish.publishLog(MqttDoorEvent::DOOR_LOCKED, MqttSource::AUTO, "");
}

void
DoorLockModule::loop(AppContext& ctx)
{
    const unsigned long now = millis();

    if (ledBlinking_)
    {
        if (now - ledLastToggleMs_ >= 1000)
        {
            ledLastToggleMs_ = now;
            ledState_ = !ledState_;
            digitalWrite(ledPin_, ledState_ ? HIGH : LOW);
        }
    }

    if (!autoRelockArmed_)
        return;

    if ((int32_t)(now - autoRelockAtMs_) < 0)
        return;

    Logger::info(TAG, "AutoRelock EXECUTE");

    servo_.write(LOCK_ANGLE);
    digitalWrite(ledPin_, LOW);

    ledBlinking_ = false;
    autoRelockArmed_ = false;

    ctx.app.doorLock.lock();
    ctx.app.deviceState.setDoorState(DoorState::LOCKED);

    ctx.publish.publishState(MqttDoorState::LOCKED, MqttSource::AUTO);
    ctx.publish.publishLog(MqttDoorEvent::DOOR_LOCKED, MqttSource::AUTO, "");
}
