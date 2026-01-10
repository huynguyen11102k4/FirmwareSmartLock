    #include "app/services/BleProvisionService.h"

    #include "models/Command.h"
    #include "network/MqttManager.h"
    #include "utils/JsonUtils.h"
    #include "utils/Logger.h"

    #include <ArduinoJson.h>
    #include <BLEDevice.h>
    #include <BLEUtils.h>

    BleProvisionService::BleProvisionService(
        AppState& appState, ConfigManager& cfgMgr, CommandQueue& cmdQueue
    )
        : appState_(appState), cfgMgr_(cfgMgr), cmdQueue_(cmdQueue)
    {
    }

    void
    BleProvisionService::disableIfActive()
    {
        if (!appState_.runtimeFlags.bleActive)
            return;

        forceCleanup();
    }

    void 
    BleProvisionService::forceCleanup()
    {
        appState_.runtimeFlags.bleActive = false;
        
        if (pConfig_)
        {
            pConfig_->setCallbacks(nullptr);
        }
        if (serverBle_)
        {
            serverBle_->setCallbacks(nullptr);
        }
        
        BLEAdvertising* adv = BLEDevice::getAdvertising();
        if (adv)
        {
            adv->stop();
        }
        
        if (serverBle_)
        {
            serverBle_->disconnect(serverBle_->getConnId());
        }
        
        BLEDevice::deinit(true);
        
        delay(500);
        
        serverBle_ = nullptr;
        pConfig_ = nullptr;
        pNotify_ = nullptr;
        
        Logger::info("BLE", "Cleanup complete. Free heap: %d", ESP.getFreeHeap());
    }

    bool
    BleProvisionService::parseBleConfigJsonToAppConfig_(const String& json, AppConfig& outCfg)
    {
        DynamicJsonDocument doc(512);
        if (!JsonUtils::deserialize(json, doc))
            return false;

        outCfg.clear(AppConfig::ClearMode::WIFI_ONLY);

        outCfg.wifiSsid = doc[AppJsonKeys::WIFI_SSID] | "";
        outCfg.wifiPass = doc[AppJsonKeys::WIFI_PASS] | "";

        return outCfg.hasWifi();
    }

    void
    BleProvisionService::ServerCallback::onConnect(BLEServer*)
    {
        svc_.appState_.deviceState.setBleState(true);
        Logger::info("BLE", "Client connected");
    }

    void
    BleProvisionService::ServerCallback::onDisconnect(BLEServer*)
    {
        svc_.appState_.deviceState.setBleState(false);
        Logger::info("BLE", "Client disconnected");

        BLEAdvertising* adv = BLEDevice::getAdvertising();
        if (adv)
            adv->start();
    }

    void
    BleProvisionService::ConfigCallback::onWrite(BLECharacteristic* c)
    {
        const std::string value = c->getValue();
        const String json(value.c_str());

        AppConfig wifiOnly;
        if (!svc_.parseBleConfigJsonToAppConfig_(json, wifiOnly))
        {
            svc_.pNotify_->setValue("error: invalid wifi json");
            svc_.pNotify_->notify();
            return;
        }

        if (!svc_.cfgMgr_.updateFromBle(wifiOnly))
        {
            svc_.pNotify_->setValue("error: save failed");
            svc_.pNotify_->notify();
            return;
        }
        
        svc_.pNotify_->setValue("OK");
        svc_.pNotify_->notify();

        svc_.appState_.wifiProvision.startWaiting();

        Command cmd;
        cmd.type = CommandType::APPLY_CONFIG;
        cmd.source = "ble";
        cmd.payload = "";

        if (!svc_.cmdQueue_.enqueue(cmd))
        {
            Logger::error("BLE", "Failed to enqueue APPLY_CONFIG command");
            return;
        }
        
        Logger::info("BLE", "WiFi config saved, waiting for connection validation...");
    }

    void
    BleProvisionService::begin()
    {
        appState_.runtimeFlags.bleActive = true;

        BLEDevice::init(BLE_DEVICE_NAME);

        serverBle_ = BLEDevice::createServer();
        serverBle_->setCallbacks(new ServerCallback(*this));

        BLEService* svc = serverBle_->createService(BLE_SERVICE_UUID);

        pConfig_ = svc->createCharacteristic(
            BLE_CHAR_CONFIG_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
        );
        pConfig_->addDescriptor(new BLE2902());

        pNotify_ = svc->createCharacteristic(
            BLE_CHAR_NOTIFY_UUID,
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE
        );

        auto* cccd = new BLE2902();
        cccd->setNotifications(true);
        cccd->setIndications(false);
        pNotify_->addDescriptor(cccd);

        pConfig_->setCallbacks(new ConfigCallback(*this));
        svc->start();

        BLEAdvertising* adv = BLEDevice::getAdvertising();
        adv->addServiceUUID(BLE_SERVICE_UUID);
        adv->setScanResponse(true);
        adv->setMinPreferred(0x06);
        adv->setMinPreferred(0x12);
        adv->start();
    }