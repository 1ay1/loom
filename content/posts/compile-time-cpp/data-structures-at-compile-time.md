---
title: "Data Structures at Compile Time — Type Lists, Value Lists, and Compile-Time Containers"
date: 2026-02-13
slug: data-structures-at-compile-time
tags: compile-time-cpp, type-lists, tuple, constexpr-containers, metaprogramming
excerpt: The compile-time language has arrays, maps, and linked lists. They just look nothing like their runtime counterparts.
---

Every programming language needs data structures. You need to group things together, look things up, iterate, filter, transform. The compile-time language inside C++ is no different. It has lists, maps, and containers — they just look nothing like `std::vector` or `std::unordered_map`.

The previous posts showed how types are values, how templates are functions, and how specialization is pattern matching. Now we build on all of that. A compile-time data structure is a template that holds multiple types or values and exposes operations through further template machinery. Once you see the pattern, you'll recognize it everywhere: in `std::tuple`, in `std::variant`, in every serious metaprogramming library.

## Type Lists: The Fundamental Container

The most important compile-time data structure is the type list. It's a variadic template that holds an ordered sequence of types:

```cpp
template<typename... Ts>
struct type_list {};
```

That's it. No member functions, no data, no constructors. It's a type that carries other types in its template parameter pack. You use it like this:

```cpp
using my_types = type_list<int, double, std::string>;
using empty = type_list<>;
using pointers = type_list<int*, char*, void*>;
```

The type list itself does nothing. All the power comes from operations you define on it — and those operations are just templates that pattern-match on the list's structure.

### Head and Tail

The most basic operations: get the first element, and get everything else.

```cpp
// head: extract the first type
template<typename List>
struct head;

template<typename T, typename... Rest>
struct head<type_list<T, Rest...>> {
    using type = T;
};

template<typename List>
using head_t = typename head<List>::type;
```

The partial specialization destructures `type_list<T, Rest...>`, binding the first element to `T` and the remainder to the pack `Rest...`. This is exactly how you'd pattern-match a cons cell in Haskell or ML.

Tail follows the same pattern:

```cpp
// tail: everything except the first type
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

### Size, Append, Concat

Size is the simplest recursive operation — except you don't even need recursion:

```cpp
template<typename List>
struct size;

template<typename... Ts>
struct size<type_list<Ts...>> {
    static constexpr std::size_t value = sizeof...(Ts);
};
```

`sizeof...(Ts)` gives the number of elements in a parameter pack. Free lunch from the compiler.

Concatenation merges two type lists:

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

Both parameter packs get expanded into a single `type_list`. Clean and direct:

```cpp
using AB = type_list<int, double>;
using CD = type_list<char, float>;
using ABCD = concat_t<AB, CD>; // type_list<int, double, char, float>
```

### Transform: Applying a Metafunction to Every Element

This is the compile-time `map`. Given a type list and a metafunction (a template that takes one type and produces one type), apply it to every element:

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

The key line is `type_list<typename F<Ts>::type...>`. The `...` expands the pack, applying `F` to each element. No recursion needed — pack expansion does the iteration.

```cpp
using types = type_list<int, double, char>;
using pointers = transform_t<types, std::add_pointer>;
// type_list<int*, double*, char*>
```

### Filter: Keeping Types That Match a Predicate

Filter is more interesting because the output list might be shorter than the input. You need conditional inclusion, which means you need recursion (or a fold, but let's keep it clear):

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

The compiler peels off the head, filters the tail recursively, and either prepends the head (if the predicate holds) or drops it. Each recursive instantiation produces a new template specialization. The compiler does all the work.

```cpp
template<typename T>
struct is_not_void {
    static constexpr bool value = !std::is_void_v<T>;
};

