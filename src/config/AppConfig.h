#pragma once
#include <Arduino.h>

struct AppConfig
{
    String wifiSsid;
    String wifiPass;

    String mqttHost;
    int mqttPort = 8883;
    String mqttUser;
    String mqttPass;

    String topicPrefix;

    bool
    hasWifi() const
    {
        return wifiSsid.length() > 0;
    }

    bool
    hasMqtt() const
    {
        return mqttHost.length() > 0 && mqttPort > 0;
    }

    bool
    isValid() const
    {
        return hasWifi() && hasMqtt();
    }

    enum class ClearMode
    {
        ALL,
        WIFI_ONLY,
        MQTT_ONLY
    };

    void
    clear(ClearMode mode = ClearMode::ALL)
    {
        if (mode == ClearMode::ALL || mode == ClearMode::WIFI_ONLY)
        {
            wifiSsid.clear();
            wifiPass.clear();
        }
        if (mode == ClearMode::ALL || mode == ClearMode::MQTT_ONLY)
        {
            mqttHost.clear();
            mqttPort = 8883;
            mqttUser.clear();
            mqttPass.clear();
            topicPrefix.clear();
        }
    }
};