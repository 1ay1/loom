---
title: "Metafunctions and Higher-Order Programming — Templates That Take Templates"
date: 2026-02-17
slug: metafunctions-and-higher-order-programming
tags: compile-time-cpp, metafunctions, template-template-parameters, higher-order
excerpt: Runtime C++ has std::function and lambdas. Compile-time C++ has metafunctions and template template parameters. Same idea, alien syntax. Welcome to functional programming with angle brackets.
---

We've built up a lot of compile-time language features by now. Variables (type aliases). Functions (templates). Branching (specialization, `if constexpr`, concepts). Loops (pack expansion, fold expressions). Data structures (type lists, value lists). That's already a complete programming language.

But a language without higher-order functions — functions that take other functions as arguments or return them — is a language you'll fight constantly. You can't write `map` without passing a function to apply. You can't write `filter` without passing a predicate. You can't build pipelines without composing transformations.

At runtime, C++ solved this with function pointers, then `std::function`, then lambdas. The compile-time language has its own solution, and it's wonderfully weird: metafunctions, metafunction classes, and template template parameters.

The concepts map directly to runtime equivalents. The syntax... does not. But by the end of this post, you'll be passing functions to functions at compile time like it's the most natural thing in the world.

## Metafunctions: The Calling Convention

A metafunction is a template that takes types (or values) as input and produces types (or values) as output. You've seen dozens of them already:

```cpp
template<typename T>
struct add_pointer {
    using type = T*;
};

template<typename T>
struct remove_reference {
    using type = T;
};

template<typename T>
struct remove_reference<T&> {
    using type = T;
};

template<typename T>
struct remove_reference<T&&> {
    using type = T;
};
```

The convention: input through template parameters, output through a nested `::type` (for types) or `::value` (for booleans/integers). This is the *calling convention* of the compile-time language. Nobody enforces it — it's a universal convention that the standard library, Boost, and every serious metaprogramming library follows.

A "function call" looks like this:

```cpp
typename add_pointer<int>::type      // int*
typename remove_reference<int&&>::type  // int
```

Think of it this way:
- `add_pointer` = the function name
- `<int>` = the argument
- `::type` = "call it and give me the result"

The `_t` aliases make it less painful:

```cpp
std::add_pointer_t<int>  // int*
std::remove_reference_t<int&&>  // int
```

This is all review. The new question is: how do you pass one of these functions *to another function*?

## Template Template Parameters: Passing Functions Around

C++ has direct syntax for this, and it looks exactly as wild as you'd expect:

```cpp
template<template<typename> class F, typename T>
using apply = typename F<T>::type;
```

Let's break this down. `F` isn't a type. It's a *template* — a function waiting for arguments. The declaration `template<typename> class F` says "F is a template that takes one type parameter." You can now pass any conforming template:

```cpp
apply<std::remove_const, const int>  // int
apply<add_pointer, double>           // double*
```

This is a higher-order function. `apply` takes a function (`F`) and an argument (`T`), and calls the function on the argument. It's the compile-time equivalent of:

```cpp
auto apply(auto f, auto x) { return f(x); }
```

Same concept. Wildly different syntax. But the *idea* — passing a function as an argument to another function — is identical.

The limitation: `template<typename> class F` only matches templates that take *exactly one type parameter*. A template taking two parameters, or a non-type parameter, won't match. This rigidity makes template template parameters awkward for truly generic higher-order programming.

## Metafunction Classes: First-Class Function Objects

The Boost.MPL library introduced a more flexible approach: wrap a metafunction inside a struct, turning it into a *type* you can pass around like any other value.

```cpp
struct add_pointer_f {
    template<typename T>
    struct apply {
        using type = T*;
    };
};
```

`add_pointer_f` is a type. Not a template — a plain type. You can pass it as a regular `typename` template argument. The function it represents lives *inside*, as a nested `apply` template. To call it:

```cpp
typename add_pointer_f::apply<int>::type  // int*
```

More verbose? Yes. But this buys you something critical: *uniformity*. Every metafunction class is just a type. You can store them in type lists. You can pass them through any template parameter. You don't need the special `template<typename> class` syntax. They're first-class values in the compile-time language.

Here's a generic `apply` that works with metafunction classes:

```cpp
template<typename F, typename T>
using apply = typename F::template apply<T>::type;
```

And now you can write higher-order functions that accept *any* metafunction class:

```cpp
struct remove_const_f {
    template<typename T>
    struct apply : std::remove_const<T> {};
};

struct add_const_f {
    template<typename T>
    struct apply { using type = const T; };
};

// Usage — same "apply" works with any metafunction class:
apply<add_pointer_f, int>              // int*
apply<remove_const_f, const double>    // double
apply<add_const_f, int>                // const int
```

The pattern: the metafunction class is a type that *carries* a function. The nested `apply` *is* the function. The separation — the function as a passable value vs. the function as something you call — is the same distinction runtime C++ makes between a `std::function` object and actually invoking it with `operator()`.

## Composing Metafunctions: The Pipe Dream

With metafunction classes, you can build `compose` — applying one function's output as another's input:

```cpp
template<typename F, typename G>
struct compose {
    template<typename T>
    struct apply {
        using type = typename F::template apply<
            typename G::template apply<T>::type
        >::type;
    };
};
```

`compose<F, G>` is itself a metafunction class. It applies `G` first, then `F` to the result. Just like mathematical function composition: `(f . g)(x) = f(g(x))`.

```cpp
using remove_const_then_add_pointer = compose<add_pointer_f, remove_const_f>;

// Step 1: remove_const_f removes const from "const int" -> "int"
// Step 2: add_pointer_f adds pointer to "int" -> "int*"
apply<remove_const_then_add_pointer, const int>  // int*
```

