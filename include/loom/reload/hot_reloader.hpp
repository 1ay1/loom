#pragma once

#include "change_event.hpp"
#include "watch_policy.hpp"
#include "../content/content_source.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace loom
{

// Thread-safe, atomically-swappable cache.
// Readers grab a shared_ptr snapshot — zero contention on the read path.
// Writers build a new cache and swap in the pointer.
template<typename T>
class AtomicCache
{
public:
    explicit AtomicCache(std::shared_ptr<const T> initial)
    {
        std::lock_guard lock(mu_);
        data_ = std::move(initial);
    }

    // Fast read: grab a snapshot (shared_ptr copy is thread-safe)
    std::shared_ptr<const T> load() const
    {
        std::lock_guard lock(mu_);
        return data_;
    }

    // Swap in a new cache
    void store(std::shared_ptr<const T> next)
    {
        std::lock_guard lock(mu_);
        data_ = std::move(next);
    }

private:
    mutable std::mutex mu_;
    std::shared_ptr<const T> data_;
};

// HotReloader: polls a WatchPolicy on a background thread and invokes a
// rebuild callback when changes are detected.
//
// Type parameters:
//   Source  — a ContentSource & Reloadable type (e.g. FileSystemSource)
//   Watcher — a WatchPolicy type (e.g. InotifyWatcher, NullWatcher)
//
// The rebuild callback receives the source (post-reload) and the change set,
// and returns a new cache value.
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader
{
public:
    using RebuildFn = std::function<std::shared_ptr<const Cache>(Source&, const ChangeSet&)>;

    HotReloader(
        Source& source,
        Watcher& watcher,
        AtomicCache<Cache>& cache,
        RebuildFn rebuild,
        std::chrono::milliseconds debounce = std::chrono::milliseconds(500)
    )
        : source_(source)
        , watcher_(watcher)
        , cache_(cache)
        , rebuild_(std::move(rebuild))
        , debounce_(debounce)
    {}

    ~HotReloader()
    {
        stop();
    }

    HotReloader(const HotReloader&) = delete;
    HotReloader& operator=(const HotReloader&) = delete;

    void start()
    {
        if (running_.exchange(true))
            return;

        watcher_.start();

        thread_ = std::thread([this]
        {
            ChangeSet pending;

            while (running_.load())
            {
                auto changes = watcher_.poll();
                if (changes)
                {
                    pending = pending | *changes;

                    // Debounce: wait a bit for more events to accumulate
                    std::this_thread::sleep_for(debounce_);

                    // Drain any further events that arrived during debounce
                    while (auto more = watcher_.poll())
                        pending = pending | *more;

                    try
                    {
                        std::cout << "[hot-reload] Changes detected"
                                  << (pending.config ? " [config]" : "")
                                  << (pending.theme  ? " [theme]"  : "")
                                  << (pending.posts  ? " [posts]"  : "")
                                  << (pending.pages  ? " [pages]"  : "")
                                  << " — rebuilding..." << std::endl;

                        source_.reload(pending);
                        auto new_cache = rebuild_(source_, pending);
                        cache_.store(std::move(new_cache));

                        std::cout << "[hot-reload] Rebuild complete." << std::endl;
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "[hot-reload] Rebuild failed: " << e.what() << std::endl;
                    }

                    pending = {};
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }

    void stop()
    {
        if (!running_.exchange(false))
            return;

        if (thread_.joinable())
            thread_.join();

        watcher_.stop();
    }

private:
    Source& source_;
    Watcher& watcher_;
    AtomicCache<Cache>& cache_;
    RebuildFn rebuild_;
    std::chrono::milliseconds debounce_;

    std::atomic<bool> running_{false};
    std::thread thread_;
};

}
