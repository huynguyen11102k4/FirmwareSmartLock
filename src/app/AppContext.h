#pragma once

struct AppState;
class PublishService;
struct LockConfig;

struct AppContext
{
    AppState& app;
    PublishService& publish;
    const LockConfig& lock;
};
