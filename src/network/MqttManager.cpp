#include "network/MqttManager.h"

#include "ca_cert.h"
#include "network/RetryPolicy.h"
#include "utils/Logger.h"

#include <ArduinoJson.h>

static WiFiClientSecure secureClient;
static PubSubClient mqtt(secureClient);

static AppConfig config;
static String clientId;

static RetryPolicy retryPolicy(1000, 60000);
static bool initialized = false;

void
MqttManager::begin(const AppConfig& cfg, const String& cid)
{
    config = cfg;
    clientId = cid;

    setupClient();
    initialized = true;
    retryPolicy.reset();
}

void
MqttManager::setupClient()
{
    secureClient.setCACert(hivemq_root_ca);
    secureClient.setTimeout(20);
    secureClient.setHandshakeTimeout(30);

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(20);
    mqtt.setBufferSize(2048);
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
    {
        retryPolicy.reset();
        return;
    }

    if (!config.hasMqtt())
        return;

    if (!retryPolicy.shouldRetry())
        return;

    Logger::info(
        "MQTT", "Connecting to %s:%d (attempt %d, delay %dms)", config.mqttHost.c_str(),
        config.mqttPort, retryPolicy.getAttemptCount() + 1, (int)retryPolicy.getCurrentDelay()
    );

    retryPolicy.recordAttempt();

    if (!mqtt.connect(clientId.c_str(), config.mqttUser.c_str(), config.mqttPass.c_str()))
    {
        Logger::error("MQTT", "Connect failed rc=%d", mqtt.state());

        if (retryPolicy.getAttemptCount() >= 5)
        {
            Logger::warn("MQTT", "Next retry in %ds", (int)(retryPolicy.getCurrentDelay() / 1000));
        }
        return;
    }

    Logger::info("MQTT", "Connected successfully");
    retryPolicy.reset();
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

    const bool success = mqtt.publish(topic.c_str(), payload.c_str(), retained);

    if (success)
    {
        Logger::info(
            "MQTT", "Publish OK topic=%s size=%d retained=%d", topic.c_str(), payload.length(),
            retained
        );

        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (err)
        {
            Logger::warn("MQTT", "Payload is not valid JSON (%s), raw:", err.c_str());
            Logger::info("MQTT", "%s", payload.c_str());
        }
        else
        {
            String pretty;
            serializeJsonPretty(doc, pretty);
            Logger::info("MQTT", "Payload:\n%s", pretty.c_str());
        }
    }
    else
    {
        Logger::error(
            "MQTT", "Publish FAILED topic=%s size=%d retained=%d state=%d", topic.c_str(),
            payload.length(), retained, mqtt.state()
        );
    }

    return success;
}

void
MqttManager::subscribe(const String& topic, int qos)
{
    if (!mqtt.connected())
        return;

    const bool success = mqtt.subscribe(topic.c_str(), qos);
    if (success)
    {
        Logger::info("MQTT", "Subscribed: %s", topic.c_str());
    }
    else
    {
        Logger::error("MQTT", "Subscribe failed: %s", topic.c_str());
    }
}

int
MqttManager::getRetryAttempts()
{
    return retryPolicy.getAttemptCount();
}
