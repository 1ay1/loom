---
title: Writing a Memory Allocator from Scratch
date: 2024-04-15
slug: memory-allocators
tags: cpp, systems, memory, performance
excerpt: malloc is fine until it isn't. Here's how to build a pool allocator that gives you predictable latency.
---

`malloc` and `free` work great for most programs. But when you need deterministic allocation times — real-time audio, game engines, network packet processing — the general-purpose allocator becomes a liability.

## The Problem with malloc

Every call to `malloc` potentially involves:

1. Scanning free lists
2. Coalescing free blocks
3. Calling `mmap` or `sbrk` for more memory
4. Acquiring a global lock (in multithreaded programs)

None of these have bounded execution time. In a hot path processing 10,000 packets per second, a single 50ms allocation stall is catastrophic.

## Pool Allocators

The simplest fixed-time allocator is a pool: pre-allocate N blocks of the same size, hand them out in O(1).

```cpp
template<typename T, std::size_t N>
class Pool
{
public:
    Pool()
    {
        // Build free list through the pool
        for (std::size_t i = 0; i < N - 1; ++i)
            storage_[i].next = &storage_[i + 1];
        storage_[N - 1].next = nullptr;
        free_ = &storage_[0];
    }

    T* allocate()
    {
        if (!free_) return nullptr;
        auto* block = free_;
        free_ = block->next;
        return reinterpret_cast<T*>(&block->data);
    }

    void deallocate(T* ptr)
    {
        auto* block = reinterpret_cast<Block*>(ptr);
        block->next = free_;
        free_ = block;
    }

private:
    union Block
    {
        Block* next;
        alignas(T) std::byte data[sizeof(T)];
    };

    std::array<Block, N> storage_;
    Block* free_;
};
```

Both `allocate` and `deallocate` are O(1) with zero branching on the hot path.

## Arena Allocators

When you allocate many objects with the same lifetime — processing a request, parsing a file, rendering a frame — an arena is ideal:

```cpp
class Arena
{
public:
    explicit Arena(std::size_t capacity)
        : buffer_(std::make_unique<std::byte[]>(capacity))
        , capacity_(capacity)
    {}

    void* allocate(std::size_t size, std::size_t align = alignof(std::max_align_t))
    {
        std::size_t padding = (align - (offset_ % align)) % align;
        if (offset_ + padding + size > capacity_)
            return nullptr;

        offset_ += padding;
        void* ptr = buffer_.get() + offset_;
        offset_ += size;
        return ptr;
    }

    void reset() { offset_ = 0; }

private:
    std::unique_ptr<std::byte[]> buffer_;
    std::size_t capacity_;
    std::size_t offset_ = 0;
};
```

No free list, no fragmentation, no per-object overhead. Just bump a pointer.

## When to Use What

- **Pool**: fixed-size objects with unpredictable lifetimes (connections, packets)
- **Arena**: mixed-size objects with batch lifetimes (request processing, frame rendering)
- **malloc**: everything else

The best allocator is the one you don't notice. But when you do notice — when that p99 latency spike keeps you up at night — knowing how to build your own is invaluable.
