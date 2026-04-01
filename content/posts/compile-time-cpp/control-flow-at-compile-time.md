---
title: "Control Flow at Compile Time — if constexpr, SFINAE, Concepts, and Tag Dispatch"
date: 2026-02-09
slug: control-flow-at-compile-time
tags: compile-time-cpp, if-constexpr, sfinae, concepts, tag-dispatch
excerpt: Every language needs branching. The compile-time language has four different if statements, each invented in a different decade, each a reaction to the last one's shortcomings.
---

Every programming language needs a way to say "if this, do that, otherwise do the other thing." At runtime, C++ gives you `if`, `switch`, and the ternary operator. Simple. Clean. Everyone understands them.

The compile-time language also needs branching. And over three decades, C++ has invented not one, not two, but *four* completely different mechanisms for "do different things depending on a type." They range from elegant to horrifying, and the real kicker is that you need to understand all of them because you'll encounter all of them in real codebases. Legacy code doesn't rewrite itself.

Let's trace the family tree of compile-time branching, from the awkward grandparent to the modern offspring. We'll use a running example throughout: **write a function `to_string_repr` that converts a value to a debug string.** For integers, call `std::to_string`. For C-strings (`const char*`), wrap them in quotes. For anything else with a `.str()` method, call that. Three branches. Easy at runtime. At compile time... we'll see.

## Template Specialization: The Original If/Else (1990s)

Template specialization is the OG. It's been around since C++98, which means it's older than most people reading this post. The idea is beautifully simple: you write a "default" template, then write specialized versions for specific types. The compiler picks the most specific match.

```cpp
// Primary template — the "else" branch
template <typename T>
struct IsPointer {
    static constexpr bool value = false;
};

// Specialization — the "if T is a pointer" branch
template <typename T>
struct IsPointer<T*> {
    static constexpr bool value = true;
};

static_assert(!IsPointer<int>::value);       // nope
static_assert(IsPointer<int*>::value);       // yep!
static_assert(IsPointer<const char*>::value); // also yep!
```

Think of it as pattern matching on types:
- Primary template = the `default` case, matches everything
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

It works. But look at the boilerplate. Each "branch" is an entirely separate struct definition. You wanted a three-way if/else; you got three separate classes. And there's a deeper problem: you can only partially specialize class templates, not function templates. So you end up wrapping everything in structs even when a plain function would be more natural.

This is the template metaprogramming equivalent of writing every function as a class with a single method. It works, but it feels like using a wrench to hammer nails.

## Tag Dispatch: Compile-Time Enums via Overloading (Late 1990s)

Tag dispatch was invented by the STL implementers in the late '90s, and it's genuinely clever. The insight: C++ already has a compile-time branching mechanism built into the language — *overload resolution*. If you pass different types as arguments to a function, the compiler picks different overloads. So create empty structs that exist purely to steer the compiler toward the right overload.

It's like putting colored labels on your function arguments. The function doesn't use the label for anything. The label exists only so the compiler knows which overload to call.

```cpp
// The "labels" (tags)
struct integral_tag {};
struct c_string_tag {};
struct has_str_tag {};

// Mapping types to labels
template <typename T>
struct ToStringTag {
    using type = has_str_tag;  // default label
};

template <>
struct ToStringTag<int> {
    using type = integral_tag;
};

template <>
struct ToStringTag<const char*> {
    using type = c_string_tag;
};

// One overload per label
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

// Entry point: look up the tag, dispatch
template <typename T>
std::string to_string_repr(const T& val) {
    return to_string_repr_impl(val, typename ToStringTag<T>::type{});
}
```

The canonical real-world example is `std::advance`. The standard library needs to advance an iterator by `n` steps. For random-access iterators, that's `it += n` (constant time). For bidirectional iterators, it's a loop of `++it` or `--it`. For input iterators, it's forward-only.

Instead of a massive `if` chain, the STL uses tag dispatch with `std::random_access_iterator_tag`, `std::bidirectional_iterator_tag`, `std::input_iterator_tag` — empty structs that act like compile-time enum values:

```cpp
template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::random_access_iterator_tag) {
    it += n;  // O(1)
}

template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::bidirectional_iterator_tag) {
    if (n > 0) while (n--) ++it;
    else while (n++) --it;  // can go backwards
}

template <typename It, typename Distance>
void advance_impl(It& it, Distance n, std::input_iterator_tag) {
    while (n--) ++it;  // forward only
}

template <typename It, typename Distance>
void advance(It& it, Distance n) {
    advance_impl(it, n,
        typename std::iterator_traits<It>::iterator_category{});
}
```

