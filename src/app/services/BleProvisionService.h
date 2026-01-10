#pragma once
#include "config/AppConfig.h"
#include "config/AppPaths.h"
#include "config/BleConfig.h"
#include "config/ConfigManager.h"
#include "models/AppState.h"
#include "utils/CommandQueue.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEServer.h>
#include <WiFi.h>

class MqttManager;

class BleProvisionService
{
  public:
    BleProvisionService(AppState& appState, ConfigManager& cfgMgr, CommandQueue& cmdQueue);

    void
    begin();

    void
    disableIfActive();

    void
    forceCleanup();
    
    bool
    isNotifyAvailable() const
    {
        return pNotify_ != nullptr && appState_.deviceState.bleConnected;
    }
    
    void
    notifyProvisionSuccess()
    {
        if (!isNotifyAvailable())
            return;
        
        DynamicJsonDocument doc(512);
        doc["type"] = "provision_success";
        doc["status"] = "connected";
        
        JsonObject deviceInfo = doc.createNestedObject("device_info");
        deviceInfo["macAddress"] = appState_.macAddress;
        deviceInfo["doorCode"] = appState_.doorCode;
        deviceInfo["doorName"] = appState_.doorName;
        deviceInfo["mqttTopicPrefix"] = appState_.mqttTopicPrefix;
        
        deviceInfo["wifiConnected"] = true;
        deviceInfo["wifiIP"] = WiFi.localIP().toString();
        
        String payload;
        serializeJson(doc, payload);
        
        
        pNotify_->setValue(payload.c_str());
        pNotify_->notify();
    }
    
    void
    notifyProvisionFailed(const String& reason)
    {
        if (!isNotifyAvailable())
            return;
        
        DynamicJsonDocument doc(256);
        doc["type"] = "provision_failed";
        doc["reason"] = reason;
        doc["status"] = "failed";
        
        String payload;
        serializeJson(doc, payload);
        
        pNotify_->setValue(payload.c_str());
        pNotify_->notify();
    }

  private:
    void
    setBaseTopicFromConfigOrDefault_();

    bool
    parseBleConfigJsonToAppConfig_(const String& json, AppConfig& outCfg);
    
    class ConfigCallback final : public BLECharacteristicCallbacks
    {
      public:
        explicit ConfigCallback(BleProvisionService& svc) : svc_(svc)
        {
        }
        void
        onWrite(BLECharacteristic* c) override;

      private:
        BleProvisionService& svc_;
    };

    class ServerCallback final : public BLEServerCallbacks
    {
      public:
        explicit ServerCallback(BleProvisionService& svc) : svc_(svc)
        {
        }

        void
        onConnect(BLEServer*) override;

        void
        onDisconnect(BLEServer*) override;

      private:
        BleProvisionService& svc_;
    };

    AppState& appState_;
    ConfigManager& cfgMgr_;
    CommandQueue& cmdQueue_;

    BLEServer* serverBle_{nullptr};
    BLECharacteristic* pConfig_{nullptr};
    BLECharacteristic* pNotify_{nullptr};
};