using cleaned = filter_t<type_list<int, void, double, void>, is_not_void>;
// type_list<int, double>
```

This is the essential pattern behind every type-level algorithm. Destructure the list, handle the base case, recurse. If you've written recursive functions in any language, you already know how to write compile-time algorithms on type lists.

## Value Lists

Sometimes you need a compile-time list of values, not types. The pattern is identical:

```cpp
template<auto... Vs>
struct value_list {};

using primes = value_list<2, 3, 5, 7, 11>;
using flags = value_list<true, false, true>;
```

The `auto` non-type template parameter (C++17) lets you mix value types, though in practice you usually keep them uniform. All the same operations apply — head, tail, concat, transform, filter — just operating on values instead of types.

The standard library has its own value list: `std::integer_sequence`. It's more restrictive (all values must have the same type), but it comes with a crucial helper:

```cpp
// std::make_index_sequence<N> generates index_sequence<0, 1, 2, ..., N-1>
using indices = std::make_index_sequence<5>; // index_sequence<0, 1, 2, 3, 4>
```

You'll use `make_index_sequence` constantly. It's the compile-time equivalent of `range(n)` — the starting point for iterating over tuples, arrays, or anything indexed.

## std::tuple: The Heterogeneous Runtime Container

Type lists hold types but no runtime data. `std::tuple` is the bridge: each element has a distinct type (known at compile time) but stores an actual runtime value.

```cpp
std::tuple<int, double, std::string> t{42, 3.14, "hello"};
```

Access is by compile-time index:

```cpp
auto x = std::get<0>(t);  // 42 (int)
auto y = std::get<1>(t);  // 3.14 (double)
auto z = std::get<2>(t);  // "hello" (std::string)
```

The index must be a constant expression. There is no `std::get<i>(t)` where `i` is a runtime variable — the compiler needs to know the return type, and each index yields a different type. This is the defining characteristic of heterogeneous containers: compile-time indexing, not runtime.

You can query a tuple's structure at compile time:

```cpp
using T = std::tuple<int, double, std::string>;
constexpr auto n = std::tuple_size_v<T>;              // 3
using second = std::tuple_element_t<1, T>;             // double
```

### Iterating Over a Tuple

There's no `for` loop over a tuple because each element has a different type. Instead, you use `std::index_sequence` to generate the indices and a fold or function call to process each one:

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

`std::index_sequence_for<Ts...>` generates `index_sequence<0, 1, ..., sizeof...(Ts)-1>`. The fold expression `((expr), ...)` expands the pack, executing the expression for each index. This is the standard idiom for doing anything with every element of a tuple.

## Compile-Time Maps

Sometimes you need key-value lookups resolved entirely at compile time. There are several approaches depending on whether your keys and values are types or values.

### Type-to-Type Map

Use template specialization as key-value pairs:

```cpp
template<typename Key>
struct type_name;

template<> struct type_name<int>    { static constexpr auto value = "int"; };
template<> struct type_name<double> { static constexpr auto value = "double"; };
template<> struct type_name<char>   { static constexpr auto value = "char"; };

static_assert(type_name<int>::value == std::string_view("int"));
```

Each specialization is an entry in the map. If you try to look up a key that doesn't exist and there's no primary template definition, you get a compile error — which is often exactly what you want. An incomplete map is a type error.

### Value-to-Value Map with constexpr

For runtime-style key-value lookups evaluated at compile time, use `constexpr` functions over arrays:

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

    return -1; // not found
}

static_assert(http_status("OK") == 200);
static_assert(http_status("Not Found") == 404);
```

This is just a constexpr function doing a linear scan over an array. The compiler evaluates it fully at compile time when called in a constant expression context. No template trickery at all — just C++ code running during compilation.

For larger maps, you can sort the array at compile time and use binary search, but for most compile-time lookup tables, linear search over a small array is perfectly fine. The "cost" is compiler time, not runtime.

## constexpr Containers: Real Data Structures at Compile Time

C++20 made `constexpr` evaluation dramatically more powerful. `std::array` was already usable in constexpr contexts, but C++20 added constexpr `std::string` and constexpr `std::vector` — real heap-allocating containers evaluated entirely at compile time.

