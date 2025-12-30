#include "app/services/RfidService.h"

#include <SPI.h>

#include "app/services/Topics.h"
#include "network/MqttManager.h"
#include "config/HardwarePins.h"

RfidService::RfidService(AppState& appState,
                         CardRepository& cardRepo,
                         std::vector<String>& iccardsCache,
                         PublishService& publish,
                         void* ctx,
                         SyncFn syncCache,
                         UnlockFn onUnlock)
  : appState_(appState),
    cardRepo_(cardRepo),
    iccardsCache_(iccardsCache),
    publish_(publish),
    ctx_(ctx),
    syncCache_(syncCache),
    onUnlock_(onUnlock),
    mfrc522_(SS_PIN, RST_PIN) {}

void RfidService::begin() {
  SPI.begin();
  mfrc522_.PCD_Init();
}

String RfidService::getUID_() {
  String uid = "";
  for (byte i = 0; i < mfrc522_.uid.size; i++) {
    if (mfrc522_.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522_.uid.uidByte[i], HEX);
  }
  uid.replace(":", "");
  uid.toUpperCase();
  return uid;
}

void RfidService::loop() {
  if (appState_.flags.swipeAddMode && appState_.swipeAdd.isTimeout()) {
    appState_.flags.swipeAddMode = false;
    appState_.swipeAdd.reset();
    MqttManager::publish(Topics::iccardsStatus(appState_.baseTopic), "swipe_add_timeout");
  }

  if (!mfrc522_.PICC_IsNewCardPresent() || !mfrc522_.PICC_ReadCardSerial()) return;

  const String uid = getUID_();

  if (appState_.flags.swipeAddMode) {
    if (!appState_.swipeAdd.hasFirstSwipe()) {
      appState_.swipeAdd.recordFirstSwipe(uid);
      MqttManager::publish(Topics::iccardsStatus(appState_.baseTopic), "swipe_first_detected");
    } else if (appState_.swipeAdd.matchesFirstSwipe(uid)) {
      if (cardRepo_.add(uid)) {
        if (syncCache_) syncCache_(ctx_);
        publish_.publishICCardList();
      }

      appState_.flags.swipeAddMode = false;
      appState_.swipeAdd.reset();
      MqttManager::publish(Topics::iccardsStatus(appState_.baseTopic), "swipe_add_success");
      publish_.publishLog("card_added", "swipe", uid);
    } else {
      appState_.flags.swipeAddMode = false;
      appState_.swipeAdd.reset();
      MqttManager::publish(Topics::iccardsStatus(appState_.baseTopic), "swipe_add_failed");
    }
  }

  if (cardRepo_.exists(uid)) {
    if (onUnlock_) onUnlock_(ctx_, "card");
  } else if (iccardsCache_.empty()) {
    if (cardRepo_.add(uid)) {
      if (syncCache_) syncCache_(ctx_);
      publish_.publishICCardList();
    }
  }

  publish_.publishLog("card_scan", "card", uid);
  mfrc522_.PICC_HaltA();
}