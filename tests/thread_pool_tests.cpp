
#include "thread_pool.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

void testTaskExecution() {
    ThreadPool pool(2, 10);

    std::atomic<int> counter{0};

    for (int i = 0; i < 5; i++) {
        bool accepted = pool.submit([&counter]() {
            counter++;
        });

        assert(accepted);
    }

    pool.shutdown();

    assert(counter == 5);

    std::cout << "Task execution test passed\n";
}

void testSubmissionAfterShutdown() {
    ThreadPool pool(2, 10);

    pool.shutdown();

    bool accepted = pool.submit([]() {});

    assert(!accepted);

    std::cout << "Post-shutdown submission test passed\n";
}

void testGracefulShutdown() {
    ThreadPool pool(2, 10);

    std::atomic<int> counter{0};

    for (int i = 0; i < 5; i++) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(20ms);
            counter++;
        });
    }

    pool.shutdown();

    assert(counter == 5);

    std::cout << "Graceful shutdown test passed\n";
}

void testExceptionHandling() {
    ThreadPool pool(1, 10);

    std::atomic<int> counter{0};

    pool.submit([]() {
        throw std::runtime_error("Test exception");
    });

    pool.submit([&counter]() {
        counter++;
    });

    pool.shutdown();

    assert(counter == 1);

    std::cout << "Exception handling test passed\n";
}

void testParallelExecution() {
    ThreadPool pool(4, 10);

    std::atomic<int> activeWorkers{0};
    std::atomic<int> maxConcurrent{0};

    for (int i = 0; i < 4; i++) {
        pool.submit([&]() {
            int current = ++activeWorkers;

            int previous = maxConcurrent.load();

            while (current > previous &&
                   !maxConcurrent.compare_exchange_weak(previous, current)) {
            }

            // Keep each task active briefly so workers can overlap.
            std::this_thread::sleep_for(200ms);

            --activeWorkers;
        });
    }

    pool.shutdown();

    assert(maxConcurrent >= 2);

    std::cout << "Parallel execution test passed. "
              << "Maximum concurrent tasks: "
              << maxConcurrent << '\n';
}


void testQueueCapacity() {
    ThreadPool pool(1, 2);

    std::atomic<bool> taskStarted{false};
    std::atomic<bool> releaseTask{false};

    // Task 1 occupies the only worker.
    bool first = pool.submit([&]() {
        taskStarted = true;

        while (!releaseTask.load()) {
            std::this_thread::yield();
        }
    });

    assert(first);

    // Wait until the worker is executing Task 1.
    while (!taskStarted.load()) {
        std::this_thread::yield();
    }

    // The worker is busy, so these two tasks fill the queue.
    bool second = pool.submit([]() {});
    bool third = pool.submit([]() {});

    // The queue is full. This task must be rejected.
    bool fourth = pool.submit([]() {});

    assert(second);
    assert(third);
    assert(!fourth);

    // Release the worker so shutdown can complete.
    releaseTask = true;

    pool.shutdown();

    std::cout << "Queue capacity test passed\n";
}

int main() {
    testTaskExecution();
    testSubmissionAfterShutdown();
    testGracefulShutdown();
    testExceptionHandling();
    testParallelExecution(); 
    testQueueCapacity(); 

    std::cout << "\nAll thread pool tests passed!\n";

    return 0;
}
