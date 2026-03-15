---
title: C++ Templates Are a Compile-Time Functional Language
date: 2024-10-15
slug: why-cpp-templates
tags: cpp, type-systems, compilers
excerpt: Templates aren't just generics. They're a Turing-complete functional language that runs at compile time. Here's why that matters.
---

Java generics erase types at runtime. Rust generics monomorphize. C++ templates do something wilder — they're a full compile-time computation system that happens to also do generics.

## Templates as Functions

A template instantiation is a compile-time function call. The "arguments" are types and values. The "return value" is a new type or value.

```cpp
// Compile-time factorial
template<int N>
struct Factorial
{
    static constexpr int value = N * Factorial<N - 1>::value;
};

template<>
struct Factorial<0>
{
    static constexpr int value = 1;
};

// Evaluated at compile time: Factorial<10>::value == 3628800
```

This is pattern matching with recursive evaluation — it's functional programming with angle brackets.

## SFINAE: Compile-Time Conditionals

Substitution Failure Is Not An Error. If a template parameter substitution produces invalid code, the compiler silently discards that overload instead of erroring:

```cpp
// Only enabled for types with a .size() method
template<typename T>
auto length(const T& container) -> decltype(container.size())
{
    return container.size();
}

// Fallback for everything else
template<typename T>
int length(const T&)
{
    return -1;
}
```

This was the pre-C++20 way to constrain templates. It works, but it's arcane.

## C++20 Concepts: Templates With Guardrails

Concepts replace SFINAE with readable constraints:

```cpp
template<typename T>
concept Container = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
};

template<Container C>
void process(const C& c)
{
    for (const auto& item : c)
        handle(item);
}
```

The error message when you pass a non-container is now "constraint not satisfied" instead of a 200-line template substitution failure.

## constexpr: Breaking the Compile-Time Barrier

`constexpr` functions run at compile time when called with constant arguments:

```cpp
constexpr auto fibonacci(int n) -> int
{
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Computed at compile time — zero runtime cost
constexpr int fib20 = fibonacci(20); // 6765
```

Combined with `if constexpr`, you get compile-time branching:

```cpp
template<typename T>
auto serialize(const T& value)
{
    if constexpr (std::is_arithmetic_v<T>)
        return std::to_string(value);
    else if constexpr (std::is_same_v<T, std::string>)
        return value;
    else
        return value.to_string();
}
```

Only the taken branch is compiled. Dead branches can contain invalid code for that type — they're discarded entirely.

## Why It Matters

This compile-time computation system enables:

- **Zero-cost abstractions**: `std::sort` with a lambda is as fast as hand-written C
- **Type-safe units**: `meters * seconds` → compile error if types don't match
- **Static polymorphism**: CRTP gives virtual-dispatch semantics without vtable overhead
- **Embedded DSLs**: Expression templates (Eigen, Boost.Spirit) build computation graphs at compile time

The cost is compile time. Heavy template use can make builds slow. But the runtime cost is zero — every template computation is resolved before the binary exists.

C++ templates aren't pretty. But they're the most powerful compile-time computation system in any mainstream language, and understanding them unlocks patterns that aren't possible anywhere else.
