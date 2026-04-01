---
title: "Types as Values — The Core Trick of Compile-Time C++"
date: 2026-02-07
slug: types-as-values
tags: compile-time-cpp, type-traits, metaprogramming, type-manipulation
excerpt: In the compile-time language, types ARE the values. You pass them to functions, store them in data structures, and compute with them. This is the key insight that makes everything else click.
---

Last post, I told you C++ has two languages. One that runs on your CPU, one that runs inside the compiler. You probably nodded along, thought "huh, interesting," and figured the compile-time language was some esoteric corner of C++ that only library authors care about.

This post is where it stops being abstract. This post is where you see The Trick. And once you see it, you can't unsee it. Every template, every type trait, every `using` declaration — they'll all snap into focus like one of those magic eye puzzles from the '90s.

Here's The Trick:

**In the compile-time language, types are the values.**

Not metaphorically. Not "kind of like values." Types *are* the values. You pass them to functions. You return them from functions. You store them in variables. You branch on them. You put them in lists. Everything you do with `int` and `std::string` at runtime, the compile-time language does with `int` and `std::string` themselves — not instances of those types, but the types themselves, as data.

Let me make this concrete.

## Variables: Just Type Aliases

At runtime, a variable holds a value:

```cpp
int x = 42;
double pi = 3.14159;
```

At compile time, a "variable" holds a type:

```cpp
using T = int;               // "variable T holds the value int"
using Ptr = int*;            // "variable Ptr holds the value int*"
using Ref = const double&;   // "variable Ref holds the value const double&"
```

That's it. `using T = int` is variable assignment. You're storing the type `int` in a compile-time variable called `T`. You can use `T` anywhere you'd use `int`:

```cpp
T x = 42;        // same as: int x = 42;
Ptr p = &x;      // same as: int* p = &x;
```

I know `using` aliases feel like a convenience feature. They're not. They're the compile-time language's variable declaration syntax. Every `using` statement is storing a type-value in a named location for later use.

## Functions: Templates with ::type

At runtime, a function takes values and returns a value:

```cpp
int square(int x) { return x * x; }
```

At compile time, a "function" takes types and returns a type. It's written as a template struct with a nested `using type`:

```cpp
template<typename T>
struct add_pointer {
    using type = T*;
};
```

This is a function. It takes a type `T` as input. It returns `T*` as output. The "return value" lives in `::type`. The function call looks like this:

```cpp
add_pointer<int>::type      // returns int*
add_pointer<double>::type   // returns double*
add_pointer<char*>::type    // returns char** (pointer to pointer to char!)
```

That `::type` at the end is the "give me the return value" syntax. It looks weird, but once you accept that templates are functions and `::type` is how you extract the result, it becomes readable:

- `add_pointer` = the function name
- `<int>` = the argument
- `::type` = "call the function and give me the result"

Think of it like calling a function and accessing a field on the result. Because that's exactly what it is — the template instantiation creates a struct, and you access its `type` member.

## A Whole Standard Library of These Things

Here's where it gets cool. The `<type_traits>` header isn't a mysterious collection of dark magic. It's a *standard library* for the compile-time language. It's full of functions that take types and return types. Once you see the pattern, the entire header becomes readable.

`std::remove_const` strips `const` from a type:

```cpp
std::remove_const<const int>::type   // int
std::remove_const<int>::type         // int (already non-const, no-op)
std::remove_const<const double*>::type  // const double* (only strips top-level const)
```

How does it work? Pattern matching. The implementation is almost embarrassingly simple:

```cpp
template<typename T>
struct remove_const {
    using type = T;         // default: return the input unchanged
};

template<typename T>
struct remove_const<const T> {
    using type = T;         // if input matches "const T", return just T
};
```

The primary template handles the general case: if `T` isn't `const`, just return it unchanged. The specialization matches `const T` specifically and strips the `const`. The compiler tries the specialization first. If the argument has a `const` on the outside, it matches and `T` gets bound to whatever's underneath. Otherwise, it falls through to the primary.

This is exactly what you'd write in a language with pattern matching:

```
remove_const(const T) = T
remove_const(T)       = T
```

Same logic. Different syntax. The C++ version has more angle brackets and colons, but the *semantics* are identical.

`std::remove_reference` works the same way, with two patterns:

