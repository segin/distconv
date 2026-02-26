#ifndef BACKOFF_TIMER_H
#define BACKOFF_TIMER_H

#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

namespace distconv {
namespace TranscodingEngine {

class BackoffTimer {
public:
    BackoffTimer(std::chrono::milliseconds min_delay, std::chrono::milliseconds max_delay)
        : min_delay_(min_delay), max_delay_(max_delay), current_delay_(min_delay),
          generator_(std::random_device{}()) {}

    void sleep() {
        std::this_thread::sleep_for(current_delay_);

        // Calculate next delay
        long long current_ms = current_delay_.count();
        long long next_ms = current_ms * 2;

        // Add jitter: +/- 10%
        long long jitter_range = next_ms / 10;
        if (jitter_range > 0) {
            std::uniform_int_distribution<long long> dist(-jitter_range, jitter_range);
            next_ms += dist(generator_);
        }

        // Clamp to range [min_delay, max_delay]
        if (next_ms > max_delay_.count()) {
            next_ms = max_delay_.count();
        }
        if (next_ms < min_delay_.count()) {
            next_ms = min_delay_.count();
        }

        current_delay_ = std::chrono::milliseconds(next_ms);
    }

    void reset() {
        current_delay_ = min_delay_;
    }

    // Helper for testing
    std::chrono::milliseconds get_current_delay() const {
        return current_delay_;
    }

private:
    std::chrono::milliseconds min_delay_;
    std::chrono::milliseconds max_delay_;
    std::chrono::milliseconds current_delay_;
    std::mt19937 generator_;
};

} // namespace TranscodingEngine
} // namespace distconv

#endif // BACKOFF_TIMER_H
