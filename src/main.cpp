#include "app/App.h"

#include <Arduino.h>
#include <SPIFFS.h>

static App app;

void
setup()
{
    // SPIFFS.format();
    app.begin();
}

void
loop()
{
    app.loop();
}
