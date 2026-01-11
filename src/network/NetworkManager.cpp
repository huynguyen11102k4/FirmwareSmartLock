#include "network/NetworkManager.h"

#include "network/MqttManager.h"
#include "network/WifiManager.h"

void
NetworkManager::begin(const AppConfig& cfg, const String& clientId)
{
    WifiManager::begin(cfg);
    MqttManager::begin(cfg, clientId);
}

void
NetworkManager::loop()
{
    WifiManager::loop();

    if (!WifiManager::connected())
        return;

    if (!MqttManager::connected())
        MqttManager::reconnect();

    MqttManager::loop();
}

bool
NetworkManager::online()
{
    return WifiManager::connected() && MqttManager::connected();
}
