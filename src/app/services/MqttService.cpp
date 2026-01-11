// File: app/services/MqttService.cpp

#include "app/services/MqttService.h"

#include "app/services/Topics.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "utils/JsonUtils.h"
#include "utils/Logger.h"
#include "utils/SecureCompare.h"
#include "utils/TimeUtils.h"

#include <ArduinoJson.h>

static const char* TAG_CB = "MQTT_CB";
static const char* TAG_DISP = "MQTT_DISP";
static const char* TAG_PASS = "MQTT_PASS";
static const char* TAG_CARD = "MQTT_CARD";
static const char* TAG_CTRL = "MQTT_CTRL";
static const char* TAG_SUB = "MQTT_SUB";
static const char* TAG_JSON = "MQTT_JSON";

namespace
{
String
defaultCardNameNext(const CardRepository& repo)
{
    return "ICCard" + String((int)(repo.size() + 1));
}

static inline void
logPayloadTruncated_(const char* tag, const char* prefix, const String& s, unsigned maxShow = 256)
{
    Logger::debug(tag, "%s len=%u", prefix, (unsigned)s.length());

    if (s.length() <= maxShow)
    {
        Logger::debug(tag, "%s='%s'", prefix, s.c_str());
        return;
    }

    String head = s.substring(0, maxShow);
    Logger::debug(tag, "%s(head %u)='%s...'", prefix, maxShow, head.c_str());
}

static inline void
logSubscribeTopic_(const String& t)
{
    Logger::info(TAG_SUB, "subscribe topic=%s", t.c_str());
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
    Logger::info(
        TAG_DISP,
        "ctor | mqttTopicPrefix='%s' doorName='%s' mac='%s'",
        appState_.mqttTopicPrefix.c_str(),
        appState_.doorName.c_str(),
        appState_.macAddress.c_str()
    );
}

void
MqttService::attachCallback()
{
    s_instance_ = this;
    Logger::info(TAG_CB, "attachCallback | instance=%p", (void*)s_instance_);
    MqttManager::setCallback(&MqttService::callbackThunk);
}

void
MqttService::onConnected(int infoVersion)
{
    const uint64_t now = TimeUtils::nowSeconds();

    if (now > 1700000000ULL)
    {
        passRepo_.setTs(now);
        Logger::info(
            "MQTT",
            "Passcode ts synchronized on connect: %llu",
            (unsigned long long)now
        );
    }
    else
    {
        Logger::warn(
            "MQTT",
            "Skip ts sync on connect (invalid time=%llu)",
            (unsigned long long)now
        );
    }

    const String base = appState_.mqttTopicPrefix;
    Logger::info(
        TAG_DISP,
        "connected | base='%s' infoVersion=%d now=%llu",
        base.c_str(),
        infoVersion,
        (unsigned long long)TimeUtils::nowSeconds()
    );

    Logger::info(TAG_DISP, "subscribing topics...");

    const String tPass = Topics::passcodes(base);
    const String tPassReq = Topics::passcodesReq(base);
    const String tCards = Topics::iccards(base);
    const String tCardsReq = Topics::iccardsReq(base);
    const String tCtrl = Topics::control(base);
    const String tBatReq = Topics::batteryReq(base);

    logSubscribeTopic_(tPass);
    MqttManager::subscribe(tPass, 0);

    logSubscribeTopic_(tPassReq);
    MqttManager::subscribe(tPassReq, 0);

    logSubscribeTopic_(tCards);
    MqttManager::subscribe(tCards, 0);

    logSubscribeTopic_(tCardsReq);
    MqttManager::subscribe(tCardsReq, 0);

    logSubscribeTopic_(tCtrl);
    MqttManager::subscribe(tCtrl, 0);

    logSubscribeTopic_(tBatReq);
    MqttManager::subscribe(tBatReq, 0);

    Logger::info(TAG_DISP, "bootstrap publish deferred");
    pendingBootstrapPublish_ = true;
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

    Logger::debug(TAG_CB, "RX topic=%s len=%u", topicStr.c_str(), length);
    logPayloadTruncated_(TAG_CB, "payload", payloadStr);

    s_instance_->dispatch_(topicStr, payloadStr);
}

void
MqttService::dispatch_(const String& topicStr, const String& payloadStr)
{
    const String base = appState_.mqttTopicPrefix;

    Logger::debug(TAG_DISP, "dispatch | base='%s' topic=%s", base.c_str(), topicStr.c_str());

    if (topicStr == Topics::passcodes(base))
    {
        Logger::info(TAG_DISP, "route -> passcodes");
        return handlePasscodesTopic_(payloadStr);
    }

    if (topicStr == Topics::passcodesReq(base))
    {
        Logger::info(TAG_DISP, "route -> passcodesReq (publish list)");
        return publish_.publishPasscodeList();
    }

    if (topicStr == Topics::iccards(base))
    {
        Logger::info(TAG_DISP, "route -> iccards");
        return handleIccardsTopic_(payloadStr);
    }

    if (topicStr == Topics::iccardsReq(base))
    {
        Logger::info(TAG_DISP, "route -> iccardsReq (publish list)");
        return publish_.publishICCardList();
    }

    if (topicStr == Topics::control(base))
    {
        Logger::info(TAG_DISP, "route -> control");
        return handleControlTopic_(payloadStr);
    }
    if (topicStr == Topics::info(base))
    {
        Logger::debug(TAG_DISP, "route -> info (ignored)");
        // ignore
        return;
    }
    if (topicStr == Topics::batteryReq(base))
    {
        Logger::info(TAG_DISP, "route -> batteryReq (publish battery)");
        publish_.publishBattery(random(20, 100));
        return;
    }

    Logger::warn(TAG_DISP, "unhandled topic=%s (base='%s')", topicStr.c_str(), base.c_str());
    logPayloadTruncated_(TAG_DISP, "unhandledPayload", payloadStr);
}

void
MqttService::handlePasscodesTopic_(const String& payloadStr)
{
    Logger::info(TAG_PASS, "handlePasscodesTopic_()");
    logPayloadTruncated_(TAG_PASS, "payload", payloadStr);

    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        Logger::warn(TAG_JSON, "passcodes: JSON deserialize FAILED");
        publish_.publishLog("HandlePasscodeRequestFailed", "AppRequest", "Phân tích dữ liệu thất bại.");
        return;
    }

