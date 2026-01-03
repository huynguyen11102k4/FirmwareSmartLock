#include "network/NetworkManager.h"

#include "network/MqttManager.h"
#include "network/OtaManager.h"
#include "network/WifiManager.h"

bool NetworkManager::otaStarted = false;

void
NetworkManager::begin(const AppConfig& cfg, const String& clientId)
{
    WifiManager::begin(cfg);
    MqttManager::begin(cfg, clientId);
    otaStarted = false;
}

void
NetworkManager::loop()
{
    WifiManager::loop();

    if (!WifiManager::connected())
        return;

    if (!otaStarted)
    {
        OtaManager::begin("ESP32-" + WifiManager::ip());
        otaStarted = true;
    }

    MqttManager::reconnect();
    MqttManager::loop();
    OtaManager::loop();
}

bool
NetworkManager::online()
{
    return WifiManager::connected() && MqttManager::connected();
}
