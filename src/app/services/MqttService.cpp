#include "app/services/MqttService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/SecureCompare.h"
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

String defaultCardNameNext(const CardRepository& repo)
{
    return "ICCard" + String((int)(repo.size() + 1));
}
} // namespace

MqttService* MqttService::s_instance_ = nullptr;

MqttService::MqttService(
    AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo,
    PublishService& publish, const LockConfig& lockConfig, DoorHardware& door
)
    : appState_(appState), passRepo_(passRepo), cardRepo_(cardRepo), publish_(publish),
      lockConfig_(lockConfig), door_(door)
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

    publish_.publishInfo(/*batteryPercent=*/0, infoVersion);

    publish_.publishState("locked", "startup");
    publish_.publishPasscodeList();
    publish_.publishICCardList();
}

void MqttService::callbackThunk(char* topic, byte* payload, unsigned int length)
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

void MqttService::dispatch_(const String& topicStr, const String& payloadStr)
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

void MqttService::handlePasscodesTopic_(const String& payloadStr)
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

        if (newCode.length() < (size_t)lockConfig_.minPinLength ||
            newCode.length() > (size_t)lockConfig_.maxPinLength)
            return;

        const String current = passRepo_.getMaster();
        const bool hasMaster = current.length() >= (size_t)lockConfig_.minPinLength;

        if (!hasMaster)
        {
            passRepo_.setMaster(newCode);
            passRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishPasscodeList();
            publish_.publishLog("master_set", "mqtt", "");
            return;
        }

        if (oldCode.isEmpty() || !SecureCompare::safeEquals(oldCode, current))
        {
            MqttManager::publish(
                Topics::passcodesError(appState_.mqttTopicPrefix),
                "{\"error\":\"old_master_required\"}"
            );
            return;
        }

        passRepo_.setMaster(newCode);
        passRepo_.setTs((long)TimeUtils::nowSeconds());
        publish_.publishPasscodeList();
        publish_.publishLog("master_changed", "mqtt", "");
        return;
    }

    if (action == "add" && type == "temp")
    {
        PasscodeTemp t;
        t.code = doc["code"] | "";
        t.expireAt = (uint64_t)(doc["expireAt"] | 0);

        if (t.code.length() < (size_t)lockConfig_.minPinLength ||
            t.code.length() > (size_t)lockConfig_.maxPinLength)
            return;

        passRepo_.setTemp(t);
        passRepo_.setTs((long)TimeUtils::nowSeconds());
        publish_.publishPasscodeList();
        publish_.publishLog("temp_passcode_added", "mqtt", maskCode(t.code));
        return;
    }

    if (action == "delete")
    {
        const String code = doc["code"] | "";

        if (passRepo_.getMaster().length() >= (size_t)lockConfig_.minPinLength &&
            SecureCompare::safeEquals(code, passRepo_.getMaster()))
        {
            passRepo_.setMaster("");
            passRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishPasscodeList();
            publish_.publishLog("master_deleted", "mqtt", "");
            return;
        }

        if (passRepo_.hasTemp() && SecureCompare::safeEquals(code, passRepo_.getTemp().code))
        {
            passRepo_.clearTemp();
            passRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishPasscodeList();
            publish_.publishLog("temp_deleted", "mqtt", "");
            return;
        }
    }
}

void MqttService::handleIccardsTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
        return;

    const String action = doc["action"] | "";
    const String id = doc["id"] | "";

    if (action == "add" && !id.isEmpty())
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        const String name = defaultCardNameNext(cardRepo_);

        if (cardRepo_.add(uid, name))
        {
            cardRepo_.setTs((long)TimeUtils::nowSeconds());
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
            cardRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishICCardList();
            publish_.publishLog("card_deleted", "mqtt", uid);
        }
        return;
    }

    if (action == "start_swipe_add")
    {
        appState_.swipeAdd.start(lockConfig_.swipeAddTimeoutMs);
        appState_.runtimeFlags.swipeAddMode = true;
        MqttManager::publish(Topics::iccardsStatus(appState_.mqttTopicPrefix), "swipe_add_started");
        return;
    }
}

void MqttService::handleControlTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(256);
    if (!JsonUtils::deserialize(payloadStr, doc))
        return;

    const String action = doc["action"] | "";
    if (action == "unlock")
    {
        door_.requestUnlock("remote");
    }
    else if (action == "lock")
    {
        door_.requestLock("remote");
    }
}
