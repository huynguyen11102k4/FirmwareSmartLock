#pragma once
#include "config/AppConfig.h"
#include "config/AppPaths.h"
#include "config/BleConfig.h"
#include "config/ConfigManager.h"
#include "models/AppState.h"
#include "utils/CommandQueue.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEServer.h>

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

  private:
    void
    setBaseTopicFromConfigOrDefault_();

    bool
    parseBleConfigJsonToAppConfig_(const String& json, AppConfig& outCfg);

    void
    notifyDeviceInfo_();

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