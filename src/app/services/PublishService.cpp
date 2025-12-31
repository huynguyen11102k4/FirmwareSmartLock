#include "app/services/PublishService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"

#include <ArduinoJson.h>

namespace
{
String
maskCode(const String& code)
{
    if (code.isEmpty())
        return "";
    if (code.length() <= 2)
        return "****";
    return String("****") + code.substring(code.length() - 2);
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
    doc["reason"] = reason;

    MqttManager::publish(Topics::state(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), true);
}

void
PublishService::publishLog(const String& ev, const String& method, const String& detail)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(256);
    doc["event"] = ev;
    doc["method"] = method;
    doc["detail"] = detail;
    doc["millis"] = (long)millis();

    MqttManager::publish(Topics::log(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), true);
}

void
PublishService::publishBattery(int percent)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(64);
    doc["battery"] = percent;

    MqttManager::publish(Topics::battery(appState_.mqttTopicPrefix), JsonUtils::serialize(doc));
}

void
PublishService::publishPasscodeList()
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    const String master = passRepo_.getMaster();
    if (master.length() >= 4)
    {
        JsonObject obj = array.createNestedObject();
        obj["code"] = maskCode(master);
        obj["type"] = "permanent";
        obj["validity"] = "";
        obj["status"] = "Active";
    }

    if (passRepo_.hasTemp())
    {
        const PasscodeTemp t = passRepo_.getTemp();
        JsonObject obj = array.createNestedObject();
        obj["code"] = maskCode(t.code);
        obj["type"] = "temp";
        obj["validity"] = String(t.expireAt);
        obj["status"] = t.isExpired(TimeUtils::nowSeconds()) ? "Expired" : "Active";
    }

    MqttManager::publish(
        Topics::passcodesList(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), true
    );
}

void
PublishService::publishICCardList()
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();

    const std::vector<String>& cards = cardRepo_.list();
    for (const String& card : cards)
    {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = card;
        obj["name"] = "Card #" + card.substring(0, 8);
        obj["status"] = "Active";
    }

    MqttManager::publish(
        Topics::iccardsList(appState_.mqttTopicPrefix), JsonUtils::serialize(doc), true
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

    MqttManager::publish(Topics::info(appState_.mqttTopicPrefix), JsonUtils::serialize(info), true);
}
