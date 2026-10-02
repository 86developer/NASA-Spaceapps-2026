#pragma once

#include "../models/ridge_payload.hpp"
#include <vector>
#include <mutex>
#include <cstddef>

namespace vitalgrid {

/**
 * @brief High-rate Circular Ring Buffer for mission-critical flight telemetry
 * Thread-safe for multi-producer / multi-consumer watchdog access
 */
class CircularTelemetryBuffer {
public:
    explicit CircularTelemetryBuffer(size_t capacity = 1024)
        : capacity_(capacity), buffer_(capacity), head_(0), count_(0) {}

    void push(const RidgePayload& sample) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[head_] = sample;
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) {
            ++count_;
        }
    }

    /**
     * @brief Snapshots historical records in chronological order
     */
    std::vector<RidgePayload> get_history() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<RidgePayload> history;
        history.reserve(count_);
        if (count_ == 0) return history;

        size_t start = (head_ + capacity_ - count_) % capacity_;
        for (size_t i = 0; i < count_; ++i) {
            history.push_back(buffer_[(start + i) % capacity_]);
        }
        return history;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        count_ = 0;
    }

private:
    size_t capacity_;
    std::vector<RidgePayload> buffer_;
    size_t head_;
    size_t count_;
    mutable std::mutex mutex_;
};

} // namespace vitalgrid