The beauty: the dispatch happens at compile time. The tag type is determined by the iterator's category, the right overload is selected, and the other overloads don't exist in the generated code. For a `std::vector::iterator`, the compiler generates just `it += n`. No branching. No checking. Just the optimal code.

Tag dispatch is more flexible than raw specialization because it works with regular functions and overload resolution. But the boilerplate is significant — you need tags, a tag-mapping trait, `_impl` functions, and a wrapper. For a three-way dispatch, that's a lot of scaffolding.

## SFINAE: The Accidental If Statement (2003-2017)

Oh boy. Here we go.

SFINAE stands for "Substitution Failure Is Not An Error." It is simultaneously one of the most powerful, most confusing, and most abused features in C++ history. People have built entire careers on understanding SFINAE. Other people have sworn off C++ entirely because of it.

Here's the core idea, explained simply: when the compiler is trying to figure out which function template to call, it substitutes your template arguments into each candidate's signature. If the substitution produces nonsense — an invalid type, a type that doesn't exist — the compiler doesn't throw an error. It just quietly removes that candidate from the list and tries the next one.

"Substitution Failure Is Not An Error." The compiler shrugs and moves on.

This was originally a corner of the template instantiation rules. Nobody designed it as a branching mechanism. But people realized: if substitution failure removes a function from consideration, you can *intentionally cause* substitution failure to conditionally enable or disable function templates. If you write a function signature that's valid for some types and invalid for others, the compiler will automatically select it only for the valid types.

`std::enable_if` is the weapon that exploits this:

```cpp
template <bool Condition, typename T = void>
struct enable_if {};  // no ::type when Condition is false

template <typename T>
struct enable_if<true, T> {
    using type = T;  // ::type exists when Condition is true
};
```

When `Condition` is true, `enable_if<true, T>::type` is `T`. When it's false, `enable_if<false, T>::type` *doesn't exist* — accessing it causes a substitution failure, which silently removes the function from the overload set.

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

See that last overload? `typename = decltype(std::declval<T>().str())` is a default template parameter. If `T` doesn't have a `.str()` method, `decltype(std::declval<T>().str())` is invalid. Substitution failure. The overload vanishes silently.

This is *insanely powerful*. SFINAE can test whether a type has a method, whether an expression is valid, whether a conversion exists, whether a trait is satisfied. It can do nearly anything concepts can do — it just does it in the most unreadable way imaginable.

Look at that return type again: `std::enable_if_t<std::is_integral_v<T>, std::string>`. The actual return type — `std::string` — is buried inside a metaprogramming construct. The intent ("this function handles integers") is obscured by the mechanism ("this function's return type exists only when `is_integral_v<T>` is true, causing substitution failure otherwise"). It's like writing a love letter in assembly language.

SFINAE dominated C++ metaprogramming from roughly 2003 to 2017. Every serious template library used it extensively. It works. It's flexible. And it's a *nightmare* to debug. When two SFINAE overloads have overlapping conditions, you get ambiguity errors that require a PhD in overload resolution to decipher.

The good news: you don't need to write new SFINAE code. You need to *read* it, because it's everywhere in existing codebases and the standard library. But for new code, we have something much better.

## if constexpr: The Real If Statement, Finally (C++17)

After two decades of specialization, tag dispatch, and SFINAE, the C++ standards committee finally said: "What if we just... made an if statement that works at compile time?"

And lo, `if constexpr` was born, and the people rejoiced:

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

That's it. That's the whole thing. One function, readable top-to-bottom, doing everything the previous three approaches did. No structs. No tags. No `enable_if`. No substitution failure. Just... an if statement.

But here's the critical difference from a regular `if` — and this is what makes `if constexpr` fundamentally different, not just syntactically nicer:

**The false branch is discarded, not just skipped. It isn't compiled at all.**

This matters enormously. Compare:

```cpp
template <typename T>
void process(T val) {
    if (std::is_integral_v<T>) {
        val += 1;       // Error if T is std::string — both branches are compiled!
    }

    if constexpr (std::is_integral_v<T>) {
        val += 1;       // Fine — discarded entirely when T is std::string
    }
}
```

With a regular `if`, even though `std::is_integral_v<std::string>` is `false` and the branch would never execute, the compiler still *compiles* both branches. And `std::string` doesn't support `+= 1`, so it's a type error. The optimizer would remove the dead branch, but the type checker runs first and rejects it.

