---
title: "Metafunctions and Higher-Order Programming — Templates That Take Templates"
date: 2026-02-17
slug: metafunctions-and-higher-order-programming
tags: compile-time-cpp, metafunctions, template-template-parameters, higher-order
excerpt: Runtime C++ has std::function and lambdas. Compile-time C++ has metafunctions and template template parameters. Same idea, alien syntax.
---

Previous posts established that compile-time C++ has variables (type aliases), functions (templates), branching (specialization), recursion, and data structures (type lists). That's already a complete programming language. But a language without higher-order functions — functions that take functions as arguments or return them — is a language you'll fight constantly. You can't write `map` without passing a function. You can't write `filter` without passing a predicate. You can't build pipelines without composing transformations.

Runtime C++ solved this with function pointers, then `std::function`, then lambdas. Compile-time C++ has its own solution: metafunctions, metafunction classes, and template template parameters. The concepts map directly. The syntax doesn't.

## Metafunctions: The Calling Convention

A metafunction is a template that takes types (or values) as input and produces types (or values) as output. You've already seen dozens of them:

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

The convention: input arrives via template parameters, output leaves via a nested `::type` (for type results) or `::value` (for value results). This is the calling convention of the compile-time language. It isn't enforced by the compiler — it's a universal convention that the standard library, Boost, and every serious metaprogramming library follows.

A function call looks like this:

```cpp
typename add_pointer<int>::type      // int*
typename remove_reference<int&&>::type  // int
```

The `_t` aliases in C++14 and later are syntactic sugar over this convention:

```cpp
template<typename T>
using add_pointer_t = typename add_pointer<T>::type;

add_pointer_t<int>  // int*, no ::type needed
```

Value-returning metafunctions follow the same pattern with `::value` and the `_v` shorthand:

```cpp
template<typename T>
struct is_pointer : std::false_type {};

template<typename T>
struct is_pointer<T*> : std::true_type {};

is_pointer<int*>::value  // true
std::is_pointer_v<int*>  // same, via _v alias
```

This is all review. The new question is: how do you pass one of these metafunctions to another metafunction?

## Template Template Parameters: The Built-in Mechanism

C++ has direct syntax for this. A template can accept another template as a parameter:

```cpp
template<template<typename> class F, typename T>
using apply = typename F<T>::type;
```

`F` isn't a type. It's a template — a function waiting for arguments. The `template<typename> class F` declaration says "F is a template that takes one type parameter." You can now pass any conforming template:

```cpp
apply<std::remove_const, const int>  // int
apply<add_pointer, double>           // double*
```

This is a higher-order function. `apply` takes a function (`F`) and an argument (`T`), and calls the function on the argument. The compile-time equivalent of `auto apply(auto f, auto x) { return f(x); }`.

Template template parameters work, but they're rigid. `template<typename> class F` matches only templates that take exactly one type parameter. A template taking two parameters, or a non-type parameter, won't match. This rigidity makes them awkward for generic higher-order programming where you want to accept any metafunction regardless of its parameter signature.

## Metafunction Classes: First-Class Function Values

The Boost.MPL library introduced a more flexible approach: wrap a metafunction inside a struct, making it a type you can pass around like any other value.

```cpp
struct add_pointer_f {
    template<typename T>
    struct apply {
        using type = T*;
    };
};
```

`add_pointer_f` is a type. Not a template — a plain type. You can pass it as a regular template argument to anything that expects `typename F`. The function it represents lives inside, as a nested `apply` template. To call it:

```cpp
typename add_pointer_f::apply<int>::type  // int*
```

This is an extra layer of indirection, but it buys you something critical: uniformity. Every metafunction class is just a type. You can store them in type lists. You can pass them through any template parameter. You don't need the special `template<typename> class` syntax.

Here's a generic `apply` that works with metafunction classes:

```cpp
template<typename F, typename T>
using apply = typename F::template apply<T>::type;
```

And now you can write higher-order metafunctions that accept metafunction classes:

```cpp
struct remove_const_f {
    template<typename T>
    struct apply : std::remove_const<T> {};
};

struct add_const_f {
    template<typename T>
    struct apply { using type = const T; };
};

// Usage:
apply<add_pointer_f, int>    // int*
apply<remove_const_f, const double>  // double
```

The pattern: the metafunction class is a type that carries a function. The nested `apply` is the function itself. This separation — the function as a passable value vs. the function as something you call — is the same distinction runtime C++ makes between a `std::function` object and actually calling it with `operator()`.

## Composing Metafunctions

With metafunction classes, you can build `compose` — the compile-time equivalent of piping one function's output into another:

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

// remove_const_f::apply<const int>::type → int
// add_pointer_f::apply<int>::type → int*
apply<remove_const_then_add_pointer, const int>  // int*
```

You can chain as many as you want by nesting `compose`:

```cpp
using pipeline = compose<add_const_f, compose<add_pointer_f, remove_const_f>>;
// const int → int → int* → const int* ... wait, that adds const to the pointer
// Actually: apply remove_const (int), then add_pointer (int*), then add_const (int* const)
```

The readability degrades fast with deep nesting, but the mechanism is sound. We'll build a cleaner pipeline syntax later.

## Compile-Time Map, Filter, Fold

With metafunction classes and type lists, you can implement the three foundational higher-order operations.

### Transform (Map)

Apply a metafunction to every type in a list:

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

Pack expansion does the heavy lifting. `F::template apply<Ts>::type...` expands to `F` applied to each element. No recursion needed.

```cpp
using input = type_list<int, const double, const char>;
using result = transform_t<input, remove_const_f>;
// type_list<int, double, char>
```

### Filter

Keep only the types that satisfy a predicate. A predicate is a metafunction class whose `apply` yields a `::value` of `true` or `false`:

```cpp
struct is_integral_f {
    template<typename T>
    struct apply : std::is_integral<T> {};
};
```

Filtering requires conditional inclusion, which means recursion or fold. Here's a recursive implementation:

```cpp
template<typename List, typename Pred>
struct filter;

