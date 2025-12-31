#pragma once
#include "models/Command.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <queue>

class CommandQueue
{
  public:
    CommandQueue(size_t maxSize = 20) : maxSize_(maxSize)
    {
        mutex_ = xSemaphoreCreateMutex();
    }

    ~CommandQueue()
    {
        if (mutex_)
        {
            vSemaphoreDelete(mutex_);
        }
    }

    bool
    enqueue(const Command& cmd)
    {
        if (!mutex_)
            return false;

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
            return false;

        bool success = false;
        if (queue_.size() < maxSize_)
        {
            queue_.push(cmd);
            success = true;
        }

        xSemaphoreGive(mutex_);
        return success;
    }

    bool
    dequeue(Command& cmd)
    {
        if (!mutex_)
            return false;

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
            return false;

        bool success = false;
        if (!queue_.empty())
        {
            cmd = queue_.front();
            queue_.pop();
            success = true;
        }

        xSemaphoreGive(mutex_);
        return success;
    }

    bool
    isEmpty()
    {
        if (!mutex_)
            return true;

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
            return true;

        bool empty = queue_.empty();
        xSemaphoreGive(mutex_);
        return empty;
    }

    size_t
    size()
    {
        if (!mutex_)
            return 0;

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
            return 0;

        size_t s = queue_.size();
        xSemaphoreGive(mutex_);
        return s;
    }

    void
    clear()
    {
        if (!mutex_)
            return;

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
            return;

        while (!queue_.empty())
        {
            queue_.pop();
        }

        xSemaphoreGive(mutex_);
    }

  private:
    std::queue<Command> queue_;
    SemaphoreHandle_t mutex_;
    size_t maxSize_;
};