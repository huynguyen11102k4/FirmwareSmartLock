#include "storage/FileSystem.h"

#include <SPIFFS.h>

namespace
{
String
tmpPathFor(const char* path)
{
    return String(path) + ".tmp";
}
} // namespace

bool
FileSystem::begin()
{
    return SPIFFS.begin(true);
}

bool
FileSystem::exists(const char* path)
{
    return SPIFFS.exists(path);
}

String
FileSystem::readFile(const char* path)
{
    File f = SPIFFS.open(path, "r");
    if (!f)
        return "";

    String content;
    content.reserve(static_cast<size_t>(f.size()));

    while (f.available())
        content += char(f.read());

    f.close();
    return content;
}

bool
FileSystem::writeFile(const char* path, const String& content)
{
    File f = SPIFFS.open(path, "w");
    if (!f)
        return false;

    const size_t written = f.print(content);
    f.close();
    return written == content.length();
}

bool
FileSystem::writeFileAtomic(const char* path, const String& content)
{
    const String tmp = tmpPathFor(path);

    {
        File f = SPIFFS.open(tmp.c_str(), "w");
        if (!f)
            return false;

        const size_t written = f.print(content);
        f.close();

        if (written != content.length())
        {
            SPIFFS.remove(tmp.c_str());
            return false;
        }
    }

    if (SPIFFS.exists(path))
    {
        if (!SPIFFS.remove(path))
        {
            SPIFFS.remove(tmp.c_str());
            return false;
        }
    }

    if (!SPIFFS.rename(tmp.c_str(), path))
    {
        SPIFFS.remove(tmp.c_str());
        return false;
    }

    return true;
}

bool
FileSystem::remove(const char* path)
{
    return SPIFFS.remove(path);
}