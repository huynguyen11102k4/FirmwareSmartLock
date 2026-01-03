#include "app/services/PublishService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"

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

    DynamicJsonDocument doc(256);
    doc["state"] = state;
    doc["source"] = "device";
    doc["method"] = reason;
    doc["ts"] = (long)TimeUtils::nowSeconds();

    MqttManager::publish(Topics::state(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), false);
}

void
PublishService::publishLog(const String& ev, const String& method, const String& detail)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(320);
    doc["event"] = ev;
    doc["method"] = method;
    if (!detail.isEmpty())
        doc["detail"] = detail;
    doc["ts"] = (long)TimeUtils::nowSeconds();

    MqttManager::publish(Topics::log(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), false);
}

void
PublishService::publishBattery(int percent)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(96);
    doc["battery"] = percent;
    doc["ts"] = (long)TimeUtils::nowSeconds();

    MqttManager::publish(Topics::battery(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), false);
}

void
PublishService::publishPasscodeList()
{
    if (!MqttManager::connected())
        return;

    const long ts = (long)TimeUtils::nowSeconds();

    const auto& stored = passRepo_.listItems();
    const bool hasMaster = !isBlank(passRepo_.getMaster());

    const size_t est =
        256 + (stored.size() + (hasMaster ? 1 : 0)) * 128;

    DynamicJsonDocument doc(est > 4096 ? 4096 : est);

    JsonObject root = doc.to<JsonObject>();
    root[AppJsonKeys::TS] = ts;

    JsonArray items = root.createNestedArray(AppJsonKeys::PASSCODES);

    const String master = passRepo_.getMaster();
    if (!isBlank(master))
    {
        JsonObject obj = items.createNestedObject();
        obj["code"] = master;
        obj["type"] = "master";
    }

    for (const auto& p : stored)
    {
        JsonObject obj = items.createNestedObject();
        obj["code"]        = p.code;
        obj["type"]        = p.type;
        obj["effectiveAt"] = (long)p.effectiveAt;
        obj["expireAt"]    = (long)p.expireAt;
    }

    passRepo_.setTs(ts);

    MqttManager::publish(
        Topics::passcodesList(appState_.mqttTopicPrefix),
        JsonUtils::serialize(doc),
        false
    );
}



void
PublishService::publishICCardList()
{
    if (!MqttManager::connected())
        return;

    const long ts = (long)TimeUtils::nowSeconds();

    const auto& cards = cardRepo_.list();
    const size_t est = 256 + cards.size() * 96;
    DynamicJsonDocument doc(est > 4096 ? 4096 : est);

    JsonObject root = doc.to<JsonObject>();
    root[AppJsonKeys::TS] = ts;

    JsonArray items = root.createNestedArray(AppJsonKeys::CARDS);

    for (size_t i = 0; i < cards.size(); i++)
    {
        const auto& c = cards[i];

        JsonObject obj = items.createNestedObject();
        obj["uid"] = c.uid;

        String name = c.name;
        if (isBlank(name))
            name = defaultCardNameByIndex(i);
        obj["name"] = name;
    }

    cardRepo_.setTs(ts);

    MqttManager::publish(
        Topics::iccardsList(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), false
    );
}

void
PublishService::publishInfo(int batteryPercent, int version)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument info(256);
    info["mac"] = appState_.macAddress;
    info["topic"] = appState_.mqttTopicPrefix;
    info["battery"] = batteryPercent;
    info["version"] = version;

    MqttManager::publish(Topics::info(appState_.mqttTopicPrefix), JsonUtils::serialize(info), false);
}