    const String action = doc["action"] | "";
    const String type = doc["type"] | "";
    const uint64_t now = passRepo_.nowSecondsFallback();

    Logger::info(TAG_PASS, "parsed | action='%s' type='%s' now=%llu", action.c_str(), type.c_str(), (unsigned long long)now);

    if (action == "add" && type == "master")
    {
        const String newCode = doc["code"] | "";
        Logger::info(TAG_PASS, "add master | codeLen=%u", (unsigned)newCode.length());

        passRepo_.setMaster(newCode);
        passRepo_.setTs((long)now);

        publish_.publishPasscodeList();
        publish_.publishLog("MasterCodeAdded", "AppRequest", newCode);
        return;
    }

    if (action == "add" && (type == "one_time" || type == "timed"))
    {
        Passcode t;
        t.code = doc["code"] | "";
        t.type = type;

        const uint64_t effectiveAt = (uint64_t)(doc["effectiveAt"] | 0);
        const uint64_t expireAt = (uint64_t)(doc["expireAt"] | 0);
        const uint64_t ts = (uint64_t)(doc["ts"] | now);

        Logger::info(
            TAG_PASS,
            "add temp | type='%s' codeLen=%u effectiveAt=%llu expireAt=%llu ts=%llu",
            type.c_str(),
            (unsigned)t.code.length(),
            (unsigned long long)effectiveAt,
            (unsigned long long)expireAt,
            (unsigned long long)ts
        );

        if (effectiveAt > 0 && now < effectiveAt)
        {
            Logger::warn(TAG_PASS, "not effective yet | now=%llu < effectiveAt=%llu", (unsigned long long)now, (unsigned long long)effectiveAt);
            publish_.publishLog("HandlePasscodeRequestFailed", "AppRequest", "Passcode chưa có hiệu lực.");
        }

        if (expireAt > 0 && now >= expireAt)
        {
            Logger::warn(TAG_PASS, "expired | now=%llu >= expireAt=%llu", (unsigned long long)now, (unsigned long long)expireAt);
            publish_.publishLog("HandlePasscodeRequestFailed", "AppRequest", "Passcode đã hết hạn.");
            return;
        }

        passRepo_.addItem(t);
        passRepo_.setTs((long)ts);

        Logger::info(TAG_PASS, "added -> publish list");
        publish_.publishPasscodeList();
        publish_.publishLog("PasscodeAdded", "AppRequest", "Thêm Passcode thành công.");
        return;
    }

