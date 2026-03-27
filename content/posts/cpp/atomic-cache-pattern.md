---
title: The Atomic Cache Pattern — Lock-Free Reads with shared_ptr
date: 2025-11-24
slug: atomic-cache-pattern
tags: cpp, internals, architecture
excerpt: Loom serves HTML from a shared_ptr<const SiteCache>. Reads are lock-free. Writes swap the pointer under a tiny mutex. Here's why this works and when to use it.
---

Loom has two concurrent activities: the HTTP server reading the cache to serve requests, and the hot-reloader writing a new cache when content changes. Getting this concurrency right requires neither too much locking (which stalls requests) nor too little (which causes data races).

The solution is the atomic cache pattern: a `shared_ptr` protected by a mutex that's only held long enough to copy or swap the pointer.

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
    std::shared_ptr<const T> ptr_;
    mutable std::mutex       mutex_;

public:
    std::shared_ptr<const T> load() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ptr_; // copy the shared_ptr under the lock
    }

    void store(std::shared_ptr<const T> new_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        ptr_ = std::move(new_ptr);
    }
};
```

**`load()`:** Locks the mutex, copies the `shared_ptr` (this atomically increments the reference count), releases the lock. The caller now holds a `shared_ptr` to the current cache. Even if `store()` runs immediately after, the old cache stays alive as long as someone holds a `shared_ptr` to it.

**`store()`:** Locks the mutex, swaps the stored pointer. The old `shared_ptr` goes out of scope when the local variable `ptr_` is overwritten — if no readers hold a copy, the `SiteCache` destructor runs immediately. If readers do hold copies, destruction waits until they're all done.

## How The Mutex Protects Only a Pointer Copy

The mutex is held for roughly two operations:
1. `load()`: increment refcount + copy pointer (nanoseconds)
2. `store()`: swap two pointers (nanoseconds)

It is **not** held while:
- Serving a request (milliseconds)
- Building a new cache (hundreds of milliseconds)

This is the key insight. After `load()` returns, the caller uses `ptr_->pages.find(...)` with no lock held. This is safe because:
1. The cache is `const` — nobody modifies it after construction
2. The caller's `shared_ptr` keeps it alive
3. The pointer won't change under the caller's feet because they don't read `ptr_` again — they use their own copy

```cpp
// In the request handler
auto cache = atomic_cache.load();  // lock held ~nanoseconds
auto it = cache->pages.find(req.path); // no lock — safe because const + alive
return serve_cached(req, it->second);  // no lock
// cache goes out of scope here — refcount drops, may free if rebuilding
```

## Why const Matters

`shared_ptr<const SiteCache>` is intentional. The `const` means no mutation is possible through this pointer. All renderers receive `const Site&`, all cache lookups use `const` accessors. This makes the "multiple concurrent readers" part trivially safe — readers don't need to synchronise with each other, only with the (rare) writer.

If the cache were mutable, every `cache->pages.find()` would need a reader lock (like `std::shared_mutex`). The cache is built once and never modified, so `const` eliminates that entire category of concern.

## Reference Counting Under the Hood

`std::shared_ptr` maintains two reference counts: strong (number of `shared_ptr`s) and weak (number of `weak_ptr`s). The managed object is destroyed when the strong count reaches zero.

When `load()` copies the `shared_ptr`, the strong count increments. When the request handler's local `shared_ptr` goes out of scope, the count decrements. `store()` decrements the count for the old pointer stored in `ptr_`. If no requests are active, the old cache is freed immediately. If requests are active, it lives until the last one finishes.

This is automatic reference counting with deterministic destruction — no garbage collector, no indeterminate lifetime.

## The Trade-off: Copies vs Atomics

An alternative: use `std::atomic<std::shared_ptr<T>>` (C++20), which makes the `shared_ptr` itself atomic without a separate mutex:

```cpp
std::atomic<std::shared_ptr<const SiteCache>> cache_;

std::shared_ptr<const SiteCache> load() {
    return cache_.load(); // atomic
}

void store(std::shared_ptr<const SiteCache> p) {
    cache_.store(std::move(p)); // atomic
}
```

This is equivalent but relies on hardware support for atomic 128-bit operations (or a lock inside the `atomic`). The mutex version is portable and explicit about what's being protected. For Loom's use case — rebuilds happen at most once per 500ms, not in a tight loop — the difference is immeasurable.

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
