---
title: "Iteration at Compile Time — Parameter Packs, Fold Expressions, and Recursive Templates"
date: 2026-02-11
slug: iteration-at-compile-time
tags: compile-time-cpp, parameter-packs, fold-expressions, recursive-templates, variadic
excerpt: The compile-time language doesn't have for loops. It has something better — parameter packs that expand into exactly the code you need.
---

At runtime, you iterate over collections. You have a vector of ints, you write a `for` loop, you process each element. The pattern is so fundamental it barely registers as a concept — it's just what you do.

At compile time, there are no runtime collections. There's no `std::vector` you can loop over, because the data you want to process isn't data at all — it's types, or values baked into template parameters. A variadic template might receive `int, double, std::string` as its parameter pack. That's not a container. It's a list of types known to the compiler at instantiation time. You can't index into it. You can't write a `for` loop over it.

So how do you iterate?

You expand.

## Parameter Packs

A parameter pack is the compile-time language's answer to a list. You declare one with `...`:

```cpp
template<typename... Ts>
struct TypeList {};

template<auto... Vs>
struct ValueList {};
```

`typename... Ts` declares a type parameter pack — zero or more types. `auto... Vs` declares a non-type parameter pack — zero or more values. When someone writes `TypeList<int, double, char>`, the pack `Ts` holds `int, double, char`.

You can query the size:

```cpp
template<typename... Ts>
constexpr std::size_t count = sizeof...(Ts);

static_assert(count<int, double, char> == 3);
static_assert(count<> == 0);
```

But that's about all you can do with a pack directly. You can't write `Ts[0]` to get the first type. You can't assign to it or mutate it. The only real operation is **expansion** — turning the pack into a comma-separated sequence wherever the language allows one.

## Pack Expansion

The `...` operator, when placed after an expression containing a pack, expands that expression once for each element. This is the core mechanism. Everything else builds on it.

The simplest expansion passes each element as a function argument:

```cpp
template<typename... Args>
void forward_all(Args&&... args) {
    target(std::forward<Args>(args)...);
}
```

When called with three arguments, the expansion produces `target(std::forward<Arg0>(arg0), std::forward<Arg1>(arg1), std::forward<Arg2>(arg2))`. The pattern `std::forward<Args>(args)` is applied to each element, and the results are comma-separated.

This works in several contexts:

**Function arguments:**

```cpp
template<typename... Args>
void call(Args... args) {
    f(process(args)...);  // f(process(a1), process(a2), process(a3))
}
```

**Template arguments:**

```cpp
template<typename... Ts>
using PointerTuple = std::tuple<Ts*...>;  // std::tuple<int*, double*, char*>
```

**Initializer lists:**

```cpp
template<typename... Args>
void print_all(Args... args) {
    int dummy[] = { (std::cout << args << '\n', 0)... };
    (void)dummy;
}
```

This initializer list trick was the pre-C++17 way to execute a side effect for each pack element. Each element expands into `(std::cout << arg << '\n', 0)` — the comma operator evaluates the print, discards the result, and yields `0` to initialize the array. Ugly, but it works.

**Base class lists:**

```cpp
template<typename... Mixins>
struct Combined : Mixins... {
    using Mixins::operator()...;  // C++17: using-declaration with pack expansion
};
```

Pack expansion is not iteration in the runtime sense. No counter increments. No loop body executes repeatedly. The compiler stamps out the expanded form in one shot, as if you'd written it all by hand. The result is code that is exactly as efficient as the non-variadic version — there's nothing to optimize away, because there was never a loop.

## Recursive Templates: The Old For Loop

Before C++17 gave us fold expressions, the only way to "iterate" a parameter pack with any logic was recursion. You'd peel off the first element, process it, and recurse on the rest. Base case to stop.

This is the compile-time language's equivalent of a `for` loop, and it looks like functional programming because it is functional programming — no mutation, just new values derived from old ones.

**Counting types that satisfy a predicate:**

```cpp
// Base case: empty pack
template<template<typename> class Pred>
constexpr int count_if() {
    return 0;
}

// Recursive case: peel off T, recurse on Rest
template<template<typename> class Pred, typename T, typename... Rest>
constexpr int count_if() {
    return (Pred<T>::value ? 1 : 0) + count_if<Pred, Rest...>();
}
```

Each instantiation peels one type off the pack. When the pack is empty, the base case returns 0. The compiler generates a chain of instantiations — `count_if<Pred, int, double, char>` calls `count_if<Pred, double, char>` calls `count_if<Pred, char>` calls `count_if<Pred>`. Each one is a distinct function, fully resolved at compile time.

