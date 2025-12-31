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
#include "config/LockConfig.h"
#include "config/TimeConfig.h"
#include "hardware/DoorHardware.h"
#include "models/AppState.h"
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
          rfid_(appState_, cardRepo_, iccardsCache_, publish_, this, &AppImpl::syncThunk_, door_),
          cmdQueue_(20)
    {
    }

    void
    begin()
    {
        Logger::begin(115200);
        Logger::info("APP", "Smart Lock Starting...");
        Logger::info("APP", "Version: 2.0 (Improved)");

        // ✅ Start watchdog (30s timeout)
        WatchdogManager::begin(30);
        Logger::info("APP", "Watchdog enabled (30s)");

        FileSystem::begin();

        // ✅ Load lock configuration
        if (lockConfig_.loadFromFile())
        {
            Logger::info("APP", "Lock config loaded from file");
        }
        else
        {
            Logger::info("APP", "Using default lock config");
            lockConfig_.saveToFile();
        }

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

        Logger::info("APP", "Initialization complete");
        WatchdogManager::feed();
    }

    void
    loop()
    {
        // ✅ Feed watchdog at start of loop
        WatchdogManager::feed();

        // Process command queue (thread-safe operations)
        processCommandQueue_();

        NetworkManager::loop();

        door_.loop(ctx_);

        const bool isConnected = MqttManager::connected();
        if (isConnected && !wasConnected_)
        {
            ble_.disableIfActive();
            mqtt_.onConnected(/*infoVersion=*/2); // Increment version
        }
        wasConnected_ = isConnected;

        http_.loop();

        // Battery publish with configurable interval
        if (shouldPublishBattery_())
        {
            publish_.publishBattery(readBatteryPercent_());
            lastBatteryPublishMs_ = millis();
        }

        // Sync with configurable interval
        if (shouldSync_())
        {
            publish_.publishPasscodeList();
            lastSyncMs_ = millis();
        }

        keypad_.loop();
        rfid_.loop();
        serviceTempPasscodeExpiry_();

        // Monitor system health
        monitorSystemHealth_();

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

        // ✅ Use configurable voltage range
        float percent = (v - lockConfig_.batteryMinVoltage) /
            (lockConfig_.batteryMaxVoltage - lockConfig_.batteryMinVoltage) * 100.0f;

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
        auto& doc = JsonPool::acquireLarge();

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

    bool
    shouldPublishBattery_() const
    {
        return (millis() - lastBatteryPublishMs_) >= lockConfig_.batteryPublishIntervalMs;
    }

    bool
    shouldSync_() const
    {
        return (millis() - lastSyncMs_) >= lockConfig_.syncIntervalMs;
    }

    void
    processCommandQueue_()
    {
        Command cmd;
        while (cmdQueue_.dequeue(cmd))
        {
            Logger::info("APP", "Processing command type: %d", (int)cmd.type);
            // Process command based on type
            // This provides thread-safe command handling
        }
    }

    void
    monitorSystemHealth_()
    {
        static uint32_t lastHealthCheck = 0;
        if (millis() - lastHealthCheck < 10000) // Every 10s
            return;

        lastHealthCheck = millis();

        // Check heap
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 20000) // Less than 20KB free
        {
            Logger::warn("APP", "Low heap: %d bytes", freeHeap);
        }

        // Check watchdog time
        uint32_t timeSinceFeed = WatchdogManager::timeSinceLastFeed();
        if (timeSinceFeed > 20000) // Over 20s without feed
        {
            Logger::warn("APP", "Watchdog feed delayed: %dms", timeSinceFeed);
        }

        // Check MQTT retry count
        if (MqttManager::getRetryAttempts() > 10)
        {
            Logger::warn("APP", "MQTT retry count high: %d", MqttManager::getRetryAttempts());
        }
    }

  private:
    Servo lockServo_;

    PasscodeRepository passRepo_;
    CardRepository cardRepo_;
    ConfigManager cfgMgr_;
    LockConfig lockConfig_;

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

    CommandQueue cmdQueue_;

    bool wasConnected_{false};
    uint32_t lastBatteryPublishMs_{0};
    uint32_t lastSyncMs_{0};
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