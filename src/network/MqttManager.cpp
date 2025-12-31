#include "MqttManager.h"

#include "utils/Logger.h"

#include <ca_cert.h>

static WiFiClientSecure secureClient;
static PubSubClient mqtt(secureClient);

static const AppConfig* config = nullptr;
static String clientId;
static uint32_t lastAttempt = 0;
static bool initialized = false;

void
MqttManager::begin(const AppConfig& cfg, const String& cid)
{
    config = &cfg;
    clientId = cid;

    setupClient();
    initialized = true;
}

void
MqttManager::setupClient()
{
    secureClient.setCACert(hivemq_root_ca);
    secureClient.setTimeout(20);
    secureClient.setHandshakeTimeout(30);

    mqtt.setServer(config->mqttHost.c_str(), config->mqttPort);
    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(20);
    mqtt.setBufferSize(4096);
}

void
MqttManager::setCallback(MqttCallback cb)
{
    mqtt.setCallback(cb);
}

bool
MqttManager::connected()
{
    return mqtt.connected();
}

void
MqttManager::reconnect()
{
    if (!initialized || mqtt.connected())
        return;
    if (millis() - lastAttempt < 5000)
        return;
    lastAttempt = millis();

    Logger::info("MQTT", "Connecting to %s:%d", config->mqttHost.c_str(), config->mqttPort);

    if (!mqtt.connect(clientId.c_str(), config->mqttUser.c_str(), config->mqttPass.c_str()))
    {
        Logger::error("MQTT", "Connect failed rc=%d", mqtt.state());
        return;
    }

    Logger::info("MQTT", "Connected");
}

void
MqttManager::loop()
{
    if (mqtt.connected())
    {
        mqtt.loop();
    }
}

bool
MqttManager::publish(const String& topic, const String& payload, bool retained)
{
    if (!mqtt.connected())
        return false;
    return mqtt.publish(topic.c_str(), payload.c_str(), retained);
}

void
MqttManager::subscribe(const String& topic, int qos)
{
    if (!mqtt.connected())
        return;
    mqtt.subscribe(topic.c_str(), qos);
}
