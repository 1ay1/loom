---
title: "Control Flow at Compile Time — if constexpr, SFINAE, Concepts, and Tag Dispatch"
date: 2026-02-09
slug: control-flow-at-compile-time
tags: compile-time-cpp, if-constexpr, sfinae, concepts, tag-dispatch
excerpt: Every language needs branching. The compile-time language has four different if statements, each invented in a different decade. Here's how they all work.
---

Every programming language needs a way to branch. Runtime C++ has `if`, `switch`, the ternary operator. The compile-time language that lives inside the C++ compiler also needs branching — but it acquired its `if` statements one at a time, across three decades, each one a reaction to the last one's shortcomings.

The result is that modern C++ has at least four distinct mechanisms for "do different things depending on a type." They range from elegant to arcane, and you need to understand all of them because you'll encounter all of them in real codebases. Let's trace the lineage.

We'll use a running example throughout: **write a function `to_string_repr` that converts a value to a debug string.** For integers, it calls `std::to_string`. For C-strings (`const char*`), it wraps them in quotes. For everything else with a `.str()` method, it calls that. Simple enough at runtime — three `if` branches. At compile time, it takes more thought.

## Template Specialization: The Original If/Else

Template specialization is the oldest branching mechanism, available since C++98. The idea: you write a primary template (the default case), then write specializations for specific types (the specific cases). The compiler picks the most specific match.

```cpp
// Primary template — the "else" branch
template <typename T>
struct IsPointer {
    static constexpr bool value = false;
};

// Partial specialization — the "if T is a pointer" branch
template <typename T>
struct IsPointer<T*> {
    static constexpr bool value = true;
};

static_assert(!IsPointer<int>::value);
static_assert(IsPointer<int*>::value);
static_assert(IsPointer<const char*>::value);
```

This is pattern matching on types. The primary template matches everything. The specialization `IsPointer<T*>` matches anything that's a pointer. The compiler always prefers the more specific match. You can think of it as:

- Primary template = default case
- Full specialization (`template<> struct X<int>`) = exact match on a specific type
- Partial specialization (`template<typename T> struct X<T*>`) = match on a structural pattern

Applied to our running example:

```cpp
template <typename T>
struct ToStringRepr {
    // Default: assume .str() exists
    static std::string convert(const T& val) { return val.str(); }
};

template <>
struct ToStringRepr<int> {
    static std::string convert(int val) { return std::to_string(val); }
};

template <>
struct ToStringRepr<const char*> {
    static std::string convert(const char* val) {
        return std::string("\"") + val + "\"";
    }
};
```

This works, but it's verbose. Each "branch" is an entirely separate struct definition. And you can only partially specialize class templates, not function templates — so you end up wrapping everything in structs even when a plain function would be more natural.

## Tag Dispatch: Compile-Time Enums

Tag dispatch emerged as a pattern in the late 1990s, heavily used in the STL. The insight: overload resolution is already a compile-time branching mechanism. If you pass different *types* as arguments, the compiler picks different overloads. So create empty types that exist purely to steer overload resolution.

```cpp
struct integral_tag {};
struct c_string_tag {};
struct has_str_tag {};

// Pick the right tag for a type
template <typename T>
struct ToStringTag {
    using type = has_str_tag;  // default
};

template <>
struct ToStringTag<int> {
    using type = integral_tag;
};

template <>
struct ToStringTag<const char*> {
    using type = c_string_tag;
};

// Overloads dispatched by tag
template <typename T>
std::string to_string_repr_impl(const T& val, has_str_tag) {
    return val.str();
}

template <typename T>
std::string to_string_repr_impl(const T& val, integral_tag) {
    return std::to_string(val);
}

template <typename T>
std::string to_string_repr_impl(const T& val, c_string_tag) {
    return std::string("\"") + val + "\"";
}

// Entry point
template <typename T>
std::string to_string_repr(const T& val) {
    return to_string_repr_impl(val, typename ToStringTag<T>::type{});
}
```

The canonical real-world example is `std::advance`. The standard library needs to advance an iterator by `n` steps. For random-access iterators, that's `it += n` (constant time). For bidirectional iterators, it's a loop of `++it` or `--it`. For input iterators, it's a forward-only loop.

The iterator category tags — `std::random_access_iterator_tag`, `std::bidirectional_iterator_tag`, `std::input_iterator_tag` — are empty structs that exist solely so `std::advance` can dispatch:

```cpp
template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::random_access_iterator_tag) {
    it += n;
}

template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::bidirectional_iterator_tag) {
    if (n > 0) while (n--) ++it;
    else while (n++) --it;
}

template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::input_iterator_tag) {
    while (n--) ++it;
}

template <typename It, typename Distance>
void advance(It& it, Distance n) {
    advance_impl(it, n,
        typename std::iterator_traits<It>::iterator_category{});
}
```

`std::true_type` and `std::false_type` are the most fundamental tags — compile-time booleans. They're the basis for most type traits: `std::is_integral<int>` inherits from `std::true_type`, which means you can dispatch on it directly.

Tag dispatch is more flexible than raw specialization because it works with regular functions and overload resolution. But it requires an extra indirection layer (the `_impl` functions and the tag-mapping traits), and it doesn't scale cleanly when you have many independent conditions to check.

## SFINAE: The Accidental If Statement

SFINAE — "Substitution Failure Is Not An Error" — is the most powerful and most confusing compile-time branching mechanism. It was originally a corner of the template instantiation rules, not a deliberate language feature. But people discovered you could exploit it to conditionally enable or disable templates.

The rule: when the compiler tries to substitute template arguments into a function template's signature, and the substitution produces an invalid type, the compiler doesn't emit an error. It silently removes that template from the overload set and moves on. This only applies to the *immediate context* of the substitution — errors inside the function body are still hard errors.

`std::enable_if` is the tool that weaponizes this rule:

```cpp
template <bool Condition, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    using type = T;
};
```

When `Condition` is true, `enable_if<true, T>::type` is `T`. When it's false, `enable_if<false, T>::type` doesn't exist — and accessing it causes a substitution failure, which silently removes the function from consideration.

Applied to our example:

```cpp
// For integers
template <typename T>
std::enable_if_t<std::is_integral_v<T>, std::string>
to_string_repr(const T& val) {
    return std::to_string(val);
}

// For C-strings
template <typename T>
std::enable_if_t<std::is_same_v<T, const char*>, std::string>
to_string_repr(const T& val) {
    return std::string("\"") + val + "\"";
}

// For types with .str()
template <typename T, typename = decltype(std::declval<T>().str())>
std::string to_string_repr(const T& val) {
    return val.str();
}
```

That last overload is the really clever (and really ugly) part. The `typename = decltype(std::declval<T>().str())` is a default template parameter. If `T` doesn't have a `.str()` method, `decltype(std::declval<T>().str())` is invalid, substitution fails, and the overload vanishes.

SFINAE is powerful because it can test almost anything: whether a type has a method, whether an expression is valid, whether a conversion exists. But look at that code. The return type is `std::enable_if_t<std::is_integral_v<T>, std::string>` — the actual return type (`std::string`) is buried inside a metaprogramming construct. The intent is obscured by the mechanism.

SFINAE dominated C++ metaprogramming from roughly 2003 to 2017. It works, it's flexible, and it's a nightmare to debug. When two SFINAE overloads have overlapping conditions, you get ambiguity errors that require a PhD in template resolution to decipher.

## if constexpr: The Real If Statement (C++17)

C++17 finally gave us what we wanted all along: an actual if/else for compile-time branching, spelled `if constexpr`.

```cpp
template <typename T>
std::string to_string_repr(const T& val) {
    if constexpr (std::is_integral_v<T>) {
        return std::to_string(val);
    } else if constexpr (std::is_same_v<T, const char*>) {
        return std::string("\"") + val + "\"";
    } else {
        return val.str();
    }
}
```

That's it. One function, readable top-to-bottom, does everything the previous three approaches did.

The critical difference from a regular `if`: **the false branch is discarded, not just skipped.** It isn't compiled at all. This is what makes `if constexpr` fundamentally different from `if` with a compile-time constant:

```cpp
template <typename T>
void process(T val) {
    if (std::is_integral_v<T>) {
        val += 1;       // Error if T is std::string — still compiled
    }

    if constexpr (std::is_integral_v<T>) {
        val += 1;       // Fine — discarded entirely when T is std::string
    }
}
```

With a regular `if`, both branches are compiled even if the condition is known at compile time. The compiler might optimize away the dead branch in the generated code, but it still must be *valid code*. With `if constexpr`, the discarded branch only needs to be syntactically valid (parseable), not semantically valid. The compiler doesn't check types, doesn't resolve function calls, doesn't do anything with it.

