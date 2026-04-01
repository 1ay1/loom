---
title: "Data Structures at Compile Time — Type Lists, Value Lists, and Compile-Time Containers"
date: 2026-02-13
slug: data-structures-at-compile-time
tags: compile-time-cpp, type-lists, tuple, constexpr-containers, metaprogramming
excerpt: The compile-time language has arrays, maps, and linked lists. They just look nothing like their runtime counterparts — and they hold types instead of values.
---

Every programming language needs data structures. You need to group things together, look things up, iterate, filter, transform. The compile-time language is no different. It has lists, maps, and containers — they just look nothing like `std::vector` or `std::unordered_map`.

Why? Because the compile-time language's "values" are *types*. You're not storing integers in a list. You're storing `int`, `double`, `std::string` — the types themselves. And a container of types is, necessarily, a very strange-looking container.

But once you get past the unfamiliar syntax, the operations are exactly what you'd expect. Head, tail, append, concat, transform, filter — the same list operations from any introductory data structures course. Just applied to types instead of numbers.

## Type Lists: The Fundamental Container

The most important compile-time data structure is absurdly simple:

```cpp
template<typename... Ts>
struct type_list {};
```

That's the whole thing. A type that holds other types in its template parameter pack. No member functions. No data. No constructors. No destructors. It's a completely empty struct that carries information purely through its template parameters.

```cpp
using my_types = type_list<int, double, std::string>;
using empty = type_list<>;
using pointers = type_list<int*, char*, void*>;
```

If you're thinking "that's just an empty struct, what's the point?" — the point is that `type_list<int, double>` and `type_list<char, float>` are *different types*. The template parameters are part of the type's identity. The compile-time language can see those parameters, match on them, take them apart, and recombine them. The struct being empty doesn't mean it's useless. It's a *carrier* — it carries type information through the type system.

The type list itself does nothing. All the power comes from operations you define on it.

### Head and Tail: Peeking and Peeling

The most basic operations: get the first element, and get everything else.

```cpp
template<typename List>
struct head;

template<typename T, typename... Rest>
struct head<type_list<T, Rest...>> {
    using type = T;
};

template<typename List>
using head_t = typename head<List>::type;
```

This uses the pattern matching from the [previous post](/post/pattern-matching-at-compile-time). The specialization destructures `type_list<T, Rest...>`, binding the first element to `T` and the rest to `Rest...`. It's the compile-time equivalent of `list[0]` or Haskell's `head`.

Tail follows the same pattern:

```cpp
template<typename List>
struct tail;

template<typename T, typename... Rest>
struct tail<type_list<T, Rest...>> {
    using type = type_list<Rest...>;
};

template<typename List>
using tail_t = typename tail<List>::type;
```

Now:

```cpp
static_assert(std::is_same_v<head_t<type_list<int, double, char>>, int>);
static_assert(std::is_same_v<tail_t<type_list<int, double, char>>, type_list<double, char>>);
```

We just implemented list operations. On types. At compile time. The compiler computed `head` and `tail` during compilation, and by the time the binary exists, there's no trace of any of it — these operations don't generate any code. They're pure compile-time computation.

### Size, Concat, and the Satisfying Feeling of It All Working

Size is free — `sizeof...` gives you the pack length:

```cpp
template<typename List>
struct size;

template<typename... Ts>
struct size<type_list<Ts...>> {
    static constexpr std::size_t value = sizeof...(Ts);
};
```

Concatenation merges two type lists into one:

```cpp
template<typename L1, typename L2>
struct concat;

template<typename... Ts, typename... Us>
struct concat<type_list<Ts...>, type_list<Us...>> {
    using type = type_list<Ts..., Us...>;
};

template<typename L1, typename L2>
using concat_t = typename concat<L1, L2>::type;
```

Both parameter packs get expanded into a single `type_list`:

```cpp
using AB = type_list<int, double>;
using CD = type_list<char, float>;
using ABCD = concat_t<AB, CD>; // type_list<int, double, char, float>
```

