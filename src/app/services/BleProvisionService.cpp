#include "app/services/BleProvisionService.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <ArduinoJson.h>

#include "app/services/MqttService.h"
#include "network/NetworkManager.h"
#include "utils/JsonUtils.h"
#include "config/AppPaths.h"

BleProvisionService::BleProvisionService(AppState& appState, ConfigManager& cfgMgr, MqttService& mqttSvc)
  : appState_(appState), cfgMgr_(cfgMgr), mqttSvc_(mqttSvc) {}

void BleProvisionService::disableIfActive() {
  if (!appState_.flags.bleActive) return;
  appState_.flags.bleActive = false;
  BLEDevice::deinit(true);
  delay(300);
}

void BleProvisionService::setBaseTopicFromConfigOrDefault_() {
  const AppConfig& cfg = cfgMgr_.get();
  appState_.setBaseTopic(cfg.topicPrefix);
}

bool BleProvisionService::parseBleConfigJsonToAppConfig_(const String& json, AppConfig& outCfg) {
  DynamicJsonDocument doc(1024);
  if (!JsonUtils::deserialize(json, doc)) return false;

  outCfg.clear();

  outCfg.wifiSsid = doc[AppJsonKeys::WIFI_SSID] | "";
  outCfg.wifiPass = doc[AppJsonKeys::WIFI_PASS] | "";

  String host = doc[AppJsonKeys::MQTT_HOST] | "";
  int port = (int)(doc[AppJsonKeys::MQTT_PORT] | 8883);

  if (host.isEmpty() && doc.containsKey("mqtt_broker")) {
    String broker = doc["mqtt_broker"] | "";
    int colon = broker.indexOf(':');
    if (colon != -1) {
      host = broker.substring(0, colon);
      port = broker.substring(colon + 1).toInt();
    } else {
      host = broker;
    }
  }

  outCfg.mqttHost = host;
  outCfg.mqttPort = (port > 0 ? port : 8883);

  outCfg.mqttUser = doc[AppJsonKeys::MQTT_USER] | "";
  outCfg.mqttPass = doc[AppJsonKeys::MQTT_PASS] | "";
  outCfg.topicPrefix = doc[AppJsonKeys::TOPIC_PREFIX] | "";

  return outCfg.isValid();
}

void BleProvisionService::ConfigCallback::onWrite(BLECharacteristic* c) {
  std::string value = c->getValue();
  String json(value.c_str());

  AppConfig newCfg;
  if (!svc_.parseBleConfigJsonToAppConfig_(json, newCfg)) {
    svc_.pNotify_->setValue("error: invalid json/config");
    svc_.pNotify_->notify();
    return;
  }

  if (!svc_.cfgMgr_.updateFromBle(newCfg)) {
    svc_.pNotify_->setValue("error: save failed");
    svc_.pNotify_->notify();
    return;
  }

  svc_.setBaseTopicFromConfigOrDefault_();

  svc_.pNotify_->setValue("OK");
  svc_.pNotify_->notify();

  const String clientId = "ESP32DoorLock-" + svc_.appState_.deviceMAC;
  NetworkManager::begin(newCfg, clientId);
  svc_.mqttSvc_.attachCallback();
}

void BleProvisionService::begin() {
  appState_.flags.bleActive = true;

  BLEDevice::init(BLE_DEVICE_NAME);

  BLEServer* serverBle = BLEDevice::createServer();
  BLEService* svc = serverBle->createService(BLE_SERVICE_UUID);

  pConfig_ = svc->createCharacteristic(
      BLE_CHAR_CONFIG_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
  pConfig_->addDescriptor(new BLE2902());

  pNotify_ = svc->createCharacteristic(
      BLE_CHAR_NOTIFY_UUID,
      BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE);

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
