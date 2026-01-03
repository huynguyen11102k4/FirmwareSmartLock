#pragma once
#include <Arduino.h>

class DeviceIdentity
{
  public:
    static String
    macNoColonLower(const String& mac)
    {
        String m = mac;
        m.replace(":", "");
        m.toLowerCase();
        return m;
    }

    static String
    macNoColonUpper(const String& mac)
    {
        String m = mac;
        m.replace(":", "");
        m.toUpperCase();
        return m;
    }

    static String
    makeTopicPrefix(const String& mac)
    {
        return macNoColonLower(mac);
    }

    static String
    makeDefaultName(const String& mac)
    {
        const String m = macNoColonUpper(mac);
        const String tail = (m.length() >= 4) ? m.substring(m.length() - 4) : m;
        return "Door-" + tail;
    }

    static String
    makeDoorCode()
    {
        uint32_t r = esp_random();
        uint32_t code = r % 1000000;

        char buf[7];
        snprintf(buf, sizeof(buf), "%06lu", code);

        return String(buf);
    }

  private:
    static uint32_t
    crc32_(const uint8_t* data, size_t len)
    {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; i++)
        {
            crc ^= (uint32_t)data[i];
            for (uint8_t b = 0; b < 8; b++)
            {
                const uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
                crc = (crc >> 1) ^ mask;
            }
        }
        return ~crc;
    }

    static String
    toBase36_(uint32_t value, uint8_t minLen)
    {
        static const char* kDigits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        char buf[16];
        uint8_t idx = 0;

        if (value == 0)
        {
            buf[idx++] = '0';
        }
        else
        {
            while (value > 0 && idx < sizeof(buf) - 1)
            {
                buf[idx++] = kDigits[value % 36u];
                value /= 36u;
            }
        }

        while (idx < minLen && idx < sizeof(buf) - 1)
        {
            buf[idx++] = '0';
        }

        buf[idx] = '\0';

        // reverse
        for (uint8_t i = 0; i < idx / 2; i++)
        {
            const char t = buf[i];
            buf[i] = buf[idx - 1 - i];
            buf[idx - 1 - i] = t;
        }

        return String(buf);
    }
};