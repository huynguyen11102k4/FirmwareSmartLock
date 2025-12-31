#pragma once
#include <Arduino.h>

class RetryPolicy
{
  public:
    RetryPolicy(uint32_t baseDelayMs, uint32_t maxDelayMs)
        : baseDelayMs_(baseDelayMs ? baseDelayMs : 1),
          maxDelayMs_(maxDelayMs >= baseDelayMs_ ? maxDelayMs : baseDelayMs_)
    {
        reset();
    }

    void
    reset()
    {
        attemptCount_ = 0;
        currentDelayMs_ = baseDelayMs_;
        lastAttemptMs_ = 0;
    }

    bool
    shouldRetry() const
    {
        if (attemptCount_ == 0)
            return true;

        return (uint32_t)(millis() - lastAttemptMs_) >= currentDelayMs_;
    }

    void
    recordAttempt()
    {
        lastAttemptMs_ = millis();
        attemptCount_++;

        if (attemptCount_ <= 1)
        {
            currentDelayMs_ = baseDelayMs_;
            return;
        }

        const uint64_t next = (uint64_t)currentDelayMs_ * 2ULL;
        currentDelayMs_ = (next > maxDelayMs_) ? maxDelayMs_ : (uint32_t)next;
    }

    int
    getAttemptCount() const
    {
        return attemptCount_;
    }

    uint32_t
    getCurrentDelay() const
    {
        return currentDelayMs_;
    }

  private:
    uint32_t baseDelayMs_;
    uint32_t maxDelayMs_;
    int attemptCount_{0};
    uint32_t currentDelayMs_{0};
    uint32_t lastAttemptMs_{0};
};