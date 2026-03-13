#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstddef>

namespace loom
{
    class ThreadPool
    {
    public:
        explicit ThreadPool(size_t num_threads);
        ~ThreadPool();

        void enqueue(std::function<void()> task);
        void shutdown();
        size_t size() const { return workers_.size(); }

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
    };
}
