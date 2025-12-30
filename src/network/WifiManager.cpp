#include "WifiManager.h"
#include "utils/Logger.h"

bool WifiManager::configured = false;

void WifiManager::begin(const AppConfig& cfg) {
  if (!cfg.hasWifi()) return;

  configured = true;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());

  Logger::info("WiFi", "Connecting to SSID=%s", cfg.wifiSsid.c_str());
}

void WifiManager::loop() {
  if (!configured) return;

  static uint32_t lastAttempt = 0;

  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();

  Logger::warn("WiFi", "Reconnecting...");
  WiFi.reconnect();
}

bool WifiManager::connected() {
  return WiFi.status() == WL_CONNECTED;
}

String WifiManager::ip() {
  return WiFi.localIP().toString();
}
