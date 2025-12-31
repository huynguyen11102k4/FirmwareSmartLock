#include "app/services/HttpService.h"

#include "network/NetworkManager.h"
#include "utils/JsonUtils.h"

#include <ArduinoJson.h>

HttpService::HttpService(AppState& appState, void* ctx, BatteryFn readBatteryPercent)
    : appState_(appState), ctx_(ctx), readBatteryPercent_(readBatteryPercent), server_(80)
{
}

void
HttpService::begin()
{
    server_.on("/info", HTTP_GET, [this]() { handleInfo_(); });
    server_.onNotFound([this]() { handleNotFound_(); });
    server_.begin();
}

void
HttpService::loop()
{
    server_.handleClient();
}

void
HttpService::handleInfo_()
{
    const int batt = readBatteryPercent_ ? readBatteryPercent_(ctx_) : 0;

    DynamicJsonDocument doc(256);
    doc["mac"] = appState_.macAddress;
    doc["topic"] = appState_.mqttTopicPrefix;
    doc["battery"] = batt;
    doc["wifi"] = NetworkManager::online();

    server_.send(200, "application/json", JsonUtils::serialize(doc));
}

void
HttpService::handleNotFound_()
{
    server_.send(404, "text/plain", "Not found");
}