#include "app/services/MqttService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/SecureCompare.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"

#include <ArduinoJson.h>

static const char* TAG_CB   = "MQTT_CB";
static const char* TAG_DISP = "MQTT_DISP";
static const char* TAG_PASS = "MQTT_PASS";
static const char* TAG_CARD = "MQTT_CARD";
static const char* TAG_CTRL = "MQTT_CTRL";

namespace
{
String
defaultCardNameNext(const CardRepository& repo)
{
    return "ICCard" + String((int)(repo.size() + 1));
}
} // namespace

MqttService* MqttService::s_instance_ = nullptr;

MqttService::MqttService(
    AppState& appState, PasscodeRepository& passRepo, CardRepository& cardRepo,
    PublishService& publish, const LockConfig& lockConfig, DoorHardware& door
)
    : appState_(appState),
      passRepo_(passRepo),
      cardRepo_(cardRepo),
      publish_(publish),
      lockConfig_(lockConfig),
      door_(door)
{
}

void
MqttService::attachCallback()
{
    s_instance_ = this;
    Logger::info(TAG_CB, "attachCallback");
    MqttManager::setCallback(&MqttService::callbackThunk);
}

void
MqttService::onConnected(int infoVersion)
{
    const String base = appState_.mqttTopicPrefix;

    Logger::info(TAG_DISP, "connected, subscribing topics");

    MqttManager::subscribe(Topics::passcodes(base), 1);
    MqttManager::subscribe(Topics::passcodesReq(base), 1);
    MqttManager::subscribe(Topics::iccards(base), 1);
    MqttManager::subscribe(Topics::iccardsReq(base), 1);
    MqttManager::subscribe(Topics::control(base), 1);

    publish_.publishInfo(0, infoVersion);
    publish_.publishState("locked", "startup");
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

    Logger::debug(
        TAG_CB,
        "RX topic=%s len=%u payload=%s",
        topicStr.c_str(),
        length,
        payloadStr.c_str()
    );

    s_instance_->dispatch_(topicStr, payloadStr);
}

void
MqttService::dispatch_(const String& topicStr, const String& payloadStr)
{
    const String base = appState_.mqttTopicPrefix;

    Logger::debug(TAG_DISP, "dispatch topic=%s", topicStr.c_str());

    if (topicStr == Topics::passcodes(base))
    {
        return handlePasscodesTopic_(payloadStr);
    }

    if (topicStr == Topics::passcodesReq(base))
    {
        return publish_.publishPasscodeList();
    }

    if (topicStr == Topics::iccards(base))
    {
        return handleIccardsTopic_(payloadStr);
    }

    if (topicStr == Topics::iccardsReq(base))
    {
        return publish_.publishICCardList();
    }

    if (topicStr == Topics::control(base))
    {
        return handleControlTopic_(payloadStr);
    }
    if (topicStr == Topics::info(base))
    {
        // ignore
        return;
    }
}

void
MqttService::handlePasscodesTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        publish_.publishLog("handle_payload_failed", "mqtt", "JSON parse failed");
        return;
    }

    const String action = doc["action"] | "";
    const String type   = doc["type"] | "";

    const uint64_t now = TimeUtils::nowSeconds();

    if (action == "add" && type == "master")
    {
        const String newCode = doc["code"] | "";

        passRepo_.setMaster(newCode);
        passRepo_.setTs((long)now);

        publish_.publishPasscodeList();
        publish_.publishLog("master_set", "mqtt", newCode);
        return;
    }

    if (action == "add" &&
        (type == "one_time" || type == "timed"))
    {
        Passcode t;
        t.code = doc["code"] | "";
        t.type = type;

        const uint64_t effectiveAt =
            (uint64_t)(doc["effectiveAt"] | 0);
        const uint64_t expireAt =
            (uint64_t)(doc["expireAt"] | 0);

        const uint64_t ts =
            (uint64_t)(doc["ts"] | now);

        if (effectiveAt > 0 && now < effectiveAt)
        {
            publish_.publishLog("add_passcode_failed", "mqtt", "passcode not effective yet");
            return;
        }

        if (expireAt > 0 && now >= expireAt)
        {
            publish_.publishLog("add_passcode_failed", "mqtt", "passcode already expired");
            return;
        }

        passRepo_.addItem(t);
        passRepo_.setTs((long)ts);

        publish_.publishPasscodeList();
        publish_.publishLog("add_passcode_success", "mqtt", "add passcode success");
        return;
    }

    if (action == "delete")
    {
        const String code = doc["code"] | "";
        const String type = doc["type"] | ""; // one_time | timed

        passRepo_.clearTemp();

        if (type == "one_time" || type == "timed")
        {
            if (passRepo_.removeItemByCode(code))
            {
                passRepo_.setTs((long)now);

                publish_.publishPasscodeList();
                publish_.publishLog("delete_passcode_success", "mqtt", "delete passcode success");
                return;
            }

            publish_.publishLog("delete_passcode_failed", "mqtt", "passcode not found");
            return;
        }

        publish_.publishLog("delete_passcode_failed", "mqtt", "delete ignored: unsupported this type");
        return;
    }
}

void
MqttService::handleIccardsTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        publish_.publishLog("handle_card_failed", "mqtt", "JSON parse failed");
        return;
    }

    const String action = doc["action"] | "";
    const String id     = doc["uid"] | "";
    const String name   = doc["name"] | "";

    if (action == "add" && !id.isEmpty())
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        String finalName = name;
        if (finalName.isEmpty())
        {
            // auto name if empty
            finalName = defaultCardNameNext(cardRepo_);
        }

        if (cardRepo_.add(uid, finalName))
        {
            cardRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishICCardList();
            publish_.publishLog("card_added", "mqtt", uid);
        }
        else
        { 
            publish_.publishLog("card_add_failed", "mqtt", "add card failed (maybe exists)");
        }
        return;
    }


    if (action == "remove" && !id.isEmpty())
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
        return;
    }
}

void
MqttService::handleControlTopic_(const String& payloadStr)
{
    DynamicJsonDocument doc(256);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        publish_.publishLog("handle_payload_failed", "mqtt", "JSON parse failed");
        return;
    }

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
