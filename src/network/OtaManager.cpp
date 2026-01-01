#include "network/OtaManager.h"

#include "utils/Logger.h"

#include <esp_ota_ops.h>

bool OtaManager::started = false;

#ifndef SMARTLOCK_OTA_PASSWORD
#define SMARTLOCK_OTA_PASSWORD ""
#endif

void
OtaManager::begin(const String& hostname)
{
    if (started)
        return;

    started = true;

    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPort(3232);

    if (String(SMARTLOCK_OTA_PASSWORD).length() > 0)
    {
        ArduinoOTA.setPassword(SMARTLOCK_OTA_PASSWORD);
    }
    else
    {
        Logger::warn("OTA", "OTA password is empty (define SMARTLOCK_OTA_PASSWORD)");
    }

    ArduinoOTA.onStart(
        []()
        {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "sketch";
            }
            else
            {
                type = "filesystem";
            }
            Logger::info("OTA", "Start updating %s", type.c_str());
        }
    );

    ArduinoOTA.onEnd([]() { Logger::info("OTA", "Update complete"); });

    ArduinoOTA.onProgress(
        [](unsigned int progress, unsigned int total)
        {
            static unsigned int lastPercent = 0;
            const unsigned int percent = (total == 0) ? 0 : (progress / (total / 100));
            if (percent >= lastPercent + 10)
            {
                Logger::info("OTA", "Progress: %u%%", percent);
                lastPercent = percent;
            }
        }
    );

    ArduinoOTA.onError(
        [](ota_error_t error)
        {
            const char* errorStr = "Unknown";

            switch (error)
            {
                case OTA_AUTH_ERROR:
                    errorStr = "Auth Failed";
                    break;
                case OTA_BEGIN_ERROR:
                    errorStr = "Begin Failed";
                    break;
                case OTA_CONNECT_ERROR:
                    errorStr = "Connect Failed";
                    break;
                case OTA_RECEIVE_ERROR:
                    errorStr = "Receive Failed";
                    break;
                case OTA_END_ERROR:
                    errorStr = "End Failed";
                    break;
            }

            Logger::error("OTA", "Error[%u]: %s", (unsigned)error, errorStr);
        }
    );

    ArduinoOTA.begin();
    Logger::info("OTA", "Ready on hostname: %s", hostname.c_str());
}

void
OtaManager::loop()
{
    if (started)
    {
        ArduinoOTA.handle();
    }
}

bool
OtaManager::isUpdateInProgress()
{
    return ArduinoOTA.getCommand() != 0;
}

void
OtaManager::verifyFirmware()
{
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (running)
    {
        Logger::info(
            "OTA", "Running partition: %s at 0x%x (size: %d bytes)", running->label,
            (unsigned)running->address, (int)running->size
        );
    }

    esp_ota_img_states_t ota_state;
    if (running && esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            Logger::info("OTA", "Validating new firmware...");
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            {
                Logger::info("OTA", "Firmware validated successfully");
            }
        }
    }
}
