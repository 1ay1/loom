---
title: "Iteration at Compile Time — Parameter Packs, Fold Expressions, and Recursive Templates"
date: 2026-02-11
slug: iteration-at-compile-time
tags: compile-time-cpp, parameter-packs, fold-expressions, recursive-templates, variadic
excerpt: The compile-time language doesn't have for loops. It has something weirder — parameter packs that expand into exactly the code you need, like a copy-paste machine controlled by the compiler.
---

At runtime, iterating is as natural as breathing. You have a vector. You write a `for` loop. You process each element. Done. You don't even think about it — it's the first thing they teach you after "hello world."

Now try to do that at compile time. Go ahead. I'll wait.

You can't. Because what would you even iterate *over*? There's no `std::vector` at compile time. The data you want to process isn't data at all — it's types, or values baked into template parameters. A variadic template might receive `int, double, std::string` as its parameter pack. That's not a container. It's three types known to the compiler during instantiation. You can't index into it. You can't write a `for` loop over it. You can't call `.begin()` on it.

So how does the compile-time language iterate?

It doesn't loop. It *expands*.

Think of it like a really smart copy-paste. You write a pattern once, and the compiler stamps it out for each element in your list. No counter incrementing. No condition checking. No loop body executing repeatedly. The compiler just... generates all the code, in one shot, as if you'd written each piece by hand.

## Parameter Packs: The Compile-Time List

A parameter pack is the compile-time language's version of a list. You declare one with `...`:

```cpp
template<typename... Ts>
struct TypeList {};

template<auto... Vs>
struct ValueList {};
```

`typename... Ts` declares a type parameter pack — zero or more types. `auto... Vs` declares a value parameter pack — zero or more values. When someone writes `TypeList<int, double, char>`, the pack `Ts` holds `int, double, char`.

You can ask how big it is:

```cpp
template<typename... Ts>
constexpr std::size_t count = sizeof...(Ts);

static_assert(count<int, double, char> == 3);
static_assert(count<> == 0);  // empty pack is fine
```

But that's about all you can do with a pack *directly*. You can't write `Ts[0]` to get the first type. You can't assign to it. You can't mutate it. The only real operation is **expansion** — and expansion is where all the magic happens.

## Pack Expansion: The Copy-Paste Machine

The `...` operator, when placed after an expression containing a pack, expands that expression once for each element. This is the entire mechanism. Everything else in this post builds on it.

The simplest example:

```cpp
template<typename... Args>
void forward_all(Args&&... args) {
    target(std::forward<Args>(args)...);
}
```

When called with three arguments, this expands into:

```cpp
target(std::forward<Arg0>(arg0), std::forward<Arg1>(arg1), std::forward<Arg2>(arg2));
```

The pattern `std::forward<Args>(args)` is stamped out for each element. The results are comma-separated. It's like the compiler has a copy-paste machine that duplicates the pattern once per element and fills in the blanks.

This works in several contexts, and each one is useful:

**Function arguments** — call a function with processed arguments:

```cpp
template<typename... Args>
void call(Args... args) {
    f(process(args)...);  // f(process(a1), process(a2), process(a3))
}
```

**Template arguments** — create types derived from each element:

```cpp
template<typename... Ts>
using PointerTuple = std::tuple<Ts*...>;
// PointerTuple<int, double, char> = std::tuple<int*, double*, char*>
```

Look at that. `Ts*...` expands to `int*, double*, char*`. You added a pointer to every type in one expression. No loop. No recursion. Just pattern expansion.

**Initializer lists** — the pre-C++17 hack for side effects:

```cpp
template<typename... Args>
void print_all(Args... args) {
    int dummy[] = { (std::cout << args << '\n', 0)... };
    (void)dummy;
}
```

This one deserves explanation because it's gloriously ugly. Each element expands to `(std::cout << arg << '\n', 0)` — the comma operator evaluates the print, discards the result, and yields `0` to fill the array. The array exists only to give the expansion somewhere to live. We immediately cast it to `void` to suppress "unused variable" warnings. It's a hack. It works. C++17 killed it with fold expressions (thank goodness).

**Base class lists** — inherit from multiple types at once:

```cpp
template<typename... Mixins>
struct Combined : Mixins... {
    using Mixins::operator()...;  // bring in all operator() from all bases
};
```

