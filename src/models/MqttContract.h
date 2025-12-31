#pragma once

namespace MqttDoorState
{
static constexpr const char* LOCKED = "locked";
static constexpr const char* UNLOCKED = "unlocked";
} // namespace MqttDoorState

namespace MqttDoorEvent
{
static constexpr const char* LOCK = "lock";
static constexpr const char* UNLOCK = "unlock";
} // namespace MqttDoorEvent

namespace MqttSource
{
static constexpr const char* AUTO = "auto";
static constexpr const char* MANUAL = "manual";
} // namespace MqttSource