We just concatenated two lists. At compile time. The compiler merged the type lists during instantiation, and the result is a new type. No runtime cost. No memory allocation. Just types flowing through the type system.

### Transform: Map for Types

Here's where things get powerful. Given a type list and a "function" (a template that takes one type and returns one type), apply the function to every element:

```cpp
template<typename List, template<typename> class F>
struct transform;

template<typename... Ts, template<typename> class F>
struct transform<type_list<Ts...>, F> {
    using type = type_list<typename F<Ts>::type...>;
};

template<typename List, template<typename> class F>
using transform_t = typename transform<List, F>::type;
```

The key is `type_list<typename F<Ts>::type...>`. The `...` expands the pack, applying `F` to each element. No recursion needed — pack expansion handles the iteration.

```cpp
using types = type_list<int, double, char>;
using pointers = transform_t<types, std::add_pointer>;
// type_list<int*, double*, char*>
```

We just mapped `add_pointer` over a list of types. Every element got a pointer added. The compile-time equivalent of `list.map(x => x + "*")`, but for types.

### Filter: Keeping Only What You Want

Filter is more interesting because the output might be shorter than the input. You need conditional inclusion, which means recursion:

```cpp
template<typename List, template<typename> class Pred>
struct filter;

// Base case: empty list
template<template<typename> class Pred>
struct filter<type_list<>, Pred> {
    using type = type_list<>;
};

// Recursive case: check head, recurse on tail
template<typename T, typename... Rest, template<typename> class Pred>
struct filter<type_list<T, Rest...>, Pred> {
    using rest_filtered = typename filter<type_list<Rest...>, Pred>::type;

    using type = std::conditional_t<
        Pred<T>::value,
        typename concat<type_list<T>, rest_filtered>::type,
        rest_filtered
    >;
};

template<typename List, template<typename> class Pred>
using filter_t = typename filter<List, Pred>::type;
```

The compiler peels off the head, recursively filters the tail, and either keeps or drops the head based on the predicate. Each recursive step is a new template instantiation. The compiler does all the work.

```cpp
template<typename T>
struct is_not_void {
    static constexpr bool value = !std::is_void_v<T>;
};

using cleaned = filter_t<type_list<int, void, double, void>, is_not_void>;
// type_list<int, double>
```

We just filtered `void` out of a type list. At compile time. The `void` types are gone — they were never included in the output list. No runtime condition. No branch. The filtered list was computed during compilation.

## Value Lists: When You Need Numbers, Not Types

Sometimes you need a compile-time list of *values*, not types:

```cpp
template<auto... Vs>
struct value_list {};

using primes = value_list<2, 3, 5, 7, 11>;
using flags = value_list<true, false, true>;
```

All the same operations apply. The standard library has its own value list: `std::integer_sequence`, with the crucial helper:

```cpp
using indices = std::make_index_sequence<5>; // index_sequence<0, 1, 2, 3, 4>
```

This is the compile-time version of Python's `range(5)`. You'll use it *constantly* for iterating tuples and packs by position.

## std::tuple: Where Compile Time Meets Runtime

Type lists hold types but no runtime data. If you want to actually *store values* where each element has a different type, you need `std::tuple`:

```cpp
std::tuple<int, double, std::string> t{42, 3.14, "hello"};

auto x = std::get<0>(t);  // 42 (int)
auto y = std::get<1>(t);  // 3.14 (double)
auto z = std::get<2>(t);  // "hello" (std::string)
```

The index in `std::get<I>` must be a compile-time constant. There's no `std::get<i>(t)` where `i` is a runtime variable — because each index returns a different *type*. `get<0>` returns `int`, `get<1>` returns `double`. The compiler needs to know which one at compile time to determine the return type.

This is the bridge between the compile-time world and the runtime world. The type list says "these are the types." The tuple says "here are actual values of those types." The compile-time language handles the type-level structure. The runtime language handles the values.

### Iterating Over a Tuple

There's no `for` loop over a tuple (each element has a different type!). Instead, use `std::index_sequence` to generate indices and a fold expression to process each one:

