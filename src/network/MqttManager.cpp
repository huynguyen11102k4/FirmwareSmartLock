#include "network/MqttManager.h"

#include "ca_cert.h"
#include "network/RetryPolicy.h"
#include "utils/Logger.h"

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
    if (!success)
    {
        Logger::error("MQTT", "Publish failed to topic: %s", topic.c_str());
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
