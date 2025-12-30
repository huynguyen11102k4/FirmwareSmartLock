#include "app/services/PublishService.h"

#include <ArduinoJson.h>

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"

PublishService::PublishService(AppState &appState,
                               PasscodeRepository &passRepo,
                               std::vector<String> &iccardsCache)
    : appState_(appState),
      passRepo_(passRepo),
      iccardsCache_(iccardsCache) {}

void PublishService::publishState(const String &state, const String &reason)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(256);
    doc["state"] = state;
    doc["reason"] = reason;

    MqttManager::publish(Topics::state(appState_.baseTopic), JsonUtils::serialize(doc), true);
}

void PublishService::publishLog(const String &ev, const String &method, const String &detail)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(256);
    doc["event"] = ev;
    doc["method"] = method;
    doc["detail"] = detail;
    doc["millis"] = (long)millis();

    MqttManager::publish(Topics::log(appState_.baseTopic), JsonUtils::serialize(doc), true);
}

void PublishService::publishBattery(int percent)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(64);
    doc["battery"] = percent;

    MqttManager::publish(Topics::battery(appState_.baseTopic), JsonUtils::serialize(doc));
}

void PublishService::publishPasscodeList()
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    const String master = passRepo_.getMaster();
    if (master.length() >= 4)
    {
        JsonObject obj = array.createNestedObject();
        obj["code"] = master;
        obj["type"] = "permanent";
        obj["validity"] = "";
        obj["status"] = "Active";
    }

    if (passRepo_.hasTemp())
    {
        PasscodeTemp t = passRepo_.getTemp();
        JsonObject obj = array.createNestedObject();
        obj["code"] = t.code;
        obj["type"] = "temp";
        obj["validity"] = String(t.expireAt);
        obj["status"] = t.isExpired(TimeUtils::nowSeconds()) ? "Expired" : "Active";
    }

    MqttManager::publish(Topics::passcodesList(appState_.baseTopic), JsonUtils::serialize(doc), true);
}

void PublishService::publishICCardList()
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();

    for (const String &card : iccardsCache_)
    {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = card;
        obj["name"] = "Card #" + card.substring(0, 8);
        obj["status"] = "Active";
    }

    MqttManager::publish(Topics::iccardsList(appState_.baseTopic), JsonUtils::serialize(doc), true);
}

void PublishService::publishInfo(int batteryPercent, int version)
{
    if (!MqttManager::connected())
        return;

    DynamicJsonDocument info(256);
    info["mac"] = appState_.deviceMAC;
    info["topic"] = appState_.baseTopic;
    info["battery"] = batteryPercent;
    info["version"] = version;

    MqttManager::publish(Topics::info(appState_.baseTopic), JsonUtils::serialize(info), true);
}