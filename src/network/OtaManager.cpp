#include "OtaManager.h"

#include "utils/Logger.h"

#include <esp_ota_ops.h>

bool OtaManager::started = false;

// ✅ Set this to your secure OTA password
static constexpr const char* OTA_PASSWORD = "YourSecurePassword123!";

void
OtaManager::begin(const String& hostname)
{
    if (started)
        return;
    started = true;

    ArduinoOTA.setHostname(hostname.c_str());

    // ✅ Enable password protection
    ArduinoOTA.setPassword(OTA_PASSWORD);

    // ✅ Set port (default 3232, can change for security)
    ArduinoOTA.setPort(3232);

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

            // ✅ Stop critical services during OTA
            // Example: mqtt.disconnect(), watchdog.disable()
        }
    );

    ArduinoOTA.onEnd([]() { Logger::info("OTA", "Update complete"); });

    ArduinoOTA.onProgress(
        [](unsigned int progress, unsigned int total)
        {
            static unsigned int lastPercent = 0;
            unsigned int percent = (progress / (total / 100));

            // Log every 10%
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

            Logger::error("OTA", "Error[%u]: %s", error, errorStr);
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
    // Check if OTA is currently running
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
            running->address, running->size
        );
    }

    // Check if this is first boot after OTA
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            Logger::info("OTA", "Validating new firmware...");

            // Mark as valid after successful boot
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            {
                Logger::info("OTA", "Firmware validated successfully");
            }
        }
    }
}