    if (action == "delete")
    {
        const String code = doc["code"] | "";
        const String type = doc["type"] | ""; // one_time | timed

        Logger::info(TAG_PASS, "delete | type='%s' codeLen=%u", type.c_str(), (unsigned)code.length());

        passRepo_.clearTemp();

        if (type == "one_time" || type == "timed")
        {
            const bool removed = passRepo_.removeItemByCode(code);
            Logger::info(TAG_PASS, "removeItemByCode -> %d", (int)removed);

            if (removed)
            {
                passRepo_.setTs((long)now);

                publish_.publishPasscodeList();
                publish_.publishLog("PasscodeDeleted", "AppRequest", "Xóa Passcode thành công.");
                return;
            }

            publish_.publishLog("HandlePasscodeRequestFailed", "AppRequest", "Xóa Passcode thất bại.");
            return;
        }

        Logger::warn(TAG_PASS, "invalid type for delete: '%s'", type.c_str());
        publish_.publishLog("HandlePasscodeRequestFailed", "AppRequest", "Loại Passcode không hợp lệ.");
        return;
    }

    Logger::warn(TAG_PASS, "unknown action/type | action='%s' type='%s'", action.c_str(), type.c_str());
}

void
MqttService::handleIccardsTopic_(const String& payloadStr)
{
    Logger::info(TAG_CARD, "handleIccardsTopic_()");
    logPayloadTruncated_(TAG_CARD, "payload", payloadStr);

    DynamicJsonDocument doc(512);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        Logger::warn(TAG_JSON, "iccards: JSON deserialize FAILED");
        publish_.publishLog("HandleCardFailed", "AppRequest", "Phân tích dữ liệu thất bại.");
        return;
    }

    const String action = doc["action"] | "";
    const String id = doc["uid"] | "";
    const String name = doc["name"] | "";

    Logger::info(TAG_CARD, "parsed | action='%s' uid='%s' nameLen=%u", action.c_str(), id.c_str(), (unsigned)name.length());

    if (action == "add" && !id.isEmpty())
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        String finalName = name;
        if (finalName.isEmpty())
        {
            finalName = defaultCardNameNext(cardRepo_);
            Logger::info(TAG_CARD, "auto name -> '%s'", finalName.c_str());
        }

        const bool ok = cardRepo_.add(uid, finalName);
        Logger::info(TAG_CARD, "cardRepo_.add(uid=%s, name=%s) -> %d", uid.c_str(), finalName.c_str(), (int)ok);

        if (ok)
        {
            cardRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishICCardList();
            publish_.publishLog("CardAdded", "AppRequest", "Thêm Card thành công");
        }
        else
        {
            publish_.publishLog("HandleCardFailed", "AppRequest", "Card đã tồn tại.");
        }
        return;
    }

    if (action == "remove" && !id.isEmpty())
    {
        String uid = id;
        uid.replace(":", "");
        uid.toUpperCase();

        const bool ok = cardRepo_.remove(uid);
        Logger::info(TAG_CARD, "cardRepo_.remove(uid=%s) -> %d", uid.c_str(), (int)ok);

        if (ok)
        {
            cardRepo_.setTs((long)TimeUtils::nowSeconds());
            publish_.publishICCardList();
            publish_.publishLog("CardDeleted", "AppRequest", "Xóa Card thành công.");
        }
        return;
    }

    if (action == "start_swipe_add")
    {
        Logger::info(TAG_CARD, "start_swipe_add | timeoutMs=%u", (unsigned)lockConfig_.swipeAddTimeoutMs);
        appState_.swipeAdd.start(lockConfig_.swipeAddTimeoutMs);
        appState_.runtimeFlags.swipeAddMode = true;
        return;
    }

    Logger::warn(TAG_CARD, "unknown action or missing uid | action='%s' uid='%s'", action.c_str(), id.c_str());
}

void
MqttService::handleControlTopic_(const String& payloadStr)
{
    Logger::info(TAG_CTRL, "handleControlTopic_()");
    logPayloadTruncated_(TAG_CTRL, "payload", payloadStr);

    DynamicJsonDocument doc(256);
    if (!JsonUtils::deserialize(payloadStr, doc))
    {
        Logger::warn(TAG_JSON, "control: JSON deserialize FAILED");
        publish_.publishLog("HandleControlFailed", "AppRequest", "Yêu cầu điều khiển thất bại.");
        return;
    }

    const String action = doc["action"] | "";
    Logger::info(TAG_CTRL, "parsed | action='%s'", action.c_str());

    if (action == "unlock")
    {
        Logger::info(TAG_CTRL, "door requestUnlock(Remote)");
        door_.requestUnlock("Remote");
    }
    else if (action == "lock")
    {
        Logger::info(TAG_CTRL, "door requestLock(Remote)");
        door_.requestLock("Remote");
    }
    else
    {
        Logger::warn(TAG_CTRL, "unknown action='%s'", action.c_str());
    }
}
