#pragma once
#include "models/MqttContract.h"

#include <ArduinoJson.h>

enum class DoorState : uint8_t
{
    LOCKED,
    UNLOCKED
};

enum class NetworkState : uint8_t
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

struct DeviceState
{
    DoorState door = DoorState::LOCKED;
    NetworkState wifi = NetworkState::DISCONNECTED;
    NetworkState mqtt = NetworkState::DISCONNECTED;
    bool bleConnected = false;

    void
    toJson(JsonObject obj) const
    {
        obj["door"] = (door == DoorState::LOCKED) ? MqttDoorState::LOCKED : MqttDoorState::UNLOCKED;

        obj["wifi"] = networkStateToString(wifi);
        obj["mqtt"] = networkStateToString(mqtt);
        obj["ble"] = bleConnected;
    }

    bool
    isWifiConnected() const
    {
        return wifi == NetworkState::CONNECTED;
    }

    bool
    isMqttConnected() const
    {
        return mqtt == NetworkState::CONNECTED;
    }

    bool
    isFullyOnline() const
    {
        return isWifiConnected() && isMqttConnected();
    }

    void
    setDoorState(DoorState state)
    {
        door = state;
    }

    void
    setWifiState(NetworkState state)
    {
        wifi = state;
    }

    void
    setMqttState(NetworkState state)
    {
        mqtt = state;
    }

    void
    setBleState(bool connected)
    {
        bleConnected = connected;
    }

  private:
    static const char*
    networkStateToString(NetworkState state)
    {
        switch (state)
        {
            case NetworkState::DISCONNECTED:
                return "disconnected";
            case NetworkState::CONNECTING:
                return "connecting";
            case NetworkState::CONNECTED:
                return "connected";
            default:
                return "unknown";
        }
    }
};