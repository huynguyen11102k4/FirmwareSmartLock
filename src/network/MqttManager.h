#pragma once
#include "config/AppConfig.h"

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

using MqttCallback = void (*)(char*, byte*, unsigned int);

class MqttManager
{
  public:
    static void
    begin(const AppConfig& cfg, const String& clientId);

    static void
    loop();

    static bool
    connected();

    static void
    reconnect();

    static bool
    publish(const String& topic, const String& payload, bool retained = false);

    static void
    subscribe(const String& topic, int qos = 1);

    static void
    setCallback(MqttCallback cb);

    static int
    getRetryAttempts();

  private:
    static void
    setupClient();
};