```cpp
template<typename T>
struct remove_reference { using type = T; };

template<typename T>
struct remove_reference<T&> { using type = T; };

template<typename T>
struct remove_reference<T&&> { using type = T; };
```

Three cases: not a reference, an lvalue reference (`T&`), an rvalue reference (`T&&`). Each strips its qualifier. Three lines of a pattern-matching function. That's the entire implementation of `std::remove_reference`. Not scary at all.

## The `_t` and `_v` Convention (A.K.A. "Please Make the Syntax Less Painful")

Writing `typename std::remove_const<T>::type` everywhere is brutal. The `typename` is required because when `T` is unknown, the compiler can't tell if `::type` is a type or a value — it's a grammar ambiguity in C++, and `typename` resolves it. But it means your code fills up with `typename` keywords like they're the world's most annoying tax.

C++14 gave us relief:

```cpp
template<typename T>
using remove_const_t = typename remove_const<T>::type;
```

Now you write `std::remove_const_t<const int>` instead of `typename std::remove_const<const int>::type`. Same result. Way less noise. The `_t` suffix means "I'm a type alias that calls `::type` for you."

For traits that return boolean values, there's `_v`:

```cpp
template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;
```

So `std::is_same_v<int, int>` is `true`, and `std::is_same_v<int, double>` is `false`. The `_v` suffix means "I extract `::value` for you."

Rule of thumb: when you see `_t`, think "calling a type function." When you see `_v`, think "asking a yes/no question about types."

The standard library provides `_t` and `_v` shortcuts for basically every trait. Use them. Your eyes will thank you.

## Building Your Own Type Functions

You're not limited to what the standard library provides. Writing your own type traits is just writing compile-time functions. And honestly? It's fun. Let me show you.

**`is_pointer`** — returns whether a type is a pointer:

```cpp
template<typename T>
struct is_pointer : std::false_type {};

template<typename T>
struct is_pointer<T*> : std::true_type {};

static_assert(is_pointer<int*>::value);    // true!
static_assert(!is_pointer<int>::value);    // false!
```

Wait, what's `std::false_type` and `std::true_type`? They're just structs whose `value` is `false` and `true` respectively. By inheriting from them, your trait automatically gets a `::value` member. It's the compile-time language's way of saying "this function returns a boolean."

The primary template inherits from `false_type` (the default answer is "no, it's not a pointer"). The specialization for `T*` inherits from `true_type` (if it matches the pointer pattern, the answer is "yes"). The compiler pattern-matches the input type and picks the right answer.

**`conditional`** — the compile-time ternary operator:

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

When `Cond` is `true`, the primary template wins, and `type` is `Then`. When `Cond` is `false`, the specialization kicks in, and `type` is `Else`. This is `if/else` for types:

```cpp
using T = std::conditional_t<(sizeof(int) > 4), long, int>;
// On most platforms: T = int, because sizeof(int) is 4, not > 4
```

The compiler evaluates `sizeof(int) > 4`, gets `false`, picks `int`. No runtime conditional. No branch. Just the type `int`, selected during compilation.

## A Practical Example: Type-Level Decision Making

Let's build something real. Suppose you're writing a numeric library and you need a "widened" type — when you accumulate a bunch of `int8_t` values, you don't want overflow, so you accumulate into `int64_t` instead. When you sum `float` values, you want `double` precision.

This is a function from types to types:

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

Now the cool part — using this in real code:

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

When you call `sum` on a `vector<int8_t>`, the compile-time language kicks in: it looks up `widen_t<int8_t>`, gets `int64_t`, and uses that as the accumulator type. No overflow. When you call it on a `vector<float>`, it accumulates into `double`. No precision loss.

The type-level function `widen` made a *decision at compile time* that affects the *runtime behavior* of the code. It chose the right accumulator type, inserted the right conversions, generated the right instructions. Zero runtime cost. The decision was made before the program existed.

This is the power of types-as-values. You're not just annotating your code with types for the compiler to check. You're *computing* with types, making decisions, and generating specialized code. The compile-time language is programming the runtime language.

## decltype and declval: Asking the Compiler Questions

Sometimes you need to observe what type an expression would produce, without actually evaluating it. `decltype` does exactly that:

```cpp
int x = 42;
decltype(x) y = 10;       // y is int (same type as x)
decltype(x + 1.0) z = 0;  // z is double (int + double promotes to double)
```

`decltype` asks: "if I wrote this expression, what type would the result be?" It never evaluates the expression. It just analyzes it at compile time and reports the type.

Now combine it with `std::declval`, and things get really interesting. `declval<T>()` conjures a "pretend" value of type `T` out of thin air. It doesn't actually create one — calling it at runtime is a compile error. It exists solely for use inside `decltype` expressions, where nothing is actually executed:

```cpp
template<typename T, typename U>
using add_result_t = decltype(std::declval<T>() + std::declval<U>());
```

This asks: "If I had a `T` and a `U`, and I added them with `+`, what type would I get?" The answer might be `int`, `double`, `std::string`, or anything else that `operator+` returns for those types. No value is ever created. The compiler just analyzes the hypothetical expression and reports the type.

Why `declval` instead of just declaring a variable? Because `T` might not be default-constructible. If `T`'s constructor is private, deleted, or requires arguments, you can't write `T{}`. `declval` sidesteps the issue entirely — it promises a value of type `T` for analysis purposes without ever constructing one. It's a polite fiction that the compiler is happy to entertain.

This pattern shows up everywhere:

```cpp
// Does T have a .size() method? What does it return?
template<typename T>
using size_result_t = decltype(std::declval<T>().size());

// What does T's operator* return? (useful for iterator types)
template<typename T>
using deref_t = decltype(*std::declval<T>());
```

These are compile-time *queries* about the structure of types. You're asking the compiler: "If I did this with a T, would it work? And what type would I get?" This is the foundation of SFINAE and concepts — checking what operations a type supports, at compile time, before trying to use them. We'll cover those in later posts.

## Type Tags: Types as Runtime Arguments

Sometimes you want to use a type to steer runtime behavior. Not the type's value — the type *itself*. Empty structs make perfect tags for this:

```cpp
struct float_tag {};
struct int_tag {};
struct string_tag {};

void process(float_tag, float value)   { /* float-specific logic */ }
void process(int_tag, int value)       { /* int-specific logic */ }
void process(string_tag, std::string_view value) { /* string logic */ }
```

The empty structs carry no data. They exist only to steer overload resolution — the compiler sees the tag type, picks the right `process` overload, and optimizes the tag object away completely. Zero bytes of overhead. The tag only existed to make a compile-time decision.

`std::type_identity<T>` serves a different, sneakier purpose. It wraps a type so the compiler won't try to deduce it:

```cpp
template<typename T>
void fill(std::vector<T>& v, std::type_identity_t<T> value) {
    for (auto& elem : v) elem = value;
}
```

Without `type_identity_t`, calling `fill(vec_of_ints, 3.14)` creates a deduction conflict: `T` deduced as `int` from the vector, `T` deduced as `double` from the second argument. With `type_identity_t`, the second parameter becomes a non-deduced context. `T` is deduced only from the vector, and `3.14` is implicitly converted to `int`. Sneaky, useful, and entirely a compile-time maneuver.

## The Mental Model

Here's the mapping, laid out cleanly:

| Runtime | Compile Time |
|---------|-------------|
| Value (`42`, `"hello"`) | Type (`int`, `std::string`) |
| Variable (`int x`) | Type alias (`using T = int`) |
| Function (`int f(int)`) | Class template with `::type` |
| Function call (`f(42)`) | Template instantiation + `::type` |
| Return value | Nested `using type = ...` |
| Boolean | `std::true_type` / `std::false_type` |
| If/else | `std::conditional` / specialization |
| Pattern match | Template specialization |

Every type trait in `<type_traits>` is a function in this compile-time language. `remove_const`, `add_pointer`, `decay`, `common_type`, `conditional` — they're all functions that take types and return types. The entire header is a standard library for a language that runs inside your compiler.

Once this clicks — really clicks — metaprogramming stops being magic. It's just programming in a language with unfamiliar syntax but very familiar semantics. You already know how to call functions, check conditions, and work with data. The data is types now. The functions are templates. But the *thinking* is the same.

The next post pushes further. We'll look at how the compile-time language does branching — `if constexpr`, SFINAE, concepts, and tag dispatch. Four different "if statements," each invented in a different decade. C++ is nothing if not thorough.
