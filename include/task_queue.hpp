#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class TaskQueue {
public:
    explicit TaskQueue(std::size_t capacity) : capacity_(capacity ? capacity : 1) {}

    bool tryPush(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) return false;
        queue_.push(std::move(value));
        ready_.notify_one();
        return true;
    }

    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        ready_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool closed_{false};
};
