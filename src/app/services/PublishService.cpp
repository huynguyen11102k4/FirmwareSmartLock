#include "app/services/PublishService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"

#include <ArduinoJson.h>

namespace
{
static String
defaultCardNameByIndex(size_t idx)
{
    String s("ICCard");
    s += String((uint32_t)(idx + 1));
    return s;
}

String
defaultCardNameFromUid(const String& uid)
{
    if (uid.isEmpty())
        return "ICCard";
    const String head = uid.length() >= 8 ? uid.substring(0, 8) : uid;
    return "ICCard" + head;
}

bool
isBlank(const String& s)
{
    for (size_t i = 0; i < s.length(); i++)
    {
        if (!isSpace(s[i]))
            return false;
    }
    return true;
}
} // namespace

PublishService::PublishService(
    AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo
)
    : appState_(appState), passRepo_(passRepo), cardRepo_(cardRepo)
{
}

void
PublishService::publishState(const String& state, const String& reason)
{
    if (!MqttManager::connected())
        return;

    MqttManager::publishStream(
        Topics::state(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            StaticJsonDocument<128> doc;

            doc["state"] = state;
            doc["source"] = "device";
            doc["method"] = reason;
            doc["ts"] = (uint64_t)TimeUtils::nowSeconds();

            serializeJson(doc, out);
        },
        false
    );
}


void
PublishService::publishLog(const String& ev, const String& method, const String& detail)
{
    if (!MqttManager::connected())
        return;

    MqttManager::publishStream(
        Topics::log(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            StaticJsonDocument<192> doc;

            doc["event"] = ev;
            doc["method"] = method;
            if (!detail.isEmpty())
                doc["detail"] = detail;

            doc["ts"] = (uint64_t)TimeUtils::nowSeconds();

            serializeJson(doc, out);
        },
        false
    );
}

void
PublishService::publishBattery(int percent)
{
    if (!MqttManager::connected())
        return;

    MqttManager::publishStream(
        Topics::battery(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            StaticJsonDocument<64> doc;

            doc["battery"] = percent;
            doc["ts"] = (uint64_t)TimeUtils::nowSeconds();

            serializeJson(doc, out);
        },
        false
    );
}

void
PublishService::publishPasscodeList()
{
    if (!MqttManager::connected())
        return;

    const long ts = (uint64_t)TimeUtils::nowSeconds();

    const auto& stored = passRepo_.listItems();
    const String master = passRepo_.getMaster();
    const bool hasMaster = !isBlank(master);

    const size_t itemCount = stored.size() + (hasMaster ? 1 : 0);
    const size_t requiredSize = 128 + (itemCount * 120);

    Logger::info("PUBLISH", "publishPasscodeList: items=%d, buffer=%d bytes", 
                itemCount, requiredSize);

    MqttManager::publishStream(
        Topics::passcodesList(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            DynamicJsonDocument doc(requiredSize);

            doc[AppJsonKeys::TS] = ts;
            JsonArray items = doc.createNestedArray(AppJsonKeys::PASSCODES);

            if (hasMaster)
            {
                JsonObject o = items.createNestedObject();
                o["code"] = master;
                o["type"] = "master";
            }

            for (const auto& p : stored)
            {
                JsonObject o = items.createNestedObject();
                o["code"] = p.code;
                o["type"] = p.type;
                o["effectiveAt"] = (uint64_t)p.effectiveAt;
                o["expireAt"] = (uint64_t)p.expireAt;
            }

            if (doc.overflowed())
            {
                Logger::error("PUBLISH", "JSON buffer overflow! Required: %d", requiredSize);
                return;
            }

            serializeJson(doc, out);

            Logger::info("PUBLISH", "JSON size: %d bytes, buffer: %d bytes", 
                        measureJson(doc), requiredSize);
        },
        false
    );

    passRepo_.setTs(ts);
}

void
PublishService::publishICCardList()
{
    if (!MqttManager::connected())
        return;

    const long ts = (uint64_t)TimeUtils::nowSeconds();
    const auto& cards = cardRepo_.list();

    const size_t itemCount = cards.size();
    const size_t requiredSize = 128 + (itemCount * 120);

    Logger::info(
        "PUBLISH",
        "publishICCardList: items=%d, buffer=%d bytes",
        itemCount,
        requiredSize
    );

    MqttManager::publishStream(
        Topics::iccardsList(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            DynamicJsonDocument doc(requiredSize);

            doc[AppJsonKeys::TS] = ts;
            JsonArray items = doc.createNestedArray(AppJsonKeys::CARDS);

            for (size_t i = 0; i < cards.size(); i++)
            {
                const auto& c = cards[i];

                JsonObject o = items.createNestedObject();
                o["uid"] = c.uid;

                String name = c.name;
                if (isBlank(name))
                    name = defaultCardNameByIndex(i);

                o["name"] = name;
            }

            if (doc.overflowed())
            {
                Logger::error(
                    "PUBLISH",
                    "ICCard JSON buffer overflow! Required: %d",
                    requiredSize
                );
                return;
            }

            serializeJson(doc, out);

            Logger::info(
                "PUBLISH",
                "ICCard JSON size: %d bytes, buffer: %d bytes",
                measureJson(doc),
                requiredSize
            );
        },
        false
    );

    cardRepo_.setTs(ts);
}


void
PublishService::publishInfo(int batteryPercent, int version)
{
    if (!MqttManager::connected())
        return;

    MqttManager::publishStream(
        Topics::info(appState_.mqttTopicPrefix),
        [&](Print& out)
        {
            StaticJsonDocument<128> doc;

            doc["mac"] = appState_.macAddress;
            doc["topic"] = appState_.mqttTopicPrefix;
            doc["battery"] = batteryPercent;
            doc["version"] = version;

            serializeJson(doc, out);
        },
        false
    );
}
