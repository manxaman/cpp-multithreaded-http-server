#pragma once
#include <atomic>
#include "task_queue.hpp"
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

class ThreadPool {
public:
    ThreadPool(std::size_t threadCount, std::size_t queueCapacity);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool submit(std::function<void()> task);
    void shutdown();

private:
    TaskQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopped_{false};
};
