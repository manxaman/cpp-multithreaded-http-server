
#include "thread_pool.hpp"

#include <algorithm>
#include <exception>
#include <iostream>

ThreadPool::ThreadPool(
    std::size_t threadCount,
    std::size_t queueCapacity
) : tasks_(queueCapacity) {

    threadCount = std::max<std::size_t>(1, threadCount);

    for (std::size_t i = 0; i < threadCount; ++i) {

        workers_.emplace_back([this]() {

            std::function<void()> task;

            while (tasks_.pop(task)) {

                try {
                    task();
                }
                catch (const std::exception& e) {
                    std::cerr << "Worker task error: "
                              << e.what() << '\n';
                }
                catch (...) {
                    std::cerr << "Unknown worker task error\n";
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::submit(std::function<void()> task) {

    if (stopped_.load()) {
        return false;
    }

    return tasks_.tryPush(std::move(task));
}
void ThreadPool::shutdown() {

    if (stopped_.exchange(true)) {
        return;
    }

    // Close the queue and wake all waiting workers.
    tasks_.close();

    // Wait for workers to finish their remaining tasks.
    for (auto& worker : workers_) {

        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
}