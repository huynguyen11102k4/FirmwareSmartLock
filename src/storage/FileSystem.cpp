#include "FileSystem.h"

#include <SPIFFS.h>

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
    f.print(content);
    f.close();
    return true;
}

bool
FileSystem::remove(const char* path)
{
    return SPIFFS.remove(path);
}
