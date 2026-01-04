#include "config/ConfigRepository.h"

#include "config/AppPaths.h"
#include "storage/FileSystem.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>

bool
ConfigRepository::exists() const
{
    return FileSystem::exists(AppPaths::CONFIG_JSON);
}

static void
fillMqttDefaults(AppConfig& cfg)
{
    cfg.mqttHost = MqttDefaults::HOST;
    cfg.mqttPort = MqttDefaults::PORT;
    cfg.mqttUser = MqttDefaults::USER;
    cfg.mqttPass = MqttDefaults::PASS;
    
}

bool
ConfigRepository::load(AppConfig& cfg)
{
    AppConfig temp;
    temp.clear();
    fillMqttDefaults(temp);
    temp.topicPrefix = "";

    if (!exists())
    {
        temp.wifiSsid = "NguyenTheAnh";
        temp.wifiPass = "theanh010424";
        cfg = temp;
        return true;
    }

    const String json = FileSystem::readFile(AppPaths::CONFIG_JSON);
    DynamicJsonDocument doc(512);

    if (!JsonUtils::deserialize(json, doc))
    {
        cfg = temp;
        return true;
    }

    temp.wifiSsid = doc[AppJsonKeys::WIFI_SSID] | "";
    temp.wifiPass = doc[AppJsonKeys::WIFI_PASS] | "";
    temp.topicPrefix = doc[AppJsonKeys::TOPIC_PREFIX] | "";

    cfg = temp;
    return true;
}


bool
ConfigRepository::save(const AppConfig& cfg)
{
    DynamicJsonDocument doc(512);
    doc[AppJsonKeys::WIFI_SSID] = cfg.wifiSsid;
    doc[AppJsonKeys::WIFI_PASS] = cfg.wifiPass;
    doc[AppJsonKeys::MQTT_HOST] = cfg.mqttHost;
    doc[AppJsonKeys::MQTT_PORT] = cfg.mqttPort;
    doc[AppJsonKeys::MQTT_USER] = cfg.mqttUser;
    doc[AppJsonKeys::MQTT_PASS] = cfg.mqttPass;
    doc[AppJsonKeys::TOPIC_PREFIX] = cfg.topicPrefix;

    return FileSystem::writeFile(AppPaths::CONFIG_JSON, JsonUtils::serialize(doc));
}
