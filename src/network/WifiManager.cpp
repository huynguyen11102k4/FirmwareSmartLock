#include "network/WifiManager.h"

#include "utils/Logger.h"

bool WifiManager::configured = false;

static const char*
wifiStatusToString(wl_status_t status)
{
    switch (status)
    {
        case WL_IDLE_STATUS:
            return "IDLE";
        case WL_NO_SSID_AVAIL:
            return "NO_SSID";
        case WL_SCAN_COMPLETED:
            return "SCAN_COMPLETED";
        case WL_CONNECTED:
            return "CONNECTED";
        case WL_CONNECT_FAILED:
            return "CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

void
WifiManager::begin(const AppConfig& cfg)
{
    if (!cfg.hasWifi())
    {
        Logger::warn("WiFi", "No WiFi config");
        return;
    }

    configured = true;

    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());

    Logger::info(
        "WiFi", "Connecting to SSID=%s (pass len=%d)", cfg.wifiSsid.c_str(), cfg.wifiPass.length()
    );
}

void
WifiManager::loop()
{
    if (!configured)
        return;

    static uint32_t lastAttempt = 0;
    static wl_status_t lastStatus = WL_IDLE_STATUS;

    const wl_status_t status = WiFi.status();

    // Log khi status thay đổi
    if (status != lastStatus)
    {
        Logger::info("WiFi", "Status changed: %s", wifiStatusToString(status));
        lastStatus = status;
    }

    if (status == WL_CONNECTED)
        return;

    if ((uint32_t)(millis() - lastAttempt) < 5000)
        return;

    lastAttempt = millis();

    Logger::warn("WiFi", "Reconnecting... status=%s", wifiStatusToString(status));
    WiFi.reconnect();
}

bool
WifiManager::connected()
{
    return WiFi.status() == WL_CONNECTED;
}

String
WifiManager::ip()
{
    return WiFi.localIP().toString();
}
