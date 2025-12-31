#include "app/services/MqttService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/TimeUtils.h"

#include <ArduinoJson.h>

MqttService* MqttService::s_instance_ = nullptr;

MqttService::MqttService(
    AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo,
    std::vector<String>& iccardsCache, PublishService& publish, void* ctx, SyncFn syncIccardsCache,
    BatteryFn readBatteryPercent, UnlockFn onUnlock, LockFn onLock
)
    : appState_(appState), passRepo_(passRepo), cardRepo_(cardRepo), iccardsCache_(iccardsCache),
      publish_(publish), ctx_(ctx), syncIccardsCache_(syncIccardsCache),
      readBatteryPercent_(readBatteryPercent), onUnlock_(onUnlock), onLock_(onLock)
{
}

void
MqttService::attachCallback()
{
    s_instance_ = this;
    MqttManager::setCallback(&MqttService::callbackThunk);
}

void
MqttService::onConnected(int infoVersion)
{
    const String base = appState_.mqttTopicPrefix;

    MqttManager::subscribe(Topics::passcodes(base), 1);
    MqttManager::subscribe(Topics::passcodesReq(base), 1);
    MqttManager::subscribe(Topics::iccards(base), 1);
    MqttManager::subscribe(Topics::iccardsReq(base), 1);
    MqttManager::subscribe(Topics::control(base), 1);

    const int batt = readBatteryPercent_ ? readBatteryPercent_(ctx_) : 0;
    publish_.publishInfo(batt, infoVersion);

    publish_.publishState("locked", "startup");
    publish_.publishBattery(batt);
    publish_.publishPasscodeList();
    publish_.publishICCardList();
}

void
MqttService::callbackThunk(char* topic, byte* payload, unsigned int length)
{
    if (!s_instance_)
        return;

    const String topicStr(topic);

    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++)
        payloadStr += (char)payload[i];

    s_instance_->dispatch_(topicStr, payloadStr);
}

void
MqttService::dispatch_(const String& topicStr, const String& payloadStr)
{
    const String base = appState_.mqttTopicPrefix;

    if (topicStr == Topics::passcodes(base))
        return handlePasscodesTopic_(payloadStr);
    if (topicStr == Topics::passcodesReq(base))
        return publish_.publishPasscodeList();
    if (topicStr == Topics::iccards(base))
        return handleIccardsTopic_(payloadStr);
    if (topicStr == Topics::iccardsReq(base))
        return publish_.publishICCardList();
    if (topicStr == Topics::control(base))
        return handleControlTopic_(payloadStr);
}

void
MqttService::handlePasscodesTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
        return;

    const String action = doc["action"] | "";
    const String type = doc["type"] | "";

    if (action == "add" && type == "permanent")
    {
        const String newCode = doc["code"] | "";
        const String oldCode = doc["old_code"] | "";

        const String current = passRepo_.getMaster();
        const bool hasMaster = current.length() >= 4;

        if (!hasMaster)
        {
            if (newCode.length() < 4 || newCode.length() > 10)
                return;
            passRepo_.setMaster(newCode);
            publish_.publishPasscodeList();
            publish_.publishLog("master_set", "first_time", newCode);
            return;
        }

        if (oldCode.isEmpty() || oldCode != current)
        {
            MqttManager::publish(
                Topics::passcodesError(appState_.mqttTopicPrefix),
                "{\"error\":\"old_master_required\"}"
            );
            return;
        }

        passRepo_.setMaster(newCode);
        publish_.publishPasscodeList();
        publish_.publishLog("master_changed", "success", "");
        return;
    }

    if (action == "add" && type == "temp")
    {
        PasscodeTemp t;
        t.code = doc["code"] | "";
        t.expireAt = (uint64_t)(doc["expireAt"] | 0);

        passRepo_.setTemp(t);
        publish_.publishPasscodeList();
        publish_.publishLog("temp_passcode_added", "mqtt", t.code);
        return;
    }

    if (action == "delete")
    {
        const String code = doc["code"] | "";

        if (passRepo_.getMaster().length() >= 4 && code == passRepo_.getMaster())
        {
            passRepo_.setMaster("");
            publish_.publishPasscodeList();
            return;
        }

        if (passRepo_.hasTemp() && code == passRepo_.getTemp().code)
        {
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
            return;
        }
    }
}

void
MqttService::handleIccardsTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
        return;

    const String action = doc["action"] | "";
    const String id = doc["id"] | "";

    if (action == "add" && id.length() > 0)
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        if (cardRepo_.add(uid))
        {
            if (syncIccardsCache_)
                syncIccardsCache_(ctx_);
            publish_.publishICCardList();
            publish_.publishLog("card_added", "mqtt", uid);
        }
        return;
    }

    if (action == "delete" && !id.isEmpty())
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        if (cardRepo_.remove(uid))
        {
            if (syncIccardsCache_)
                syncIccardsCache_(ctx_);
            publish_.publishICCardList();
            publish_.publishLog("card_deleted", "mqtt", uid);
        }
        return;
    }

    if (action == "start_swipe_add")
    {
        appState_.swipeAdd.start();
        appState_.runtimeFlags.swipeAddMode = true;
        MqttManager::publish(Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_add_started");
        return;
    }
}

void
MqttService::handleControlTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(256);
    if (!JsonUtils::deserialize(payloadStr, doc))
        return;

    const String action = doc["action"] | "";
    if (action == "unlock")
    {
        if (onUnlock_)
            onUnlock_(ctx_, "remote");
    }
    else if (action == "lock")
    {
        if (onLock_)
            onLock_(ctx_, "remote");
    }
}