This is how `std::overloaded` works (or the commonly-written equivalent). You inherit from multiple lambda types and use pack expansion to bring all their `operator()` methods into scope. The compiler resolves which one to call based on the argument type. It's compile-time dispatch through inheritance and overloading, and it's remarkably elegant.

The mental model: pack expansion is not iteration. There's no loop in the generated code. There's no counter. There's no branch. The compiler stamps out the expanded form in one shot, as if you'd written it all by hand. The result is code that's exactly as efficient as the non-variadic version — because there's nothing to optimize away. There was never a loop.

## Recursive Templates: The Old-School For Loop

Before C++17 gave us fold expressions, the only way to "iterate" a parameter pack with any logic was recursion. Peel off the first element, process it, recurse on the rest. Base case to stop.

If you've ever written recursive functions in a functional language, this will feel natural. If you haven't — imagine a `for` loop, but instead of incrementing a counter, you call yourself with a shorter list.

**Counting types that satisfy a predicate:**

```cpp
// Base case: empty pack, nothing to count
template<template<typename> class Pred>
constexpr int count_if() {
    return 0;
}

// Recursive case: check the first type, add 0 or 1, recurse on the rest
template<template<typename> class Pred, typename T, typename... Rest>
constexpr int count_if() {
    return (Pred<T>::value ? 1 : 0) + count_if<Pred, Rest...>();
}
```

Each call peels one type off the front of the pack. When the pack is empty, the base case returns 0. The compiler generates a chain of instantiations — `count_if<Pred, int, double, char>` calls `count_if<Pred, double, char>` calls `count_if<Pred, char>` calls `count_if<Pred>` (base case). Each one is a distinct function instantiation, fully resolved at compile time. The optimizer typically inlines the entire chain into a single constant.

**Finding a type in a pack:**

```cpp
template<typename Target>
constexpr int find() {
    return -1;  // not found
}

template<typename Target, typename T, typename... Rest>
constexpr int find() {
    if constexpr (std::is_same_v<Target, T>) {
        return 0;  // found it!
    } else {
        constexpr int result = find<Target, Rest...>();
        return result == -1 ? -1 : 1 + result;
    }
}

static_assert(find<double, int, double, char>() == 1);  // double is at index 1
static_assert(find<float, int, double, char>() == -1);  // float not found
```

Compile-time `indexOf`. The compiler searches through a list of types and tells you where the target is. The search happens during compilation. The binary just contains the number.

The downside of recursive templates is compile-time cost. Each recursion level instantiates a new template. A pack of 100 types means 100 instantiations. Compilers have instantiation depth limits (typically 256 or 1024). For simple reductions, fold expressions eliminated this problem entirely.

## Fold Expressions: The Modern For Loop (C++17)

C++17 introduced fold expressions, and they're beautiful. They reduce a parameter pack with a binary operator in a single expression. No recursion. No helper functions. No dummy arrays. Just... the thing you wanted to say.

There are four forms, but don't panic — they all do the same basic thing (reduce a pack with an operator), just with different parenthesization:

```cpp
// Unary right fold: (pack op ...)
// Expands to: a1 op (a2 op (a3 op a4))
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);
}

// Binary left fold with initial value: (init op ... op pack)
// Expands to: ((init op a1) op a2) op a3
template<typename... Args>
auto sum_safe(Args... args) {
    return (0 + ... + args);  // works even if args is empty!
}
```

The binary forms are essential when the pack might be empty. A unary fold over an empty pack is an error for most operators. The binary form provides an identity element — `0` for addition, `true` for `&&`, `false` for `||`.

### The Comma Fold: Your New Best Friend

The most useful fold pattern in practice is the comma fold. It replaces the grotesque initializer-list hack entirely:

```cpp
template<typename... Args>
void print_all(const Args&... args) {
    ((std::cout << args << '\n'), ...);
}
```

This expands to:

```cpp
(std::cout << a1 << '\n'), (std::cout << a2 << '\n'), (std::cout << a3 << '\n');
```

Each sub-expression executes as a side effect. The comma operator sequences them left to right. Clean, readable, no dummy arrays.

You can put *anything* in a comma fold:

```cpp
template<typename... Ts>
auto type_names() {
    std::vector<std::string_view> names;
    ((names.push_back(typeid(Ts).name())), ...);
    return names;
}
```

Push a value into a container for each type in the pack. One line. No recursion. No helper functions.

## Index Sequences: When You Need the Position

