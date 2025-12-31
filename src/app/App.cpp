#include "app/App.h"

#include "app/services/BleProvisionService.h"
#include "app/services/HttpService.h"
#include "app/services/KeypadService.h"
#include "app/services/MqttService.h"
#include "app/services/PublishService.h"
#include "app/services/RfidService.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "config/HardwarePins.h"
#include "config/LockConfig.h"
#include "config/TimeConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
#include "models/Command.h"
#include "models/PasscodeTemp.h"
#include "network/MqttManager.h"
#include "network/NetworkManager.h"
#include "storage/CardRepository.h"
#include "storage/FileSystem.h"
#include "storage/PasscodeRepository.h"
#include "utils/CommandQueue.h"
#include "utils/JsonPool.h"
#include "utils/JsonUtils.h"
#include "utils/Logger.h"
#include "utils/TimeUtils.h"
#include "utils/WatchdogManager.h"

#include <Arduino.h>
#include <Servo.h>
#include <WiFi.h>
#include <time.h>

class AppImpl
{
  public:
    AppImpl()
        : publish_(appState_, passRepo_, cardRepo_), ctx_{appState_, publish_, lockConfig_},
          door_(
              lockServo_, LED_PIN, SERVO_PIN, DOOR_CONTACT_PIN,
              /*contactActiveLow=*/false,
              /*debounceMs=*/80,
              /*usePullup=*/true
          ),
          cmdQueue_(20), mqtt_(appState_, passRepo_, cardRepo_, publish_, lockConfig_, door_),
          ble_(appState_, cfgMgr_, cmdQueue_), http_(appState_, this, &AppImpl::batteryThunk_),
          keypad_(appState_, passRepo_, publish_, door_, lockConfig_),
          rfid_(appState_, cardRepo_, publish_, door_, lockConfig_)
    {
    }

    void
    begin()
    {
        Logger::begin(115200);
        Logger::info("APP", "Smart Lock Starting...");
        Logger::info("APP", "Version: 3.0 (Refactor + Policy Wiring)");

        WatchdogManager::begin(30);
        Logger::info("APP", "Watchdog enabled (30s)");

        FileSystem::begin();

        if (lockConfig_.loadFromFile())
        {
            Logger::info("APP", "Lock config loaded");
        }
        else
        {
            Logger::warn("APP", "Lock config missing, saving defaults");
            lockConfig_.saveToFile();
        }

        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

        WiFi.mode(WIFI_STA);
        delay(100);

        appState_.init(WiFi.macAddress());

        passRepo_.load();
        cardRepo_.load();

        const bool hasConfig = cfgMgr_.load();
        setBaseTopicFromConfigOrDefault_();

        door_.begin(ctx_);

        rfid_.begin();
        keypad_.begin();
        http_.begin();

        if (!hasConfig || !cfgMgr_.isProvisioned())
        {
            Logger::warn("APP", "Not provisioned -> BLE mode");
            ble_.begin();
        }
        else
        {
            Logger::info("APP", "Provisioned -> Network mode");
            ble_.disableIfActive();

            const String clientId = "ESP32DoorLock-" + appState_.macAddress;
            NetworkManager::begin(cfgMgr_.get(), clientId);
            mqtt_.attachCallback();
        }

        Logger::info("APP", "Initialization complete");
        WatchdogManager::feed();
    }

    void
    loop()
    {
        WatchdogManager::feed();

        processCommandQueue_();

        NetworkManager::loop();

        door_.loop(ctx_);

        const bool isConnected = MqttManager::connected();
        if (isConnected && !wasConnected_)
        {
            ble_.disableIfActive();
            mqtt_.onConnected(/*infoVersion=*/3);
        }
        wasConnected_ = isConnected;

        http_.loop();

        if (shouldPublishBattery_())
        {
            publish_.publishBattery(readBatteryPercent_());
            lastBatteryPublishMs_ = millis();
        }

        if (shouldSync_())
        {
            publish_.publishPasscodeList();
            publish_.publishICCardList();
            lastSyncMs_ = millis();
        }

        keypad_.loop();
        rfid_.loop();

        serviceTempPasscodeExpiry_();
        monitorSystemHealth_();

        yield();
    }

  private:
    static int
    batteryThunk_(void* ctx)
    {
        return static_cast<AppImpl*>(ctx)->readBatteryPercent_();
    }

    int
    readBatteryPercent_()
    {
        const int raw = analogRead(BATTERY_PIN);
        const float v = (raw / 4095.0f) * 3.3f * 2.0f;

        const float minV = lockConfig_.batteryMinVoltage;
        const float maxV = lockConfig_.batteryMaxVoltage;

        float percent = 0.0f;
        if (maxV > minV)
        {
            percent = (v - minV) / (maxV - minV) * 100.0f;
        }

        if (percent < 0.0f)
            percent = 0.0f;
        if (percent > 100.0f)
            percent = 100.0f;

        return (int)percent;
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

    bool
    shouldPublishBattery_() const
    {
        return (uint32_t)(millis() - lastBatteryPublishMs_) >= lockConfig_.batteryPublishIntervalMs;
    }

    bool
    shouldSync_() const
    {
        return (uint32_t)(millis() - lastSyncMs_) >= lockConfig_.syncIntervalMs;
    }

    void
    processCommandQueue_()
    {
        Command cmd;
        while (cmdQueue_.dequeue(cmd))
        {
            Logger::info("APP", "Command: type=%d src=%s", (int)cmd.type, cmd.source.c_str());

            if (cmd.type == CommandType::APPLY_CONFIG)
            {
                const bool ok = cfgMgr_.load() && cfgMgr_.isProvisioned();
                setBaseTopicFromConfigOrDefault_();

                if (!ok)
                {
                    Logger::error("APP", "APPLY_CONFIG failed: invalid config");
                    continue;
                }

                const String clientId = "ESP32DoorLock-" + appState_.macAddress;
                NetworkManager::begin(cfgMgr_.get(), clientId);
                mqtt_.attachCallback();

                Logger::info("APP", "APPLY_CONFIG ok -> Network begin");
            }
        }
    }

    void
    monitorSystemHealth_()
    {
        static uint32_t lastHealthCheck = 0;
        if ((uint32_t)(millis() - lastHealthCheck) < 10000)
            return;

        lastHealthCheck = millis();

        const size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 20000)
        {
            Logger::warn("APP", "Low heap: %d bytes", (int)freeHeap);
        }

        const uint32_t timeSinceFeed = WatchdogManager::timeSinceLastFeed();
        if (timeSinceFeed > 20000)
        {
            Logger::warn("APP", "Watchdog feed delayed: %dms", (int)timeSinceFeed);
        }

        const int retry = MqttManager::getRetryAttempts();
        if (retry > 10)
        {
            Logger::warn("APP", "MQTT retry high: %d", retry);
        }
    }

  private:
    Servo lockServo_;

    PasscodeRepository passRepo_;
    CardRepository cardRepo_;
    ConfigManager cfgMgr_;
    LockConfig lockConfig_;

    AppState appState_;

    PublishService publish_;
    AppContext ctx_;

    DoorHardware door_;

    CommandQueue cmdQueue_;

    MqttService mqtt_;
    BleProvisionService ble_;
    HttpService http_;
    KeypadService keypad_;
    RfidService rfid_;

    bool wasConnected_{false};
    uint32_t lastBatteryPublishMs_{0};
    uint32_t lastSyncMs_{0};
};

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