template<typename Pred>
struct filter<type_list<>, Pred> {
    using type = type_list<>;
};

template<typename Head, typename... Tail, typename Pred>
struct filter<type_list<Head, Tail...>, Pred> {
private:
    using rest = typename filter<type_list<Tail...>, Pred>::type;
    static constexpr bool keep = Pred::template apply<Head>::value;
public:
    using type = std::conditional_t<
        keep,
        typename prepend<Head, rest>::type,
        rest
    >;
};

template<typename List, typename Pred>
using filter_t = typename filter<List, Pred>::type;
```

Where `prepend` adds an element to the front of a type list:

```cpp
template<typename T, typename List>
struct prepend;

template<typename T, typename... Ts>
struct prepend<T, type_list<Ts...>> {
    using type = type_list<T, Ts...>;
};
```

```cpp
using input = type_list<int, double, char, float, long>;
using result = filter_t<input, is_integral_f>;
// type_list<int, char, long>
```

### Fold (Reduce)

Collapse a type list into a single type by repeatedly applying a binary metafunction:

```cpp
template<typename List, typename Init, typename BinaryF>
struct fold;

template<typename Init, typename BinaryF>
struct fold<type_list<>, Init, BinaryF> {
    using type = Init;
};

template<typename Head, typename... Tail, typename Init, typename BinaryF>
struct fold<type_list<Head, Tail...>, Init, BinaryF> {
    using type = typename fold<
        type_list<Tail...>,
        typename BinaryF::template apply<Init, Head>::type,
        BinaryF
    >::type;
};

template<typename List, typename Init, typename BinaryF>
using fold_t = typename fold<List, Init, BinaryF>::type;
```

Note that `BinaryF` here has an `apply` that takes two type parameters. Fold is the most general of the three — you can implement both `transform` and `filter` in terms of `fold`, though the direct implementations are clearer.

Here's a concrete example — using fold to count the number of pointer types in a list:

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
static_assert(result::value == 2);
```

## C++20 and Beyond: Inline Compile-Time Functions

The metafunction class boilerplate — a struct with a nested `apply` template — is verbose. C++20's constexpr lambdas and template lambdas chip away at this for value-level computations:

```cpp
// C++03-era metafunction class
struct square_f {
    template<typename T>
    struct apply {
        using type = std::integral_constant<int, T::value * T::value>;
    };
};

// C++20 constexpr lambda (for values, not types)
constexpr auto square = [](auto x) { return x * x; };
static_assert(square(5) == 25);
```

For pure value computations, constexpr functions and lambdas have largely replaced template metaprogramming. You don't need a metafunction to square an integer at compile time anymore. But for type-level computations — transforming `int` to `int*`, filtering types by properties, restructuring type lists — the metafunction machinery remains necessary. Types aren't values you can capture in a lambda. You can't write `[](auto) -> type { ... }` in any meaningful way.

C++23 and proposals beyond continue to narrow the gap. But as of today, if you're computing with types, you're writing metafunctions.

## Practical Example: A Type Transformation Pipeline

Let's put everything together. Given a type list, we want to: remove references, add const, then wrap in `std::optional`. Three transformations, chained.

First, the metafunction classes:

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

Now, applying them one at a time:

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

Or, using `compose` to build a single metafunction that does all three:

```cpp
using pipeline = compose<wrap_optional_f, compose<add_const_f, remove_reference_f>>;
using result = transform_t<input, pipeline>;
```

One pass. Three transformations fused into a single metafunction class. The compiler applies all three to each type in sequence, producing the final list in one `transform` call.

You could also write a variadic `pipeline` helper that reads left-to-right instead of nesting `compose`:

```cpp
template<typename... Fs>
struct pipeline;

template<typename F>
struct pipeline<F> : F {};

template<typename F, typename... Rest>
struct pipeline<F, Rest...> {
    template<typename T>
    struct apply {
        using intermediate = typename F::template apply<T>::type;
        using type = typename pipeline<Rest...>::template apply<intermediate>::type;
    };
};

using my_pipeline = pipeline<remove_reference_f, add_const_f, wrap_optional_f>;
using result = transform_t<input, my_pipeline>;
```

Now the transformations read in application order: remove reference, then add const, then wrap in optional. This is the compile-time equivalent of a Unix pipe or a Rust iterator chain. Data flows left to right through a sequence of transformations, each one a first-class function value.

## The Full Picture

Here's where we stand. The compile-time language now has:

- **Values**: types and integral constants
- **Variables**: type aliases
- **Functions**: templates with `::type` / `::value`
- **Branching**: specialization, `if constexpr`, `std::conditional`
- **Recursion**: recursive template instantiation
- **Data structures**: type lists (variadic templates)
- **Pattern matching**: partial specialization
- **Higher-order functions**: metafunction classes and template template parameters
- **Composition**: `compose`, `pipeline`, `transform`, `filter`, `fold`

That's a full functional programming language. It's Lisp with angle brackets — immutable data, recursive computation, higher-order functions, and lists as the primary data structure. The syntax is hostile. The error messages are worse. But the computational model is clean, and once you see it, template metaprogramming stops being dark magic and becomes straightforward (if verbose) functional programming.

The next posts will push further into what you can build with this machinery. But the hard conceptual work is done. Everything from here is application.