**Finding a type in a pack:**

```cpp
template<typename Target>
constexpr int find() {
    return -1;  // not found
}

template<typename Target, typename T, typename... Rest>
constexpr int find() {
    if constexpr (std::is_same_v<Target, T>) {
        return 0;
    } else {
        constexpr int result = find<Target, Rest...>();
        return result == -1 ? -1 : 1 + result;
    }
}

static_assert(find<double, int, double, char>() == 1);
static_assert(find<float, int, double, char>() == -1);
```

**Transforming each type:**

```cpp
template<typename... Ts>
struct AddPointer {
    using type = std::tuple<Ts*...>;  // pack expansion handles this directly
};
```

That last one doesn't even need recursion — pack expansion in a type context handles it. But for more complex transforms (say, conditionally wrapping types), recursion was the only option before C++17.

The downside of recursive templates is compile-time cost. Each recursion level instantiates a new template. A pack of 100 types means 100 instantiations. Compilers have instantiation depth limits (typically 256 or 1024). For simple operations, fold expressions eliminated this entirely.

## Fold Expressions: The Modern For Loop

C++17 introduced fold expressions — a way to reduce a parameter pack with a binary operator in a single expression, no recursion needed.

There are four forms:

```cpp
// Unary right fold: (pack op ...)
// Expands to: a1 op (a2 op (a3 op a4))
template<typename... Args>
auto sum_r(Args... args) {
    return (args + ...);
}

// Unary left fold: (... op pack)
// Expands to: ((a1 op a2) op a3) op a4
template<typename... Args>
auto sum_l(Args... args) {
    return (... + args);
}

// Binary right fold: (pack op ... op init)
// Expands to: a1 op (a2 op (a3 op init))
template<typename... Args>
auto sum_ri(Args... args) {
    return (args + ... + 0);
}

// Binary left fold: (init op ... op pack)
// Expands to: ((init op a1) op a2) op a3
template<typename... Args>
auto sum_li(Args... args) {
    return (0 + ... + args);
}
```

The binary forms are essential when the pack might be empty. A unary fold over an empty pack is ill-formed for most operators (the exception: unary fold over `&&` gives `true`, `||` gives `false`, and `,` gives `void()`). The binary forms provide an identity element.

For associative operations like addition, the fold direction doesn't matter. For non-associative ones like subtraction, it absolutely does. `(args - ...)` with values `1, 2, 3` gives `1 - (2 - 3)` = `2`. `(... - args)` gives `(1 - 2) - 3` = `-4`.

### The Comma Fold Trick

The most useful fold pattern in practice is the comma fold. It replaces the ugly initializer-list hack entirely:

```cpp
template<typename... Args>
void print_all(const Args&... args) {
    ((std::cout << args << '\n'), ...);
}
```

This expands to `(std::cout << a1 << '\n'), (std::cout << a2 << '\n'), (std::cout << a3 << '\n')`. Each sub-expression executes as a side effect. The comma operator sequences them left to right.

You can do anything in there — call functions, accumulate into a local variable, push into a container:

```cpp
template<typename... Ts>
auto type_names() {
    std::vector<std::string_view> names;
    ((names.push_back(typeid(Ts).name())), ...);
    return names;
}
```

## Index Sequences

Sometimes you need the index, not just the element. Maybe you're iterating a `std::tuple`, and you need to call `std::get<I>` for each position. Packs don't carry indices, so you manufacture them.

`std::index_sequence` is a type that holds a compile-time sequence of integers:

```cpp
// std::index_sequence<0, 1, 2> holds the values 0, 1, 2
// std::make_index_sequence<3> produces std::index_sequence<0, 1, 2>
```

The standard pattern uses a helper function that takes the index sequence as a parameter, deducing the indices as a pack:

```cpp
template<typename Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << std::get<Is>(t) << '\n'), ...);
}

template<typename... Ts>
void print_tuple(const std::tuple<Ts...>& t) {
    print_tuple_impl(t, std::make_index_sequence<sizeof...(Ts)>{});
}
```

`make_index_sequence<3>` produces `index_sequence<0, 1, 2>`. The helper deduces `Is` as the pack `0, 1, 2`. Now you can use `Is` in a fold expression or pack expansion to index into the tuple.

This is how `std::apply` works internally — it unpacks a tuple into function arguments:

