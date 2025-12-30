#include "hardware/DoorLockModule.h"

// These are from your project includes.
#include "models/AppState.h"
#include "models/DoorLockState.h"
#include "models/DeviceState.h"
#include "utils/JsonUtils.h"
#include "network/MqttManager.h"

struct AppContext {
  AppState* app;
  void (*publishState)(const String&, const String&);
  void (*publishLog)(const String&, const String&, const String&);
};

DoorLockModule::DoorLockModule(Servo& servo, uint8_t ledPin, uint8_t servoPin)
  : servo_(servo), ledPin_(ledPin), servoPin_(servoPin) {}

void DoorLockModule::setDoorOpen(bool open) { doorOpen_ = open; }

void DoorLockModule::begin(AppContext&) {
  pinMode(ledPin_, OUTPUT);
  digitalWrite(ledPin_, LOW);
  servo_.attach(servoPin_);
  servo_.write(0);
}

void DoorLockModule::beginUnlock(AppContext& ctx, const String& method) {
  digitalWrite(ledPin_, HIGH);
  servo_.write(90);

  ctx.app->doorLock.unlock();
  ctx.app->device.setDoorState(DoorState::UNLOCKED);

  ctx.publishState("unlocked", method);
  ctx.publishLog("unlock", method, "");
}

void DoorLockModule::forceLock(AppContext& ctx, const String& reason) {
  servo_.write(0);
  digitalWrite(ledPin_, LOW);

  ctx.app->doorLock.lock();
  ctx.app->device.setDoorState(DoorState::LOCKED);

  ctx.publishState("locked", reason);
  ctx.publishLog("lock", reason, "");
}

void DoorLockModule::serviceAutoRelock_(AppContext& ctx) {
  if (doorOpen_) return; // why: avoid locking while door is physically open

  if (ctx.app->doorLock.shouldAutoRelock()) {
    servo_.write(0);
    digitalWrite(ledPin_, LOW);

    ctx.app->doorLock.lock();
    ctx.app->device.setDoorState(DoorState::LOCKED);

    ctx.publishState("locked", "auto");
  }
}

void DoorLockModule::loop(AppContext& ctx) {
  serviceAutoRelock_(ctx);
}