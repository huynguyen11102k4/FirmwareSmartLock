#pragma once

struct AppState;
class PublishService;

struct AppContext
{
    AppState& app;
    PublishService& publish;
};