Sometimes you need the index, not just the element. "Give me the 0th, 1st, 2nd, 3rd element" — not just "give me each element." This comes up constantly when working with `std::tuple`, because `std::get<I>` requires a compile-time index.

`std::index_sequence` is a type that holds a compile-time sequence of integers:

```cpp
// std::make_index_sequence<3> produces std::index_sequence<0, 1, 2>
// Think of it as: range(3) but at compile time
```

The standard pattern uses a helper function that deduces the indices as a pack:

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

`make_index_sequence<3>` produces `index_sequence<0, 1, 2>`. The helper deduces `Is` as the pack `0, 1, 2`. Now you can use `Is` in a fold expression to index into the tuple.

This is how `std::apply` works internally — it unpacks a tuple into function arguments:

```cpp
template<typename F, typename Tuple, std::size_t... Is>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<Is...>) {
    return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))...);
}
```

The expansion `std::get<Is>(t)...` produces `std::get<0>(t), std::get<1>(t), std::get<2>(t)` — each tuple element becomes a separate function argument. This is the bridge between compile-time type-level data (the tuple) and runtime function calls.

## When to Use What

**Use pack expansion** when your inputs are types or heterogeneous values. If you have a parameter pack of different types, you can't put them in a `constexpr` array. Pack expansion is the natural tool.

**Use `constexpr` loops** when your inputs are homogeneous values. If everything is an `int`, a `constexpr` function with a `for` loop is way clearer than template recursion.

**Use fold expressions** for reductions over packs. Summing a value pack, ANDing conditions, ORing checks — fold expressions handle these in one line.

**Avoid recursive templates** unless you need logic that can't be expressed as a fold or expansion. They're slower to compile and harder to read. They were necessary before C++17. They're rarely the best choice after it.

In practice, a lot of compile-time code mixes all three. You might use `make_index_sequence` to generate indices, expand them in a fold expression, and call `constexpr` functions inside the expansion. The tools compose beautifully.

## Practical Examples: Things You Can Actually Build

### Compile-time sum of values

```cpp
template<auto... Vs>
constexpr auto sum = (Vs + ... + 0);

static_assert(sum<1, 2, 3, 4, 5> == 15);
static_assert(sum<> == 0);  // empty fold with identity
```

Two lines. A compile-time sum. The result is embedded in the binary as a constant.

### for_each on a type list

```cpp
template<typename... Ts, typename F>
void for_each_type(F&& f) {
    (f.template operator()<Ts>(), ...);
}

// Print the size of each type:
for_each_type<int, double, std::string>([]<typename T>() {
    std::cout << typeid(T).name() << ": " << sizeof(T) << '\n';
});
```

This calls the lambda once for each type, with a different `T` each time. The fold expression handles the iteration. The lambda gets to use `T` as a template parameter. It's `for_each`, but over types instead of values.

### Transforming every element of a tuple

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

// Double every element:
auto doubled = transform_tuple([](auto x) { return x * 2; },
                                std::make_tuple(1, 2.5, 3));
// doubled is std::tuple<int, double, int>{2, 5.0, 6}
```

Map over a tuple. Each element can be a different type. The lambda is called with each one. The result is a new tuple with the transformed values. All type-safe, all resolved at compile time.

### Rendering variadic children in a tree

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

This pattern shows up constantly in compile-time DOM trees, expression templates, and ECS architectures. Each child can be a different type. The tuple stores them with zero overhead. `std::apply` unpacks them, and a fold expression iterates them. All resolved at compile time, with the compiler generating exactly the code you'd write by hand for each specific combination of children.

## The Mental Model

Runtime iteration is imperative: initialize a counter, check a condition, execute a body, increment, repeat. The compiler translates this to jumps and branches.

Compile-time iteration is *expansion*: you write a pattern once, and the compiler stamps it out for each element. There's no loop in the generated code. There's no branch. There's no counter. Just the expanded result, as if you'd written each call individually.

Fold expressions add *reduction* — collapsing a pack into a single value with an operator. Index sequences add *positional access* — letting you use integers to reach into tuples. Recursive templates add *arbitrary logic* — anything you can express as a base case and a recursive step.

Together, these give you full iteration capability at compile time. Not by looping, but by expanding, folding, and recursing. Different mechanics, same expressive power. And in the generated code? No loops at all. Just the exact operations you needed, stamped out precisely, with zero overhead.

That's the compile-time way. Don't repeat yourself — let the compiler do the repeating for you.
