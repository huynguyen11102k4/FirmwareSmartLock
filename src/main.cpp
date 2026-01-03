#include "app/App.h"

#include <Arduino.h>

static App app;

void
setup()
{
    app.begin();
}

void
loop()
{
    app.loop();
}
