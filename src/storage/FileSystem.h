#pragma once
#include <Arduino.h>

class FileSystem
{
  public:
    static bool
    begin();
    static bool
    exists(const char* path);

    static String
    readFile(const char* path);
    static bool
    writeFile(const char* path, const String& content);
    static bool
    remove(const char* path);
};
