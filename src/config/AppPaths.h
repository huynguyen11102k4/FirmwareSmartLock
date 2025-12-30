#pragma once

namespace AppPaths
{
    // Storage (SPIFFS)
    static constexpr const char *CONFIG_JSON = "/config.json";
    static constexpr const char *CARDS_JSON = "/cards.json";
    static constexpr const char *PASSCODES_JSON = "/passcodes.json"; // ví dụ nếu bạn có
}

namespace AppJsonKeys
{
    // Config keys
    static constexpr const char *WIFI_SSID = "wifi_ssid";
    static constexpr const char *WIFI_PASS = "wifi_pass";
    static constexpr const char *MQTT_HOST = "mqtt_host";
    static constexpr const char *MQTT_PORT = "mqtt_port";
    static constexpr const char *MQTT_USER = "mqtt_user";
    static constexpr const char *MQTT_PASS = "mqtt_pass";
    static constexpr const char *TOPIC_PREFIX = "topic_prefix";

    // Cards
    static constexpr const char *CARDS = "cards";
}
