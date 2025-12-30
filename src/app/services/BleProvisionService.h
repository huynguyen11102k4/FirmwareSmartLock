#pragma once
#include <Arduino.h>

#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEServer.h>

#include "config/AppConfig.h"
#include "config/BleConfig.h"
#include "config/ConfigManager.h"
#include "models/AppState.h"

class MqttService;

class BleProvisionService {
public:
  BleProvisionService(AppState& appState, ConfigManager& cfgMgr, MqttService& mqttSvc);

  void begin();
  void disableIfActive();

private:
  bool parseBleConfigJsonToAppConfig_(const String& json, AppConfig& outCfg);
  void setBaseTopicFromConfigOrDefault_();

  class ConfigCallback final : public BLECharacteristicCallbacks {
  public:
    explicit ConfigCallback(BleProvisionService& svc) : svc_(svc) {}
    void onWrite(BLECharacteristic* c) override;

  private:
    BleProvisionService& svc_;
  };

  AppState& appState_;
  ConfigManager& cfgMgr_;
  MqttService& mqttSvc_;

  BLECharacteristic* pConfig_{nullptr};
  BLECharacteristic* pNotify_{nullptr};
};
