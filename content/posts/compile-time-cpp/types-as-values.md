---
title: "Types as Values — The Core Trick of Compile-Time C++"
date: 2026-02-07
slug: types-as-values
tags: compile-time-cpp, type-traits, metaprogramming, type-manipulation
excerpt: In the compile-time language, types ARE the values. You pass them to functions, store them in data structures, and compute with them. This is the key insight.
---

Everything in the previous posts has been building to this single idea: at compile time, types are values.

At runtime, you compute with `int`, `std::string`, `double`. You pass them to functions, store them in variables, branch on their contents, and return them. At compile time, you do the exact same thing — except the "values" you pass around, store, branch on, and return are types themselves. `int`, `const double*`, `std::vector<std::string>` — these aren't just annotations that help the compiler check your code. In the compile-time language, they're the data you compute with.

This is the fundamental insight of C++ metaprogramming. Everything else — SFINAE, concepts, type traits, template specialization — is just machinery built on top of this one idea.

## Variables, Functions, Return Values

Let's map the runtime concepts to their compile-time equivalents.

A **variable** at compile time is a type alias:

```cpp
using T = int;               // "variable T holds the value int"
using Ptr = int*;            // "variable Ptr holds the value int*"
using Ref = const double&;   // "variable Ref holds the value const double&"
```

A **function** at compile time is a template that takes types and produces a type:

```cpp
template<typename T>
struct add_pointer {
    using type = T*;
};
```

A **return value** is a nested typedef called `type`:

```cpp
add_pointer<int>::type   // returns int*
add_pointer<double>::type // returns double*
```

A **function call** is template instantiation followed by `::type` to extract the result. That `::type` suffix is the calling convention of the compile-time language. The template is the function. The angle brackets are the argument list. The `::type` is "give me the result."

Once you see this mapping, the entire `<type_traits>` header stops being cryptic and becomes a standard library of compile-time functions.

## Type Traits as Functions

Consider `std::remove_const`. At runtime, you might write a function that strips the `const` qualifier off a value's description. At compile time, `remove_const` does exactly that:

```cpp
std::remove_const<const int>::type  // int
std::remove_const<int>::type        // int (already non-const, no-op)
```

This is a function call. The input is `const int`. The output is `int`. Here's the implementation:

```cpp
template<typename T>
struct remove_const {
    using type = T;
};

template<typename T>
struct remove_const<const T> {
    using type = T;
};
```

Two things are happening at once. The primary template handles the general case: if `T` isn't `const`-qualified, just return it unchanged. The partial specialization handles the `const T` case: strip the `const` and return `T`. This is pattern matching and function definition fused into one construct. The compiler tries the specialization first. If the argument matches the pattern `const T`, it uses that branch. Otherwise, it falls through to the primary template.

This is exactly how you'd write a function in a pattern-matching language:

```
remove_const(const T) = T
remove_const(T)       = T
```

The C++ syntax is noisier. The semantics are identical.

The same pattern shows up everywhere:

```cpp
template<typename T>
struct remove_reference { using type = T; };

template<typename T>
struct remove_reference<T&> { using type = T; };

template<typename T>
struct remove_reference<T&&> { using type = T; };
```

Three patterns: not a reference, an lvalue reference, an rvalue reference. Each pattern strips the corresponding qualifier. Three lines of a pattern-matching function.

## The `_t` and `_v` Convention

Writing `typename std::remove_const<T>::type` everywhere is painful. The `typename` is required because `remove_const<T>::type` is a dependent name — the compiler can't be sure it's a type until it knows what `T` is. C++14 introduced alias templates to fix this:

```cpp
template<typename T>
using remove_const_t = typename remove_const<T>::type;
```

Now you write `std::remove_const_t<const int>` instead of `typename std::remove_const<const int>::type`. Same result, less noise.

For traits that return boolean values (technically `bool` constants, not types), there's the `_v` suffix:

```cpp
template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;
```

So `std::is_same_v<int, int>` is `true`, and `std::is_same_v<int, double>` is `false`. The `_v` variant extracts `::value` for you the same way `_t` extracts `::type`.

These conventions turned the compile-time language from something only template wizards could stomach into something any C++ programmer can read. When you see `_t`, think "type function call." When you see `_v`, think "boolean query."

## Building Your Own Type Functions

You aren't limited to the standard library. Writing your own type traits is just writing compile-time functions.

**`is_pointer`** — returns whether a type is a pointer:

```cpp
template<typename T>
struct is_pointer : std::false_type {};

template<typename T>
struct is_pointer<T*> : std::true_type {};

static_assert(is_pointer<int*>::value);
static_assert(!is_pointer<int>::value);
```

The primary template inherits from `std::false_type` (a struct whose `value` is `false`). The specialization for `T*` inherits from `std::true_type`. Pattern matching on the structure of the type determines the result.

**`add_const`** — unconditionally adds const:

```cpp
template<typename T>
struct add_const {
    using type = const T;
};

// Usage:
static_assert(std::is_same_v<add_const<int>::type, const int>);
```

Straightforward. The function takes a type and returns `const T`.

**`conditional`** — the compile-time ternary:

```cpp
template<bool Cond, typename Then, typename Else>
struct conditional {
    using type = Then;
};

template<typename Then, typename Else>
struct conditional<false, Then, Else> {
    using type = Else;
};
```

When `Cond` is `true`, the primary template is selected, and `type` is `Then`. When `Cond` is `false`, the specialization kicks in, and `type` is `Else`. This is `if/else` at the type level:

```cpp
using T = std::conditional_t<(sizeof(int) > 4), long, int>;
// On most platforms: T = int, because sizeof(int) is 4
```

You can chain these for multi-way branching, nest them for complex logic, or combine them with other traits for type-level decision trees.

## Type Tags and Type Identity

Sometimes you need to pass a type as an argument to a runtime function — not to use the type's values, but to select behavior based on the type itself. Empty structs work as compile-time tags:

```cpp
struct float_tag {};
struct int_tag {};
struct string_tag {};

void process(float_tag, float value)   { /* float logic */ }
void process(int_tag, int value)       { /* int logic */ }
void process(string_tag, std::string_view value) { /* string logic */ }
```

This is called tag dispatch. The empty structs carry no data. They exist only to carry type information into overload resolution. They're compile-time enum values — the compiler sees the tag type, selects the right overload, and optimizes the tag object away completely.

`std::type_identity<T>` serves a different purpose. It wraps a type so the compiler won't try to deduce it:

```cpp
template<typename T>
void fill(std::vector<T>& v, std::type_identity_t<T> value) {
    for (auto& elem : v) elem = value;
}
```

Without `type_identity_t`, calling `fill(vec_of_ints, 3.14)` would create a deduction conflict — `T` deduced as `int` from the vector, `T` deduced as `double` from the second argument. With `type_identity_t`, the second parameter is a non-deduced context. `T` is deduced only from the vector, and the second argument is implicitly converted. The type is "held" inside `type_identity` so the compiler leaves it alone during deduction.

## decltype and declval

`decltype` is the compiler's type observation operator. It takes an expression and tells you what type it would have — without evaluating it:

```cpp
int x = 42;
decltype(x) y = 10;       // y is int
decltype(x + 1.0) z = 0;  // z is double (int + double promotes)
```

This is useful, but the real power comes when you combine it with `std::declval`. `declval<T>()` produces a "fake" value of type `T`. It never actually creates the value — calling it at runtime is a compile error. It exists solely for use inside `decltype` expressions, where nothing is evaluated:

```cpp
template<typename T, typename U>
using add_result_t = decltype(std::declval<T>() + std::declval<U>());
```

This asks: "If I had a `T` and a `U`, and I added them, what type would I get?" No `T` or `U` value is ever created. The compiler just analyzes the expression and reports the type.

Why do you need `declval` instead of just declaring a variable? Because `T` might not be default-constructible. You can't write `T{}` if `T`'s constructor is deleted or private. `declval` sidesteps the issue entirely — it promises a value of type `T` for expression analysis without ever constructing one.

This pattern is everywhere in real metaprogramming:

```cpp
// Does T have a .size() method?
template<typename T>
using size_result_t = decltype(std::declval<T>().size());

// What does T's operator* return?
template<typename T>
using deref_t = decltype(*std::declval<T>());
```

These are compile-time queries about the structure of types. You ask the compiler "would this expression be valid, and if so, what type would it have?" This is the foundation of SFINAE and concepts, which we'll cover in later posts.

## Putting It Together: A Type-Level Decision Function

Here's a practical example. Suppose you want a type function that takes a numeric type and returns the "widened" version — `float` becomes `double`, small integers become `int64_t`, everything else stays unchanged:

```cpp
template<typename T>
struct widen {
    using type = T;  // default: no change
};

template<> struct widen<float>    { using type = double; };
template<> struct widen<int8_t>   { using type = int64_t; };
template<> struct widen<int16_t>  { using type = int64_t; };
template<> struct widen<int32_t>  { using type = int64_t; };
template<> struct widen<uint8_t>  { using type = uint64_t; };
template<> struct widen<uint16_t> { using type = uint64_t; };
template<> struct widen<uint32_t> { using type = uint64_t; };

template<typename T>
using widen_t = typename widen<T>::type;
```

Now `widen_t<float>` is `double`, `widen_t<int8_t>` is `int64_t`, and `widen_t<std::string>` is `std::string`. You can use this in a generic accumulator that avoids overflow:

```cpp
template<typename Container>
auto sum(const Container& c) {
    using T = typename Container::value_type;
    using Wide = widen_t<T>;

    Wide total = 0;
    for (const auto& elem : c) {
        total += static_cast<Wide>(elem);
    }
    return total;
}
```

Summing a `vector<int8_t>` accumulates into `int64_t`. Summing a `vector<float>` accumulates into `double`. The type-level function `widen` made a decision at compile time that affects the runtime behavior of the code — choosing the right accumulator type, inserting the right conversions, generating the right instructions. No runtime cost. The decision is made before the program exists.

## The Mental Model

Here is the mapping, stated plainly:

| Runtime | Compile Time |
|---------|-------------|
| Value | Type |
| Variable | Type alias (`using`) |
| Function | Class template with `::type` |
| Function call | Template instantiation + `::type` |
| Return value | Nested `using type = ...` |
| Boolean | `std::true_type` / `std::false_type` |
| If/else | `std::conditional` or specialization |
| Pattern match | Template specialization |

Every type trait in `<type_traits>` is a function in this compile-time language. `remove_const`, `add_pointer`, `decay`, `common_type`, `conditional` — they're all functions that take types and return types. The entire header is a standard library for a language that runs inside your compiler.

Once this clicks, metaprogramming stops being magic. It's just programming in a language with strange syntax and familiar semantics. The next post will push this further: we'll look at how to build compile-time data structures — lists, maps, and sequences — out of types and parameter packs.