This is a superpower. You can write code that would be a type error for certain template arguments, and as long as it's in the discarded branch of an `if constexpr`, it compiles fine.

Limitation: `if constexpr` works inside function templates. It doesn't help with choosing between class template definitions or selecting between entirely different function signatures. For those, you still need specialization, tag dispatch, or concepts.

## Concepts and requires: The Type-Safe If (C++20)

Concepts are the final evolution: named, composable predicates on types, with first-class language support.

```cpp
#include <concepts>

template <typename T>
concept Integral = std::is_integral_v<T>;

template <typename T>
concept CString = std::same_as<T, const char*>;

template <typename T>
concept HasStr = requires(T t) {
    { t.str() } -> std::convertible_to<std::string>;
};
```

Now the branching becomes constrained overloads:

```cpp
std::string to_string_repr(Integral auto const& val) {
    return std::to_string(val);
}

std::string to_string_repr(CString auto const& val) {
    return std::string("\"") + val + "\"";
}

std::string to_string_repr(HasStr auto const& val) {
    return val.str();
}
```

Compare this with the SFINAE version. The intent is immediately clear. Each overload declares what it requires, and the concept name documents *why*.

Concepts also solve the ambiguity problem through **subsumption**: if concept A implies concept B (A subsumes B), then a function constrained on A is more specific and wins overload resolution. This gives you a partial ordering on constraints without any manual disambiguation.

```cpp
template <typename T>
concept Animal = requires(T t) { t.speak(); };

template <typename T>
concept Dog = Animal<T> && requires(T t) { t.fetch(); };

void interact(Animal auto& a) { a.speak(); }
void interact(Dog auto& d)    { d.fetch(); }  // More specific — wins for Dogs
```

You can also use `requires` clauses directly, without defining a named concept:

```cpp
template <typename T>
    requires std::integral<T>
std::string to_string_repr(const T& val) {
    return std::to_string(val);
}
```

This is essentially SFINAE with readable syntax. The compiler checks the constraint; if it fails, the overload is removed from the candidate set. Same mechanism, vastly better ergonomics.

## The Same Problem, Four Ways — Side by Side

Here's a condensed comparison. The task: return `true` if a type is "printable" (has an `operator<<` for `std::ostream`).

**Template specialization** — requires manual opt-in for each type:
```cpp
template <typename T> struct IsPrintable : std::false_type {};
template <> struct IsPrintable<int> : std::true_type {};
template <> struct IsPrintable<std::string> : std::true_type {};
// ... enumerate every printable type
```

**Tag dispatch** — routes through overloaded helpers:
```cpp
template <typename T>
bool is_printable_impl(std::true_type) { return true; }
template <typename T>
bool is_printable_impl(std::false_type) { return false; }
// Still need a trait to produce the tag
```

**SFINAE** — auto-detects the capability:
```cpp
template <typename T, typename = void>
struct IsPrintable : std::false_type {};

template <typename T>
struct IsPrintable<T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};
```

**Concepts** — the same auto-detection, readable:
```cpp
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val };
};
```

The progression is clear. Each mechanism does more with less ceremony.

## Which One to Use Today

The decision tree is straightforward:

**Use concepts** (C++20) when you're constraining template parameters. They're readable, composable, and produce clear error messages. This should be your default.

**Use `if constexpr`** (C++17) when you need different code paths *inside a single function body*. This is the go-to for "do X for type A, Y for type B" logic within one function template.

**Use template specialization** when you're defining type traits or need to associate data/types with specific template arguments. It's still the right tool for mapping types to values.

**Use SFINAE** when you're stuck on C++14 or earlier, or when you're working within a codebase that predates concepts. Understand it for reading existing code. Avoid writing new SFINAE if you have alternatives.

**Tag dispatch** is mostly historical. You'll see it in the standard library implementation and older codebases. It's elegant in its own way, but concepts and `if constexpr` cover its use cases with less boilerplate.

The compile-time language's branching story is one of the best examples of C++'s layered evolution. Each mechanism was the best answer available at the time. Understanding all of them isn't just academic — real codebases span decades, and you'll read code written with every one of these approaches. The good news: the latest tools are genuinely good. `if constexpr` and concepts make compile-time branching almost as natural as runtime branching. Almost.
