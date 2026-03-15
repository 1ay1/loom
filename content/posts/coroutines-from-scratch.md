---
title: Coroutines From Scratch in C++20
date: 2024-11-20
slug: coroutines-from-scratch
tags: cpp, concurrency, systems
excerpt: C++20 coroutines are the most misunderstood feature in the language. Here's how they actually work under the hood.
---

C++20 coroutines aren't goroutines. They're not green threads. They're not async/await in the way JavaScript or Python does it. They're a compiler transformation that splits a function into a state machine. Understanding the transformation is the key to using them.

## What the Compiler Does

When you write a coroutine (any function containing `co_await`, `co_yield`, or `co_return`), the compiler:

1. Allocates a **coroutine frame** on the heap (contains locals + suspension point)
2. Creates a **promise object** that controls the coroutine's behavior
3. Transforms the function body into a state machine with jump labels at each suspension point

```cpp
Generator<int> range(int start, int end)
{
    for (int i = start; i < end; ++i)
        co_yield i;
}
```

The compiler turns this into something conceptually like:

```cpp
// Pseudocode — actual transformation is more complex
struct range_frame
{
    int start, end, i;
    int suspension_point = 0;
    Generator<int>::promise_type promise;
};

void range_resume(range_frame* frame)
{
    switch (frame->suspension_point)
    {
        case 0: goto start;
        case 1: goto resume_point;
    }
start:
    for (frame->i = frame->start; frame->i < frame->end; ++frame->i)
    {
        frame->promise.yield_value(frame->i);
        frame->suspension_point = 1;
        return; // suspend
resume_point:;
    }
    frame->promise.return_void();
}
```

## The Promise Type

Every coroutine has an associated promise type that controls:

- What happens at the start (`initial_suspend`)
- What happens at the end (`final_suspend`)
- How `co_yield` and `co_return` work
- How the return object is created

```cpp
template<typename T>
struct Generator
{
    struct promise_type
    {
        T current_value;

        Generator get_return_object()
        {
            return Generator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value)
        {
            current_value = std::move(value);
            return {};
        }

        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle_;

    bool next()
    {
        handle_.resume();
        return !handle_.done();
    }

    T value() const { return handle_.promise().current_value; }

    ~Generator() { if (handle_) handle_.destroy(); }
};
```

## co_await: The Suspension Mechanism

`co_await expr` calls `expr.await_ready()`. If it returns false, the coroutine suspends:

```cpp
struct SleepAwaiter
{
    std::chrono::milliseconds duration;

    bool await_ready() const { return duration.count() <= 0; }

    void await_suspend(std::coroutine_handle<> h)
    {
        // Schedule resumption on an event loop
        event_loop::schedule(h, duration);
    }

    void await_resume() {} // Return value of co_await expression
};

Task<void> delayed_work()
{
    co_await SleepAwaiter{std::chrono::milliseconds{100}};
    // Resumes here after 100ms
    do_work();
}
```

The awaiter controls *where* and *when* the coroutine resumes. This is what makes C++ coroutines so flexible — and so complex.

## The Heap Allocation Problem

Every coroutine allocates a frame on the heap. For hot paths, this matters. Mitigations:

- **HALO (Heap Allocation eLision Optimization)**: If the compiler can prove the coroutine's lifetime is bounded by the caller, it can allocate the frame on the caller's stack. This is fragile and implementation-dependent.
- **Custom allocators**: Override `promise_type::operator new` to use a pool allocator.
- **Symmetric transfer**: `await_suspend` returns a `coroutine_handle` to resume directly, avoiding stack overflow in coroutine chains.

## When to Use Them

**Good fit**: lazy generators, async I/O with event loops, parser combinators, pipeline processing.

**Bad fit**: simple callbacks, fire-and-forget tasks, anything where the overhead of the coroutine frame matters more than the clarity benefit.

C++20 coroutines are a low-level primitive. They give you complete control over suspension and resumption semantics. The price is that you have to build (or choose) the abstractions yourself. There's no built-in event loop, no built-in task scheduler, no built-in `async`/`await` that just works. That's by design.
