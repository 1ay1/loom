---
title: "Atomics, Threads, and Memory Ordering — Concurrency Without Locks"
date: 2025-10-17
slug: atomics-threads-and-memory-ordering
tags: cpp, atomics, concurrency, memory-ordering, threads
excerpt: Your CPU reorders your code. Atomics are how you tell it to stop.
---

Loom is a single-threaded event loop for HTTP serving. But it has one piece of concurrency that matters enormously: the hot reloader. A background thread watches for file changes, rebuilds the site cache, and publishes it for the HTTP thread to use. Getting this right — without mutexes, without blocking the HTTP thread, without data races — requires understanding atomics and memory ordering.

## std::thread: The Basics

Creating a thread is simple:

```cpp
#include <thread>

std::thread worker([this] {
    while (running_.load()) {
        auto changes = watcher_.poll();
        // ... process changes ...
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
});

// Later:
worker.join();  // wait for the thread to finish
```

This is from Loom's `HotReloader`. The lambda runs on a new thread. It polls for changes, rebuilds if needed, and sleeps. The main thread continues running the event loop. When the server shuts down, `stop()` sets `running_` to false and joins the thread.

The join is mandatory. If a `std::thread` is destroyed while still running (without being joined or detached), the program calls `std::terminate`. This is RAII for threads — the destructor enforces that you handle the thread's completion.

## std::atomic<bool>: The Simplest Atomic

The `running_` flag is shared between the HTTP thread and the hot-reload thread. Reading and writing a regular `bool` from two threads is a data race — undefined behavior. `std::atomic<bool>` makes the reads and writes atomic:

```cpp
class HotReloader {
    std::atomic<bool> running_{false};
    // ...
};

// Hot-reload thread:
while (running_.load()) { /* ... */ }

// Main thread:
running_.store(false);  // signal shutdown
```

`load()` reads the value atomically. `store(value)` writes it atomically. No torn reads, no torn writes, no data race.

The `exchange` operation is even more useful — it atomically reads the old value and writes a new one:

```cpp
void start() {
    if (running_.exchange(true))  // was already true?
        return;                     // already running, bail out
    // ... start the thread ...
}

void stop() {
    if (!running_.exchange(false))  // was already false?
        return;                      // already stopped, bail out
    // ... join the thread ...
}
```

This pattern ensures `start()` and `stop()` are idempotent. Even if called from multiple threads, the `exchange` is atomic — exactly one call sees the "before" value and proceeds.

Loom's HTTP server uses the same pattern for its shutdown flag:

```cpp
static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running.store(false);
}

// Event loop:
while (running_.load() && g_running.load()) {
    int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
    // ...
}
```

The signal handler sets `g_running` to false. The event loop checks it on each iteration. The atomic ensures the store from the signal handler is visible to the event loop thread.

## std::atomic<std::shared_ptr<T>>: C++20's Game Changer

C++20 introduced `std::atomic<std::shared_ptr<T>>` — the ability to atomically load and store shared pointers. This is the foundation of Loom's cache pattern:

```cpp
template<typename T>
class AtomicCache {
public:
    explicit AtomicCache(std::shared_ptr<const T> initial)
        : data_(std::move(initial)) {}

    std::shared_ptr<const T> load() const noexcept {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept {
        data_.store(std::move(next), std::memory_order_release);
    }

private:
    std::atomic<std::shared_ptr<const T>> data_;
};
```

The HTTP thread calls `load()` to get a snapshot of the current cache. The hot-reload thread calls `store()` to publish a new cache. Both operations are atomic — no mutex, no locking, no blocking.

Here's why this is powerful: the HTTP thread gets a `shared_ptr<const SiteCache>`. Even if the hot-reload thread stores a new cache a nanosecond later, the HTTP thread still holds a valid reference to the old cache. The old cache stays alive (reference counting) until the last HTTP request using it finishes. Then it's automatically destroyed.

No locks. No contention. No stale data. No use-after-free.

## Memory Ordering: Why It Matters

Now we need to talk about something that most programmers don't know about: CPU reordering.

Modern CPUs don't execute instructions in the order you wrote them. They reorder loads and stores for performance — executing whatever is ready first, buffering writes, speculating on branches. This is invisible on a single thread (the CPU maintains the *illusion* of sequential execution). But on multiple threads, it creates chaos.

Consider this (pseudocode):

```
// Thread 1                    // Thread 2
cache = build_new_cache();     auto c = cache.load();
ready.store(true);             if (ready.load()) {
                                   c->use();  // might see old cache!
                               }
```

Without memory ordering, Thread 2 might see `ready == true` but still read the *old* cache pointer, because the CPU reordered Thread 1's store to `cache` after its store to `ready`.

Memory ordering tells the CPU how much reordering is allowed:

### memory_order_seq_cst (Sequential Consistency)

The default. All threads see all operations in a single, consistent order. Safest but potentially slowest:

```cpp
running_.store(false);  // implicitly seq_cst
// Equivalent to:
running_.store(false, std::memory_order_seq_cst);
```

### memory_order_acquire / memory_order_release

The pair that Loom uses. `release` on a store means: "all writes I did before this store are visible to any thread that does an `acquire` load of the same variable."

```cpp
// Hot-reload thread (writer):
auto new_cache = rebuild_everything();  // lots of writes
data_.store(new_cache, std::memory_order_release);
//     ↑ release: all the writes above are visible to acquirers

// HTTP thread (reader):
auto cache = data_.load(std::memory_order_acquire);
//     ↑ acquire: sees all writes that happened before the release
auto html = cache->render(request);  // safe to read cache contents
```

The `release` store in the hot-reload thread creates a *happens-before* relationship with the `acquire` load in the HTTP thread. All the memory writes that built the new cache (allocating strings, filling vectors, computing HTML) are guaranteed visible to the HTTP thread when it loads the new pointer.

This is weaker than `seq_cst` (it doesn't establish a total order across all atomic operations) but sufficient for the publish-subscribe pattern. And it's cheaper — on x86, acquire and release are essentially free (the hardware already provides these guarantees). On ARM, they map to cheaper fence instructions than seq_cst.

### memory_order_relaxed

No ordering guarantees at all. The atomic operation is still atomic (no torn reads/writes), but it can be reordered freely:

```cpp
counter_.fetch_add(1, std::memory_order_relaxed);
```

Use this only when you don't care about ordering — pure counters, statistics, cases where you just need a consistent increment but don't need to synchronize other data.

## The Atomic Cache Pattern in Practice

Here's how Loom assembles all the pieces:

```cpp
// In the hot reloader:
void start() {
    watcher_.start();
    thread_ = std::thread([this] {
        ChangeSet pending;
        while (running_.load()) {
            auto changes = watcher_.poll();
            if (changes) {
                pending = pending | *changes;
                std::this_thread::sleep_for(debounce_);  // debounce

                // Drain events that arrived during debounce
                while (auto more = watcher_.poll())
                    pending = pending | *more;

                source_.reload(pending);
                auto new_cache = rebuild_(source_, pending);
                cache_.store(std::move(new_cache));  // atomic publish
                pending = {};
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}
```

The pattern:
1. Poll for changes (non-blocking)
2. Debounce (wait for more changes to accumulate)
3. Reload the source (re-read files from disk)
4. Rebuild the cache (render all HTML, pre-serialize responses)
5. Atomically publish the new cache

The HTTP thread, meanwhile, is in its event loop:

```cpp
auto cache = cache_.load();  // atomic acquire
auto response = dispatch_request(request, cache);
// cache keeps the old snapshot alive via shared_ptr
```

Two threads. No mutexes. No blocking. The hot-reload thread writes to the cache pointer. The HTTP thread reads from it. The atomic shared_ptr ensures both operations are safe.

## Common Pitfalls

**Don't use atomics for complex operations.** An atomic can protect a single variable, not a set of related variables. If you need to update two variables atomically, use a mutex or the atomic shared_ptr pattern (group the variables into a struct, swap the whole struct atomically).

**Don't use relaxed ordering unless you know exactly why.** Acquire/release is the correct default for inter-thread communication. Relaxed is for counters and statistics where you only need atomicity, not synchronization.

**Don't forget the const.** In `shared_ptr<const T>`, the `const` is load-bearing. Without it, multiple threads could read the same cache and one could mutate it — a data race. The `const` makes the shared data immutable, and immutable data is always thread-safe.

**Don't roll your own atomics.** If `std::atomic` doesn't support the type you want (it handles integral types, pointers, `bool`, and since C++20, `shared_ptr`), restructure your data so it can use atomic operations on supported types. Custom lock-free algorithms are among the hardest things to get right in all of computer science.

## The Design Principle

Loom's concurrency model is simple: one thread does I/O (the event loop), one thread does background work (the hot reloader), and they communicate through an atomic shared pointer. No thread pool, no async/await, no lock-free queues. Just two threads and one atomic.

This works because the workloads are asymmetric. The HTTP thread handles thousands of requests per second but each request is fast (cache lookup + write). The hot-reload thread runs infrequently (when files change) but does heavy work (re-parse, re-render, re-serialize). The atomic shared pointer lets them run independently, sharing only the final result.

Most servers don't need complex concurrency primitives. They need one fast path (the event loop) and one slow path (background tasks), with a clean handoff between them. Atomics and shared_ptr provide that handoff.

Next: epoll — how the event loop handles thousands of connections on a single thread.