With `if constexpr`, the discarded branch only needs to be *syntactically valid* — it needs to parse. But it doesn't need to be *semantically valid*. The compiler doesn't check types, doesn't resolve function calls, doesn't do anything with it. It's as if the code doesn't exist for that instantiation.

This is a superpower. You can write code that would be a type error for certain template arguments, as long as it's in the discarded branch of an `if constexpr`. The branches are not "taken or not taken at runtime." They're "compiled or not compiled at all."

**Limitation:** `if constexpr` works inside function templates. It doesn't help with choosing between class template definitions or selecting between entirely different function signatures. For those, you still need specialization or concepts.

## Concepts and requires: The Type-Safe If (C++20)

Concepts are the final evolution. Where SFINAE was an accidental if statement, concepts are a *designed* one. Named, composable, readable predicates on types, with first-class language support.

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

Compare this with the SFINAE version. Go ahead, scroll back up. I'll wait.

Night and day, right? The intent is immediately clear. Each overload declares what it requires, and the concept name documents *why*. `HasStr` is so much more readable than `typename = decltype(std::declval<T>().str())` that it's almost comical they express the same constraint.

Concepts also solve the ambiguity problem through **subsumption**: if concept A implies concept B (A "subsumes" B), then a function constrained on A is more specific and wins in overload resolution. This gives you automatic partial ordering without any manual disambiguation:

```cpp
template <typename T>
concept Animal = requires(T t) { t.speak(); };

template <typename T>
concept Dog = Animal<T> && requires(T t) { t.fetch(); };

void interact(Animal auto& a) { a.speak(); }
void interact(Dog auto& d)    { d.fetch(); }  // More specific — wins for Dogs
```

If something is a `Dog`, both overloads match. But `Dog` subsumes `Animal` (every Dog is an Animal, but not every Animal is a Dog), so the `Dog` overload wins. No ambiguity. No manual tag dispatch. The compiler figures it out from the logical relationship between the concepts.

## Side by Side: The Same Problem, Four Ways

Here's the same task — return `true` if a type is "printable" (has `operator<<` for `std::ostream`) — solved with all four mechanisms:

**Template specialization** — manual opt-in for each type:
```cpp
template <typename T> struct IsPrintable : std::false_type {};
template <> struct IsPrintable<int> : std::true_type {};
template <> struct IsPrintable<std::string> : std::true_type {};
// ... hope you didn't forget any
```

**Tag dispatch** — more ceremony than a royal wedding:
```cpp
template <typename T>
bool is_printable_impl(std::true_type) { return true; }
template <typename T>
bool is_printable_impl(std::false_type) { return false; }
// Still need a trait to produce the tag...
```

**SFINAE** — auto-detects the capability, terrifies the reader:
```cpp
template <typename T, typename = void>
struct IsPrintable : std::false_type {};

template <typename T>
struct IsPrintable<T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};
```

**Concepts** — auto-detects the capability, readable by humans:
```cpp
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val };
};
```

The progression is clear. Each mechanism does more with less ceremony. Each was the best answer available at the time it was invented. The SFINAE version was state-of-the-art in 2010. The concept version is what you write today.

## Which One to Use Today

**Use concepts** (C++20) when you're constraining template parameters. They're readable, composable, and produce clear error messages. This should be your default.

**Use `if constexpr`** (C++17) when you need different code paths *inside a single function body*. "Do X for type A, do Y for type B" — this is `if constexpr`'s sweet spot.

**Use template specialization** when you're defining type traits or need to associate data/types with specific template arguments. It's still the right tool for mapping types to values.

**Use SFINAE** when you're stuck on C++14 or earlier, or when maintaining a codebase that predates concepts. Understand it for reading existing code. Don't write new SFINAE if you have alternatives. Life is too short.

**Tag dispatch** is mostly historical. You'll see it in the standard library implementation and older codebases. It's elegant in its own way, but concepts and `if constexpr` cover its use cases with less boilerplate.

The compile-time language's branching story is one of the best examples of C++'s layered evolution. Each mechanism was the best answer available in its decade. Understanding all of them isn't just academic — real codebases span multiple decades, and you'll read code written with every one of these approaches. But the good news: the latest tools are genuinely good. `if constexpr` and concepts make compile-time branching almost as natural as runtime branching.

Almost. There are still more angle brackets than anyone would like. But we're getting there.
