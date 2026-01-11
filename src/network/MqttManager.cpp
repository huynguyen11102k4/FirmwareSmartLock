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
    secureClient.setTimeout(30);
    secureClient.setHandshakeTimeout(30);

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(30);
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
    {
        Logger::warn("MQTT", "Publish skipped - not connected");
        return false;
    }

    const int stateBefore = mqtt.state();

    const bool success = mqtt.publish(topic.c_str(), payload.c_str(), retained);

    const int stateAfter = mqtt.state();
    const bool connectedAfter = mqtt.connected();

    if (success)
    {
        Logger::info(
            "MQTT", "Publish OK topic=%s size=%d retained=%d", topic.c_str(), payload.length(),
            retained
        );

        if (stateBefore != stateAfter)
        {
            Logger::warn(
                "MQTT", "State changed after publish: %d -> %d, connected=%d",
                stateBefore, stateAfter, connectedAfter
            );
        }

        if (payload.length() > 0 && (payload[0] == '{' || payload[0] == '['))
        {
            StaticJsonDocument<1024> doc;
            DeserializationError err = deserializeJson(doc, payload);

            if (!err)
            {
                String pretty;
                serializeJsonPretty(doc, pretty);
                Logger::info("MQTT", "Payload:\n%s", pretty.c_str());
            }
            else
            {
                Logger::warn("MQTT", "JSON parse failed (%s), raw: %s", err.c_str(), payload.c_str());
            }
        }
        else
        {
            // Plain text payload
            Logger::info("MQTT", "Payload: %s", payload.c_str());
        }
    }
    else
    {
        Logger::error(
            "MQTT", "Publish FAILED topic=%s size=%d retained=%d state=%d->%d", topic.c_str(),
            payload.length(), retained, stateBefore, stateAfter
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

class StringPrinter : public Print
{
public:
    String buffer;
    
    size_t write(uint8_t c) override
    {
        buffer += (char)c;
        return 1;
    }
    
    size_t write(const uint8_t* buf, size_t size) override
    {
        buffer.reserve(buffer.length() + size);
        for (size_t i = 0; i < size; i++)
        {
            buffer += (char)buf[i];
        }
        return size;
    }
};

bool
MqttManager::publishStream(
    const String& topic,
    std::function<void(Print&)> writer,
    bool retained
)
{
    if (!mqtt.connected())
    {
        Logger::error("MQTT", "PublishStream FAILED - not connected");
        return false;
    }

    StringPrinter printer;
    
    try
    {
        writer(printer);
    }
    catch (...)
    {
        Logger::error("MQTT", "Writer callback EXCEPTION");
        return false;
    }

    const String& payload = printer.buffer;

    if (payload.length() == 0)
    {
        Logger::warn("MQTT", "PublishStream empty payload topic=%s", topic.c_str());
        return false;
    }

    if (payload.length() > 4096)
    {
        Logger::error(
            "MQTT", "PublishStream payload TOO LARGE (%d bytes) topic=%s", 
            payload.length(), topic.c_str()
        );
        return false;
    }

    Logger::info(
        "MQTT", "PublishStream topic=%s size=%d retained=%d", 
        topic.c_str(), payload.length(), retained
    );

    const bool success = publish(topic, payload, false);

    if (!success)
    {
        Logger::error("MQTT", "PublishStream FAILED topic=%s state=%d", topic.c_str(), mqtt.state());
        return false;
    }

    Logger::info("MQTT", "PublishStream OK topic=%s", topic.c_str());

    return true;
}