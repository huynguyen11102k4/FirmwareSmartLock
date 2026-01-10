#pragma once
#include "models/Command.h"

#include <Arduino.h>
#include <queue>

class CommandQueue
{
  public:
    CommandQueue(size_t maxSize = 20) : maxSize_(maxSize) {}

    bool enqueue(const Command& cmd)
    {
        if (queue_.size() >= maxSize_)
            return false;

        queue_.push(cmd);
        return true;
    }

    bool dequeue(Command& cmd)
    {
        if (queue_.empty())
            return false;

        cmd = queue_.front();
        queue_.pop();
        return true;
    }

    void clear()
    {
        while (!queue_.empty())
            queue_.pop();
    }

    size_t size() const
    {
        return queue_.size();
    }

  private:
    std::queue<Command> queue_;
    size_t maxSize_;
};