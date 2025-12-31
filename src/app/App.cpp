#include "app/App.h"

#include "app/services/BleProvisionService.h"
#include "app/services/HttpService.h"
#include "app/services/KeypadService.h"
#include "app/services/MqttService.h"
#include "app/services/PublishService.h"
#include "app/services/RfidService.h"
#include "config/AppConfig.h"
#include "config/AppPaths.h"
#include "config/ConfigManager.h"
#include "config/HardwarePins.h"
#include "config/TimeConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "network/NetworkManager.h"
#include "storage/CardRepository.h"
#include "storage/FileSystem.h"
#include "storage/PasscodeRepository.h"
#include "utils/JsonUtils.h"
#include "utils/Logger.h"
#include "utils/TimeUtils.h"

#include <Arduino.h>
#include <Servo.h>
#include <WiFi.h>
#include <time.h>

class AppImpl
{
  public:
    AppImpl()
        : publish_(appState_, passRepo_, iccardsCache_), ctx_{appState_, publish_},
          door_(
              lockServo_, LED_PIN, SERVO_PIN, DOOR_CONTACT_PIN,
              /*contactActiveLow=*/false, /*debounceMs=*/80, /*usePullup=*/true
          ),
          mqtt_(
              appState_, passRepo_, cardRepo_, iccardsCache_, publish_, this, &AppImpl::syncThunk_,
              &AppImpl::batteryThunk_, door_
          ),
          ble_(appState_, cfgMgr_, mqtt_), http_(appState_, this, &AppImpl::batteryThunk_),
          keypad_(appState_, passRepo_, publish_, door_),
          rfid_(appState_, cardRepo_, iccardsCache_, publish_, this, &AppImpl::syncThunk_, door_)
    {
    }

    void
    begin()
    {
        Logger::begin(115200);
        Logger::info("APP", "Smart Lock Starting...");

        FileSystem::begin();
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

        WiFi.mode(WIFI_STA);
        delay(100);

        appState_.init(WiFi.macAddress());

        passRepo_.load();
        cardRepo_.load();
        syncIccardsCacheFromRepositoryFile_();

        const bool hasConfig = cfgMgr_.load();
        setBaseTopicFromConfigOrDefault_();

        door_.begin(ctx_);

        rfid_.begin();
        keypad_.begin();
        http_.begin();

        if (!hasConfig || !cfgMgr_.isProvisioned())
        {
            ble_.begin();
        }
        else
        {
            ble_.disableIfActive();
            const String clientId = "ESP32DoorLock-" + appState_.macAddress;
            NetworkManager::begin(cfgMgr_.get(), clientId);
            mqtt_.attachCallback();
        }
    }

    void
    loop()
    {
        NetworkManager::loop();

        door_.loop(ctx_);

        const bool isConnected = MqttManager::connected();
        if (isConnected && !wasConnected_)
        {
            ble_.disableIfActive();
            mqtt_.onConnected(/*infoVersion=*/1);
        }
        wasConnected_ = isConnected;

        http_.loop();

        if (appState_.shouldPublishBattery())
        {
            publish_.publishBattery(readBatteryPercent_());
            appState_.markBatteryPublished();
        }

        if (appState_.shouldSync())
        {
            publish_.publishPasscodeList();
            appState_.markSynced();
        }

        keypad_.loop();
        rfid_.loop();
        serviceTempPasscodeExpiry_();

        yield();
    }

  private:
    static void
    syncThunk_(void* ctx)
    {
        static_cast<AppImpl*>(ctx)->syncIccardsCacheFromRepositoryFile_();
    }

    static int
    batteryThunk_(void* ctx)
    {
        return static_cast<AppImpl*>(ctx)->readBatteryPercent_();
    }

    int
    readBatteryPercent_()
    {
        int raw = analogRead(BATTERY_PIN);
        float v = (raw / 4095.0f) * 3.3f * 2.0f;
        float percent = (v - 3.3f) / (4.2f - 3.3f) * 100.0f;
        if (percent < 0)
            percent = 0;
        if (percent > 100)
            percent = 100;
        return (int)percent;
    }

    void
    syncIccardsCacheFromRepositoryFile_()
    {
        iccardsCache_.clear();

        if (!FileSystem::exists(AppPaths::CARDS_JSON))
            return;

        const String json = FileSystem::readFile(AppPaths::CARDS_JSON);
        DynamicJsonDocument doc(1024);
        if (!JsonUtils::deserialize(json, doc))
            return;

        if (doc.containsKey(AppJsonKeys::CARDS) && doc[AppJsonKeys::CARDS].is<JsonArray>())
        {
            for (JsonVariant v : doc[AppJsonKeys::CARDS].as<JsonArray>())
            {
                iccardsCache_.push_back(v.as<String>());
            }
        }
    }

    void
    setBaseTopicFromConfigOrDefault_()
    {
        const AppConfig& cfg = cfgMgr_.get();
        appState_.setMqttTopicPrefix(cfg.topicPrefix);
    }

    void
    serviceTempPasscodeExpiry_()
    {
        if (!passRepo_.hasTemp())
            return;

        const PasscodeTemp t = passRepo_.getTemp();
        if (t.isExpired(TimeUtils::nowSeconds()))
        {
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
        }
    }

  private:
    Servo lockServo_;

    PasscodeRepository passRepo_;
    CardRepository cardRepo_;
    ConfigManager cfgMgr_;

    AppState appState_;
    std::vector<String> iccardsCache_;

    PublishService publish_;
    AppContext ctx_;

    DoorHardware door_;

    MqttService mqtt_;
    BleProvisionService ble_;
    HttpService http_;
    KeypadService keypad_;
    RfidService rfid_;

    bool wasConnected_{false};
};

// Wrapper
static AppImpl g_app;

App::App() = default;

void
App::begin()
{
    g_app.begin();
}

void
App::loop()
{
    g_app.loop();
}