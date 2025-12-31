#pragma once
#include <Arduino.h>

enum class DoorFSMState : uint8_t
{
    LOCKED_CLOSED,   // Normal locked state
    LOCKED_OPEN,     // ALARM! Door forced open
    UNLOCKED_CLOSED, // Door unlocked, waiting for open
    UNLOCKED_OPEN    // Normal operation: door opened after unlock
};

enum class DoorEvent : uint8_t
{
    LOCK_COMMAND,
    UNLOCK_COMMAND,
    DOOR_OPENED,
    DOOR_CLOSED,
    TIMEOUT
};

class DoorStateMachine
{
  public:
    using StateChangeCallback =
        std::function<void(DoorFSMState from, DoorFSMState to, DoorEvent event)>;

    DoorStateMachine() : state_(DoorFSMState::LOCKED_CLOSED), alarmActive_(false)
    {
    }

    void
    setCallback(StateChangeCallback cb)
    {
        callback_ = cb;
    }

    DoorFSMState
    getState() const
    {
        return state_;
    }

    bool
    isAlarmActive() const
    {
        return alarmActive_;
    }

    void
    handleEvent(DoorEvent event)
    {
        DoorFSMState oldState = state_;

        switch (state_)
        {
            case DoorFSMState::LOCKED_CLOSED:
                if (event == DoorEvent::UNLOCK_COMMAND)
                {
                    state_ = DoorFSMState::UNLOCKED_CLOSED;
                }
                else if (event == DoorEvent::DOOR_OPENED)
                {
                    // ALARM: Door forced open while locked!
                    state_ = DoorFSMState::LOCKED_OPEN;
                    alarmActive_ = true;
                }
                break;

            case DoorFSMState::LOCKED_OPEN:
                if (event == DoorEvent::DOOR_CLOSED)
                {
                    state_ = DoorFSMState::LOCKED_CLOSED;
                    alarmActive_ = false;
                }
                else if (event == DoorEvent::UNLOCK_COMMAND)
                {
                    // Clear alarm if user unlocks
                    state_ = DoorFSMState::UNLOCKED_OPEN;
                    alarmActive_ = false;
                }
                break;

            case DoorFSMState::UNLOCKED_CLOSED:
                if (event == DoorEvent::DOOR_OPENED)
                {
                    state_ = DoorFSMState::UNLOCKED_OPEN;
                }
                else if (event == DoorEvent::LOCK_COMMAND || event == DoorEvent::TIMEOUT)
                {
                    state_ = DoorFSMState::LOCKED_CLOSED;
                }
                break;

            case DoorFSMState::UNLOCKED_OPEN:
                if (event == DoorEvent::DOOR_CLOSED)
                {
                    // Auto-lock when door closes
                    state_ = DoorFSMState::LOCKED_CLOSED;
                }
                else if (event == DoorEvent::LOCK_COMMAND)
                {
                    // Manual lock while door is open
                    state_ = DoorFSMState::LOCKED_OPEN;
                }
                break;
        }

        if (oldState != state_ && callback_)
        {
            callback_(oldState, state_, event);
        }
    }

    const char*
    getStateString() const
    {
        switch (state_)
        {
            case DoorFSMState::LOCKED_CLOSED:
                return "locked_closed";
            case DoorFSMState::LOCKED_OPEN:
                return "locked_open_ALARM";
            case DoorFSMState::UNLOCKED_CLOSED:
                return "unlocked_closed";
            case DoorFSMState::UNLOCKED_OPEN:
                return "unlocked_open";
            default:
                return "unknown";
        }
    }

    void
    reset()
    {
        state_ = DoorFSMState::LOCKED_CLOSED;
        alarmActive_ = false;
    }

  private:
    DoorFSMState state_;
    bool alarmActive_;
    StateChangeCallback callback_;
};