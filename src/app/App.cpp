#include "app/App.h"

#include "app/services/BleProvisionService.h"
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
        : publish_(appState_, passRepo_, cardRepo_), 
        ctx_{appState_, publish_},
          door_(
              lockServo_, LED_PIN, SERVO_PIN, DOOR_CONTACT_PIN,
              /*contactActiveLow=*/false,
              /*debounceMs=*/80,
              /*usePullup=*/true
          ),
          cmdQueue_(20), 
          mqtt_(appState_, passRepo_, cardRepo_, publish_, lockConfig_, door_),
          ble_(appState_, cfgMgr_, cmdQueue_),
          keypad_(appState_, passRepo_, publish_, door_, lockConfig_),
          rfid_(appState_, cardRepo_, publish_, door_, lockConfig_)
    {
    }

    void
    begin()
    {
        Logger::begin(115200);
        Logger::info("APP", "Smart Lock Starting...");

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

        const bool hasConfig = cfgMgr_.load();
        setBaseTopicFromConfigOrDefault_();

        door_.begin(ctx_);

        passRepo_.load();
        cardRepo_.load();

        rfid_.begin();
        keypad_.begin();

        if (!hasConfig || !cfgMgr_.isProvisioned())
        {
            Logger::warn("APP", "Not provisioned -> BLE mode");
            ble_.begin();
        }
        else
        {
            Logger::info("APP", "Provisioned -> Network mode");

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

        handleWifiProvisionValidation_();

        NetworkManager::loop();

        door_.loop(ctx_);

        if (appState_.wifiProvision.waitingForConnection &&
        WiFi.status() == WL_CONNECTED)
        {
            Logger::info("APP", "Sending information for client...");
            
            ble_.notifyProvisionSuccess();

            Logger::info("APP", "WiFi provision successful! Restarting...");

            appState_.wifiProvision.reset();
            ble_.forceCleanup();
            delay(1000);
            ESP.restart();
            return;
        }

        const bool isConnected = MqttManager::connected();
        if (isConnected && !wasConnected_)
        {
            mqtt_.onConnected(/*infoVersion=*/3);
        }
        wasConnected_ = isConnected;

        keypad_.loop();
        rfid_.loop();

        serviceTempPasscodeExpiry_();
        monitorSystemHealth_();

        yield();
    }

    void
    handleWifiProvisionValidation_()
    {
        if (!appState_.wifiProvision.waitingForConnection)
            return;

        if (WiFi.status() == WL_CONNECTED)
        {
            return;
        }

        if (!appState_.wifiProvision.shouldRetry())
            return;

        appState_.wifiProvision.recordAttempt();
        
        Logger::warn("APP", "WiFi connection attempt %d/%d failed", 
                    appState_.wifiProvision.connectionAttempts,
                    WifiProvisionState::MAX_ATTEMPTS);

        if (appState_.wifiProvision.hasExceededMaxAttempts())
        {
            Logger::error("APP", "WiFi provision failed. Clearing WiFi config...");
            
            AppConfig cfg = cfgMgr_.get();
            cfg.clear(AppConfig::ClearMode::WIFI_ONLY);
            cfgMgr_.save();
            
            appState_.wifiProvision.reset();
            
            ble_.notifyProvisionFailed("WiFi connection failed after 5 attempts");
            
            Logger::info("APP", "BLE mode active - waiting for new WiFi credentials");
        }
    }

  private:
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

        const Passcode t = passRepo_.getTemp();
        if (t.isExpired(passRepo_.nowSecondsFallback()))
        {
            passRepo_.clearTemp();
            publish_.publishPasscodeList();
        }
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
    if ((uint32_t)(millis() - lastHealthCheck) < 30000)
        return;

    lastHealthCheck = millis();

    const size_t freeHeap = ESP.getFreeHeap();
    
    if (freeHeap < 20000)
    {
        Logger::error("APP", "CRITICAL: Low heap %d bytes. Restarting...", (int)freeHeap);
        delay(1000);
        ESP.restart();
        return;
    }
    
    if (freeHeap < 30000)
    {
        Logger::warn("APP", "Low heap warning: %d bytes", (int)freeHeap);
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
    KeypadService keypad_;
    RfidService rfid_;

    bool wasConnected_{false};
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
