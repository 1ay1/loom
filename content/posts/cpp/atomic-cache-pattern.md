---
title: The Atomic Cache Pattern — Lock-Free Reads with shared_ptr
date: 2025-11-24
slug: atomic-cache-pattern
tags: cpp, internals, architecture
excerpt: Loom serves HTML from a shared_ptr<const SiteCache>. Reads and writes are lock-free using C++20's std::atomic<shared_ptr>. Here's why this works and when to use it.
---

Loom has two concurrent activities: the HTTP server reading the cache to serve requests, and the hot-reloader writing a new cache when content changes. Getting this concurrency right requires neither too much locking (which stalls requests) nor too little (which causes data races).

The solution is the atomic cache pattern: a `shared_ptr` inside `std::atomic` — lock-free reads, lock-free writes, automatic lifetime management.

## The Problem

Naive approach: a global `SiteCache*` that the hot reloader swaps.

```cpp
// Don't do this
SiteCache* g_cache = build_cache(...);

void handle_request(Request& req) {
    auto it = g_cache->pages.find(req.path); // data race!
    // ...
}

void rebuild() {
    SiteCache* new_cache = build_cache(...);
    delete g_cache;   // old cache freed while requests might use it
    g_cache = new_cache; // non-atomic pointer swap
}
```

Two races: the pointer write in `rebuild` is not atomic, and requests using the old cache might still be running when `delete` happens.

## The shared_ptr Solution

```cpp
template<typename T>
class AtomicCache {
    std::atomic<std::shared_ptr<const T>> data_;

public:
    std::shared_ptr<const T> load() const noexcept {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept {
        data_.store(std::move(next), std::memory_order_release);
    }
};
```

**`load()`:** Atomically loads the `shared_ptr` and increments the reference count. The caller now holds a snapshot. Even if `store()` runs immediately after, the old cache stays alive as long as someone holds a `shared_ptr` to it.

**`store()`:** Atomically swaps the stored pointer. The old `shared_ptr`'s reference count drops — if no readers hold a copy, the `SiteCache` destructor runs immediately. If readers do hold copies, destruction waits until they're all done.

C++20's `std::atomic<std::shared_ptr<T>>` handles the coordination internally. On x86_64 with GCC, this compiles to a spinlock-protected pointer swap — not a true lock-free CAS on the 128-bit `shared_ptr`, but no OS mutex involvement. The critical section is a few pointer operations: nanoseconds.

## Why No Mutex Is Needed

The previous version used an explicit `std::mutex` around the `shared_ptr`. This worked but had two costs: a syscall if the mutex was contended, and a memory barrier even when it wasn't. `std::atomic<shared_ptr>` expresses the same intent with less overhead — the compiler and runtime choose the minimal synchronisation needed.

The key insight remains the same. After `load()` returns, the caller uses the cache with no synchronisation at all:

```cpp
auto snap = cache.load();                 // atomic — nanoseconds
auto it = snap->pages.find(req.path);     // no lock — safe because const + alive
return serve_cached(it->second, req, snap); // no lock — pointer into pre-built wire data
// snap goes out of scope here — refcount drops, may free if cache was rebuilt
```

This is safe because:
1. The cache is `const` — nobody modifies it after construction
2. The caller's `shared_ptr` keeps it alive
3. The pointer won't change under the caller's feet because they use their own copy

## Why const Matters

`shared_ptr<const SiteCache>` is intentional. The `const` means no mutation is possible through this pointer. All renderers receive `const Site&`, all cache lookups use `const` accessors. This makes the "multiple concurrent readers" part trivially safe — readers don't need to synchronise with each other, only with the (rare) writer.

If the cache were mutable, every `cache->pages.find()` would need a reader lock (like `std::shared_mutex`). The cache is built once and never modified, so `const` eliminates that entire category of concern.

## Reference Counting Under the Hood

`std::shared_ptr` maintains two reference counts: strong (number of `shared_ptr`s) and weak (number of `weak_ptr`s). The managed object is destroyed when the strong count reaches zero.

When `load()` copies the `shared_ptr`, the strong count increments. When the request handler's local `shared_ptr` goes out of scope, the count decrements. `store()` decrements the count for the old pointer stored in `ptr_`. If no requests are active, the old cache is freed immediately. If requests are active, it lives until the last one finishes.

This is automatic reference counting with deterministic destruction — no garbage collector, no indeterminate lifetime.

## The Trade-off: atomic<shared_ptr> vs Mutex

The previous version of Loom used an explicit `std::mutex` to protect the `shared_ptr`. This is portable and explicit about what's being protected. The current version uses `std::atomic<std::shared_ptr<T>>` (C++20) for two reasons:

1. **No syscall path.** A `std::mutex` can invoke `futex()` under contention. The atomic version uses a userspace spinlock — it never enters the kernel.
2. **Semantic clarity.** `std::atomic<shared_ptr>` says "this pointer is accessed concurrently" at the type level. A raw `shared_ptr` + `mutex` requires the reader to understand the locking protocol.

For Loom's use case — rebuilds happen at most once per 500ms, reads happen thousands of times per second — the performance difference is small. The design difference is what matters: the type system enforces the concurrency contract.

## When To Use This Pattern

The atomic cache pattern is the right choice when:

1. **Reads are frequent, writes are rare.** A blog cache rebuilds maybe once a minute at most.
2. **The data is large and immutable.** Copying the whole cache on every request would be wasteful; sharing an immutable snapshot via `shared_ptr` is O(1).
3. **Write latency doesn't matter.** Building the new cache takes 10–100ms; nobody is waiting on that.
4. **Readers should never block.** HTTP request handling must not stall for cache rebuilds.

If writes were frequent (multiple per second), consider `std::shared_mutex` (multiple readers, exclusive writer) instead. If the data were small, an `std::atomic<T>` directly might work. For Loom's specific shape — infrequent large-write, constant small-read — `shared_ptr` + `mutex` is ideal.

## Visualising the Lifecycle

```
t=0:  store(cache_v1)       ptr_ → cache_v1 [ref=1]

t=1:  load()                ptr_ → cache_v1 [ref=2]
      → req1 holds shared_ptr to cache_v1

t=2:  load()                ptr_ → cache_v1 [ref=3]
      → req2 holds shared_ptr to cache_v1

t=3:  store(cache_v2)       ptr_ → cache_v2 [ref=1]
                             cache_v1 [ref=2] ← req1 and req2 still alive

t=4:  req1 done             cache_v1 [ref=1]
t=5:  req2 done             cache_v1 [ref=0] → DESTROYED
```

From t=3 onwards, new requests get `cache_v2`. Old requests finish on `cache_v1`. The old cache is destroyed only when the last request holding it completes.