```cpp
template<typename Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << (Is == 0 ? "" : ", ") << std::get<Is>(t)), ...);
}

template<typename... Ts>
void print_tuple(const std::tuple<Ts...>& t) {
    print_tuple_impl(t, std::index_sequence_for<Ts...>{});
}
```

The index sequence generates `0, 1, 2, ...`. The fold expression expands the pack, calling `std::get` with each index. The comma formatting handles the separators. All type-safe, all resolved at compile time.

## Compile-Time Maps: Key-Value at Zero Cost

Sometimes you need key-value lookups resolved entirely during compilation. There are two approaches depending on your keys and values.

**Type-to-type map** (using template specialization):

```cpp
template<typename Key>
struct type_name;

template<> struct type_name<int>    { static constexpr auto value = "int"; };
template<> struct type_name<double> { static constexpr auto value = "double"; };
template<> struct type_name<char>   { static constexpr auto value = "char"; };

static_assert(type_name<int>::value == std::string_view("int"));
```

Each specialization is a key-value entry. Missing keys are a compile error (no primary template to fall back to). An incomplete map is a type error. That's a *feature* — it means the compiler catches missing entries.

**Value-to-value map** (using constexpr functions):

```cpp
constexpr int http_status(std::string_view method) {
    constexpr std::pair<std::string_view, int> table[] = {
        {"OK", 200},
        {"Not Found", 404},
        {"Internal Server Error", 500},
    };

    for (auto& [key, val] : table) {
        if (key == method) return val;
    }

    return -1;
}

static_assert(http_status("OK") == 200);
static_assert(http_status("Not Found") == 404);
```

This is just a constexpr function doing a linear scan over an array. The compiler evaluates it at compile time when used in a constant expression. No template trickery — just code running during compilation. For larger maps, you could sort the array at compile time and binary search, but for most compile-time lookups, linear search is fine. The "cost" is compiler time, not runtime.

## constexpr Containers: Real Data Structures at Compile Time

C++20 made `constexpr` evaluation dramatically more powerful. You can now use `std::string` and `std::vector` at compile time — real heap-allocating containers, evaluated entirely inside the compiler.

```cpp
constexpr auto build_squares() {
    std::array<int, 10> result{};
    for (int i = 0; i < 10; ++i) {
        result[i] = i * i;
    }
    return result;
}

constexpr auto squares = build_squares();
static_assert(squares[4] == 16);
static_assert(squares[9] == 81);
```

Imperative code — loops, mutation, indexing — running at compile time. The compiler executes the function and bakes the result into the binary. You get the readability of procedural code with the performance of a hand-written constant table.

`std::vector` at compile time is even wilder:

```cpp
constexpr int sum_of_filtered() {
    std::vector<int> v;
    for (int i = 0; i < 100; ++i) {
        if (i % 3 == 0 || i % 5 == 0) {
            v.push_back(i);
        }
    }

    int total = 0;
    for (int x : v) total += x;
    return total;
}

static_assert(sum_of_filtered() == 2318);
```

The compiler created a vector. Pushed elements. Looped over them. Summed them. Destroyed the vector. All during compilation. The vector never existed at runtime. It was born, lived, and died inside the compiler's head. Only the answer — 2318 — survived into the binary.

The rule: **transient allocation**. Memory allocated during constexpr evaluation must be freed before the evaluation ends. You can't smuggle a constexpr vector into runtime. But you can compute with one and return the results as a constant.

## When to Use Which

**Type lists** — for computing with types. Building variant types, filtering overload sets, generating class hierarchies. The data exists only in the type system.

**Value lists / integer sequences** — for compile-time index generation. Use `std::index_sequence` whenever you need to iterate a tuple or pack by position.

**std::tuple** — for heterogeneous runtime data. The bridge between compile-time type information and runtime values.

**constexpr containers** — for imperative value computation. When your algorithm is naturally loops and mutation over homogeneous data, just write normal code with `constexpr`.

The choice is usually obvious from the problem. Types? Use type lists. Values? Use constexpr. Both? Use tuples. The compile-time language has a data structure for every occasion. They just look a bit... different.