Two functions, composed into one. The compiler evaluates the whole chain at instantiation time. No runtime cost.

For deeply nested compositions, you can build a left-to-right pipeline:

```cpp
template<typename... Fs>
struct pipeline;

template<typename F>
struct pipeline<F> : F {};  // single function = just that function

template<typename F, typename... Rest>
struct pipeline<F, Rest...> {
    template<typename T>
    struct apply {
        using intermediate = typename F::template apply<T>::type;
        using type = typename pipeline<Rest...>::template apply<intermediate>::type;
    };
};

using my_pipeline = pipeline<remove_const_f, add_pointer_f>;
// Step 1: remove const
// Step 2: add pointer
// Reads left to right, like a Unix pipe
```

This is the compile-time equivalent of `value | remove_const | add_pointer`. Data flows left to right through a sequence of transformations. Each transformation is a first-class function value.

## Map, Filter, Fold: The Holy Trinity

With metafunction classes and type lists, you can implement the three operations that make functional programming *functional*.

### Transform (Map)

Apply a function to every element in a list:

```cpp
template<typename List, typename F>
struct transform;

template<typename... Ts, typename F>
struct transform<type_list<Ts...>, F> {
    using type = type_list<typename F::template apply<Ts>::type...>;
};

template<typename List, typename F>
using transform_t = typename transform<List, F>::type;
```

Pack expansion does the heavy lifting. `F::template apply<Ts>::type...` applies `F` to each element. No recursion needed.

```cpp
using input = type_list<int, const double, const char>;
using result = transform_t<input, remove_const_f>;
// type_list<int, double, char>
```

We just `map`ped `remove_const` over a list of types. Every const was stripped. In one expression. At compile time.

### Filter

Keep only the types that satisfy a predicate:

```cpp
struct is_integral_f {
    template<typename T>
    struct apply : std::is_integral<T> {};
};

// (filter implementation uses recursion — see previous post)

using input = type_list<int, double, char, float, long>;
using result = filter_t<input, is_integral_f>;
// type_list<int, char, long>
```

`double` and `float` didn't pass the predicate. Gone. The filtered list contains only integral types.

### Fold (Reduce)

Collapse a type list into a single type by repeatedly applying a binary function:

```cpp
template<typename List, typename Init, typename BinaryF>
struct fold;

template<typename Init, typename BinaryF>
struct fold<type_list<>, Init, BinaryF> {
    using type = Init;  // empty list: return accumulator
};

template<typename Head, typename... Tail, typename Init, typename BinaryF>
struct fold<type_list<Head, Tail...>, Init, BinaryF> {
    using type = typename fold<
        type_list<Tail...>,
        typename BinaryF::template apply<Init, Head>::type,
        BinaryF
    >::type;
};
```

Here's a concrete use — counting pointer types in a list:

```cpp
struct count_pointers_f {
    template<typename Acc, typename T>
    struct apply {
        using type = std::integral_constant<
            int,
            Acc::value + (std::is_pointer_v<T> ? 1 : 0)
        >;
    };
};

using input = type_list<int*, double, char*, void>;
using result = fold_t<input, std::integral_constant<int, 0>, count_pointers_f>;
static_assert(result::value == 2);  // two pointers found
```

We folded over a type list, accumulating a count. The binary function takes the accumulator and the current element, returning a new accumulator. Same pattern as `std::accumulate`, just over types instead of values.

## A Full Pipeline Example

Let's put it all together. Given a type list, we want to: remove references, add const, then wrap in `std::optional`. Three transformations, chained.

```cpp
struct remove_reference_f {
    template<typename T>
    struct apply : std::remove_reference<T> {};
};

struct add_const_f {
    template<typename T>
    struct apply { using type = const T; };
};

struct wrap_optional_f {
    template<typename T>
    struct apply { using type = std::optional<T>; };
};
```

Apply them step by step:

```cpp
using input = type_list<int&, double&&, const char&, float>;

using step1 = transform_t<input, remove_reference_f>;
// type_list<int, double, const char, float>

using step2 = transform_t<step1, add_const_f>;
// type_list<const int, const double, const char, const float>

using step3 = transform_t<step2, wrap_optional_f>;
// type_list<std::optional<const int>, std::optional<const double>,
//           std::optional<const char>, std::optional<const float>>
```

Or, using the pipeline:

```cpp
using my_pipeline = pipeline<remove_reference_f, add_const_f, wrap_optional_f>;
using result = transform_t<input, my_pipeline>;
```

One pass. Three transformations fused. The compiler applies all three to each type, producing the final list in one `transform` call. This is the compile-time equivalent of a Unix pipe or a Rust iterator chain.

## The Full Picture

Here's where we stand. The compile-time language now has:

| Feature | Mechanism |
|---------|-----------|
| Values | Types and integral constants |
| Variables | Type aliases (`using`) |
| Functions | Templates with `::type` / `::value` |
| Branching | Specialization, `if constexpr`, `std::conditional` |
| Recursion | Recursive template instantiation |
| Data structures | Type lists (variadic templates) |
| Pattern matching | Partial specialization |
| Higher-order functions | Metafunction classes, template template parameters |
| Composition | `compose`, `pipeline`, `transform`, `filter`, `fold` |

That's a full functional programming language. Seriously — it's Lisp with angle brackets. Immutable data. Recursive computation. Higher-order functions. Lists as the primary data structure. The syntax is hostile. The error messages are worse. But the computational model is clean.

Once you see template metaprogramming as functional programming in disguise, it stops being dark magic and becomes straightforward (if verbose) code. You already know how to map, filter, and fold. You already know how to compose functions. The compile-time language just uses angle brackets where normal languages use parentheses.

Everything from here is application. We've built the engine. Now let's see what it can drive.