```cpp
template<typename F, typename Tuple, std::size_t... Is>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<Is...>) {
    return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))...);
}

template<typename F, typename Tuple>
decltype(auto) apply(F&& f, Tuple&& t) {
    constexpr auto size = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    return apply_impl(std::forward<F>(f), std::forward<Tuple>(t),
                      std::make_index_sequence<size>{});
}
```

The expansion `std::get<Is>(std::forward<Tuple>(t))...` produces `std::get<0>(t), std::get<1>(t), std::get<2>(t)` — each tuple element as a separate function argument. This is the bridge between compile-time type-level data (the tuple) and runtime function calls.

## constexpr Loops vs Pack Expansion

C++20 made `constexpr` functions powerful enough to use real loops over compile-time data:

```cpp
constexpr int sum(std::initializer_list<int> vals) {
    int total = 0;
    for (int v : vals)
        total += v;
    return total;
}

static_assert(sum({1, 2, 3, 4}) == 10);
```

So when do you use a `constexpr` loop versus pack expansion?

**Use pack expansion when your inputs are types or heterogeneous values.** If you have a parameter pack of different types, you can't shove them into a `constexpr` array. Pack expansion is the natural tool.

**Use `constexpr` loops when your inputs are homogeneous values.** If everything is an `int`, a `constexpr` function with a `for` loop is clearer and compiles faster than template recursion.

**Use fold expressions for reductions over packs.** Summing a value pack, ANDing a list of conditions, ORing a list of checks — fold expressions handle these in one line with no recursion overhead.

In practice, a lot of compile-time code mixes all three. You might use `make_index_sequence` to generate indices, expand them in a fold expression, and call `constexpr` functions inside the expansion. The tools compose.

## Practical Examples

### Compile-time sum of a value pack

```cpp
template<auto... Vs>
constexpr auto sum = (Vs + ... + 0);

static_assert(sum<1, 2, 3, 4, 5> == 15);
static_assert(sum<> == 0);
```

### for_each on a type list

```cpp
template<typename... Ts, typename F>
void for_each_type(F&& f) {
    (f.template operator()<Ts>(), ...);
}

// Usage:
for_each_type<int, double, std::string>([](auto) {
    // ...
});

// More practically, with a generic lambda:
for_each_type<int, double, std::string>([]<typename T>() {
    std::cout << typeid(T).name() << ": " << sizeof(T) << '\n';
});
```

### Transforming a tuple

```cpp
template<typename F, typename Tuple, std::size_t... Is>
auto transform_impl(F&& f, const Tuple& t, std::index_sequence<Is...>) {
    return std::make_tuple(f(std::get<Is>(t))...);
}

template<typename F, typename... Ts>
auto transform_tuple(F&& f, const std::tuple<Ts...>& t) {
    return transform_impl(std::forward<F>(f), t,
                          std::make_index_sequence<sizeof...(Ts)>{});
}

// Usage:
auto doubled = transform_tuple([](auto x) { return x * 2; },
                                std::make_tuple(1, 2.5, 3));
// doubled is std::tuple<int, double, int>{2, 5.0, 6}
```

### Processing variadic children

```cpp
template<typename... Children>
struct Node {
    std::tuple<Children...> children;

    void render() {
        std::apply([](auto&... kids) {
            (kids.render(), ...);
        }, children);
    }

    template<typename F>
    void for_each_child(F&& f) {
        std::apply([&f](auto&... kids) {
            (f(kids), ...);
        }, children);
    }
};
```

This pattern shows up constantly in compile-time DOM trees, expression templates, and ECS architectures. Each child can be a different type. The tuple stores them with zero overhead. `std::apply` unpacks them, and a fold expression iterates them — all resolved at compile time, with the compiler generating exactly the code you'd write by hand for each specific combination of children.

## The Mental Model

Runtime iteration is imperative: initialize a counter, check a condition, execute a body, increment, repeat. The compiler translates this to jumps and branches.

Compile-time iteration is expansion: you write a pattern once, and the compiler stamps it out for each element in the pack. There's no loop in the generated code. There's no branch. There's just the expanded result, as if you'd written each call individually.

Fold expressions add reduction — collapsing a pack into a single value with an operator. Index sequences add positional access — letting you use integers to reach into tuples and packs. Recursive templates add arbitrary logic — anything you can express as a base case and a recursive case.

Together, these give you full iteration capability at compile time. Not by looping, but by expanding, folding, and recurring. Different mechanics, same expressive power.

Next post: we look at compile-time branching beyond `if constexpr` — tag dispatch, SFINAE, and concepts as the compile-time language's pattern matching.
