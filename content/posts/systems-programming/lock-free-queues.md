---
title: Lock-Free Queues and the Art of Atomic Operations
date: 2024-05-20
slug: lock-free-queues
tags: cpp, concurrency, performance, systems
excerpt: Mutexes are easy to reason about but hard on your cache lines. Lock-free data structures flip that trade-off.
---

Every time a thread acquires a mutex, it potentially:

1. Enters the kernel via `futex`
2. Gets descheduled if the lock is contended
3. Invalidates cache lines across cores
4. Wakes up at the OS scheduler's leisure

For a logging system processing millions of events per second, that's unacceptable.

## The SPSC Ring Buffer

The simplest lock-free structure is a single-producer, single-consumer (SPSC) ring buffer. No CAS loops, no ABA problems — just two atomic indices.

```cpp
template<typename T, std::size_t N>
class SPSCQueue
{
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

public:
    bool push(const T& item)
    {
        auto head = head_.load(std::memory_order_relaxed);
        auto next = (head + 1) & (N - 1);

        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full

        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item)
    {
        auto tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire))
            return false;  // empty

        item = buffer_[tail];
        tail_.store((tail + 1) & (N - 1), std::memory_order_release);
        return true;
    }

private:
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, N> buffer_;
};
```

Key details:

- **Power-of-2 size**: `& (N-1)` replaces `% N` — no division
- **`alignas(64)`**: puts head and tail on different cache lines, preventing false sharing
- **Memory ordering**: `release` on the writer, `acquire` on the reader — minimal fence overhead

## MPSC: Multiple Producers

When multiple threads push but one pops (common in logging), we need compare-and-swap:

```cpp
bool push(const T& item)
{
    auto head = head_.load(std::memory_order_relaxed);
    std::size_t next;
    do {
        next = (head + 1) & (N - 1);
        if (next == tail_.load(std::memory_order_acquire))
            return false;
    } while (!head_.compare_exchange_weak(head, next,
                std::memory_order_acq_rel));

    buffer_[head] = item;
    // Need to signal completion...
}
```

This gets tricky fast. The CAS loop handles contention, but there's a window between reserving the slot (`compare_exchange`) and writing the data. The consumer needs to know when the write is complete.

## The Lesson

Lock-free programming is not about being faster — it's about being more predictable. A mutex-based queue might have lower average latency, but the lock-free version has bounded worst-case latency. Choose based on what your system needs.