### constexpr std::array

Build a lookup table at compile time:

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

This is imperative code — loops, mutation, indexing — but it runs at compile time. The compiler executes the function and bakes the result into the binary as a constant. You write normal C++, and the compiler acts as an interpreter.

### constexpr std::vector (C++20)

The striking addition. `std::vector` allocates heap memory, which seems incompatible with compile-time evaluation. The rule in C++20 is: **transient allocation** is allowed. Memory allocated during constexpr evaluation must be freed before the evaluation ends. The compiler tracks the allocations, and if anything leaks, it's a compile error.

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

The vector is created, elements are pushed onto it, a sum is computed, and then the vector is destroyed — all at compile time. The compiler's constexpr evaluator acts as a full interpreter with a memory allocator. As long as every allocation is matched by a deallocation within the same evaluation, it works.

You cannot, however, smuggle a constexpr vector out into runtime. This won't compile:

```cpp
constexpr std::vector<int> v = {1, 2, 3}; // Error: allocation is not transient
```

The allocation from `{1, 2, 3}` would need to persist into the runtime program, but constexpr allocations must be freed during evaluation. If you need the result at runtime, copy the data into a `std::array` or some other fixed-size container before the evaluation ends.

### Building Real Tables

Combining constexpr containers with constexpr algorithms lets you build sophisticated lookup tables:

```cpp
constexpr auto build_ascii_table() {
    std::array<bool, 128> is_alpha{};
    for (char c = 'A'; c <= 'Z'; ++c) is_alpha[c] = true;
    for (char c = 'a'; c <= 'z'; ++c) is_alpha[c] = true;
    return is_alpha;
}

constexpr auto ascii_alpha = build_ascii_table();
static_assert(ascii_alpha['A']);
static_assert(ascii_alpha['z']);
static_assert(!ascii_alpha['5']);
```

The table is computed once by the compiler and embedded directly into the binary. Zero runtime cost. You get the clarity of procedural code with the performance of a hand-written constant table.

## When to Use Which

The compile-time language now has several container abstractions, and they serve different purposes:

**Type lists** are for computing with types. Use them when your algorithm takes types as input and produces types as output — building variant types, filtering overload sets, generating class hierarchies. The data only exists in the compiler's type system and leaves no trace in the binary.

**Value lists and integer sequences** are for computing with compile-time values, especially indices. Use `std::index_sequence` whenever you need to iterate over a parameter pack or tuple by position. Use custom value lists when you need compile-time sequences of non-index values.

**std::tuple** is for when you need runtime data with heterogeneous types. It's the bridge between the compile-time world (where each element's type is known and distinct) and the runtime world (where actual values are stored and used). If you're building a compile-time type list and eventually need to hold actual objects of those types, tuple is the answer.

**constexpr containers** are for imperative value computation at compile time. When your algorithm is naturally expressed with loops, mutation, and dynamic collections, write it as a normal constexpr function using `std::array` or `std::vector`. This is the most readable option for any computation that doesn't inherently require type manipulation.

The choice is usually clear from the problem. If you're manipulating types, you need type lists. If you're computing values, prefer constexpr functions and containers — they're just normal code. If you're crossing the boundary between compile-time type information and runtime data, reach for tuple.

## The Standard Library Already Uses All of This

These aren't theoretical constructs. `std::tuple` is a type list with storage. `std::variant` is a type list with a runtime discriminant. `std::index_sequence` is the standard value list. The `<type_traits>` header is a library of operations on individual types that compose with type lists. Every template-heavy library you've ever used — Boost.Hana, Boost.Mp11, Brigand — is built on exactly the patterns shown here, scaled up with more operations and better ergonomics.

The compile-time language has data structures. They're just templates. And now, with constexpr containers, they can also be... normal containers. The gap between the two languages inside C++ gets narrower with every standard.
