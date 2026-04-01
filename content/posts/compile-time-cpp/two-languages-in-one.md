---
title: "Two Languages in One — The Compile-Time Language Inside C++"
date: 2026-02-01
slug: two-languages-in-one
tags: compile-time-cpp, metaprogramming, templates, constexpr
excerpt: C++ isn't one language. It's two. One runs on your machine. The other runs inside the compiler. This series teaches the second one.
---

Here's a secret that will change how you think about C++ forever:

C++ is not one programming language. It is *two* programming languages wearing a trench coat, pretending to be one language, sharing the same source files, and hoping you don't notice.

The first language is the one you already know. Variables, functions, loops, classes, pointers. You write code, the compiler turns it into machine instructions, the CPU runs those instructions. Normal stuff. We'll call this the **runtime language** because it runs at runtime. Creative, I know.

The second language is weirder. It runs *inside the compiler itself*, during compilation, before your program even exists. Its values aren't ints and strings — they're *types*. Its functions aren't regular functions — they're *templates*. Its loops don't use `for` — they use *recursive template instantiation*. And when it's done computing, it doesn't produce output on your screen. It produces... more C++ code. Code that then gets compiled into your binary.

This is the **compile-time language**. And this series is going to teach it to you. Not as a collection of arcane tricks. Not as "advanced template metaprogramming tips for experts." As a real, honest-to-god programming language that happens to execute during compilation.

## Wait, Two Languages? Seriously?

Seriously. Let me show you.

Here's a function in the runtime language. Nothing fancy:

```cpp
int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

int main() {
    int x = factorial(12);  // computed at runtime
    std::cout << x << "\n";
}
```

You've seen a million functions like this. `factorial` takes a number, loops, multiplies, returns the result. The CPU does the work when you run the program. Nothing to see here.

Now here's the *same computation*, written in the compile-time language:

```cpp
template<int N>
struct Factorial {
    static constexpr int value = N * Factorial<N - 1>::value;
};

template<>
struct Factorial<0> {
    static constexpr int value = 1;
};

// Factorial<12>::value is computed entirely during compilation.
// The binary just contains the number 479001600. No multiplication happens at runtime.
```

Read that again. `Factorial<12>::value` doesn't compute anything when your program runs. The compiler itself does the multiplication. By the time the binary is produced, `Factorial<12>::value` is just the number 479001600, sitting there in your executable, as if you'd typed it by hand. The loop happened inside the compiler. Your CPU never sees it.

And here's the modern version, which looks almost identical to the runtime version:

```cpp
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

constexpr int x = factorial(12);  // computed at compile time
```

Same loop. Same multiplication. Same result. But that one word — `constexpr` — changes *everything*. It tells the compiler: "evaluate this during compilation." No CPU involved. The compiler *is* the CPU. It runs the loop, does the multiplication, gets 479001600, and embeds that constant directly into your binary.

This is what I mean by two languages. The surface syntax looks similar (especially with `constexpr`), but what's happening underneath is completely different. In one case, the CPU executes instructions at runtime. In the other, the compiler evaluates expressions during compilation. Same answer. Different universe.

## Why Would Anyone Do This?

Fair question. Why go through the trouble of computing things during compilation when you could just... compute them at runtime like a normal person?

Four reasons, and they're all bangers.

**Zero-cost abstractions.** This is the big one. The compile-time language lets you build incredibly powerful abstractions — type-safe unit systems, compile-time regex engines, statically-checked state machines — that produce the *exact same machine code* you'd write by hand without any abstraction at all. The abstraction exists only in the source code, for humans to read. The binary knows nothing about it. It's been dissolved. Evaporated. The compiler ate it and produced raw, optimal machine code.

Imagine you build a type-safe physics library where you literally can't add meters to seconds — the compiler rejects it. At runtime, the code is just plain floating-point arithmetic. No wrapper objects. No virtual dispatch. No overhead. The type safety was a compile-time construct that vanished before the program was born.

**Errors before execution.** When computation happens at compile time, errors happen at compile time. A type mismatch? Compile error. An invalid state transition? Compile error. A format string with the wrong number of arguments? Compile error. You find bugs before your code runs. Before it ships. Before it reaches a user. Before it crashes in production at 3 AM on a Saturday.

Runtime errors are expensive. They require testing to find, debugging to fix, and apologizing to users when you miss them. Compile-time errors are free. The compiler is the world's most aggressive QA team, and it works for free, 24/7, on every build.

**Optimal code generation.** When the compiler knows values at compile time, it can generate much better code. A `constexpr` lookup table is embedded directly in the binary — no initialization function, no startup cost. A template-generated dispatch table has no indirection. Loop unrolling, constant propagation, dead code elimination — all of these benefit from compile-time knowledge. You're giving the compiler more information, and it rewards you with faster code.

**Embedded DSLs.** This is where it gets really fun. The compile-time language lets you embed *domain-specific languages* inside C++. Think about it: you can write a string literal like `"/users/{id}/posts/{post_id}"`, parse it during compilation, validate that the route parameters match your handler's signature, and generate a zero-overhead URL router. The string is a mini-program in a routing DSL. The compiler is the interpreter for that DSL. The output is optimized machine code.

This is what Loom does. Its [HTML component system](/post/dom-and-components), [CSS DSL](/post/theme-system), and [routing](/post/router) are all compile-time constructs. You write expressive, readable source code. The binary is tight, specialized, fast. The abstraction existed for humans. The machine code exists for CPUs. Nothing exists in between.

## The Accidental Discovery

Here's the wild part: nobody planned this.

The compile-time language was not designed. It was *discovered*.

In 1994, a man named Erwin Unruh showed up to a C++ standards committee meeting with a program. The program didn't compile. That was the point. The *error messages* the compiler produced contained a list of prime numbers.

Let that sink in. He wrote a program that computed prime numbers as a *side effect of failing to compile*. The computation happened entirely within the compiler's template instantiation engine. The template system — which was designed for boring things like writing `vector<int>` without duplicating code — turned out to be *accidentally Turing-complete*.

Nobody intended this. Templates were a mechanism for generic programming. But the template instantiation rules — how the compiler expands templates, matches specializations, performs substitution — happened to form a complete computational system. You could express any computation as a series of template instantiations. The compiler would "run" that computation as a side effect of trying to compile your code.

This accident shaped the next three decades of C++. Template metaprogramming became a discipline. Libraries like Boost.MPL and Boost.Hana formalized compile-time computation. The standards committee, recognizing what had emerged from the primordial ooze, started making the compile-time language more intentional: `constexpr` in C++11, relaxed `constexpr` in C++14, `if constexpr` in C++17, concepts and `consteval` in C++20, expanded constexpr containers in C++23, reflection in C++26.

The language that was born by accident is now being raised on purpose. But to use it well, you need to understand it as a language — not as a bag of disconnected features with confusing names.

## Different Values, Different Functions

Here's the key conceptual shift that makes everything else click. It's worth spending some time on because once you see it, the entire `<type_traits>` header stops being cryptic and becomes obvious.

In the runtime language:

- **Values** are ints, doubles, strings, objects — data that lives in memory
- **Functions** take values and return values: `int square(int x) { return x * x; }`

In the compile-time language:

- **Values** are **types** (`int`, `std::string`, `std::vector<double>`) and **compile-time constants** (`42`, `true`, `sizeof(int)`)
- **Functions** take types and return types

That second point is the one that breaks people's brains, so let me be very explicit. A type trait like `std::remove_const` is a *function*. It takes a type as input. It returns a type as output. Watch:

```cpp
// Runtime function: takes a value, returns a value
int square(int x) { return x * x; }

// Compile-time "function": takes a type, returns a type
using T = std::remove_const_t<const int>;  // T = int
```

`std::remove_const_t` is a function call. The input is `const int`. The output is `int`. It "removed the const." It's a function that operates on types the same way `square` operates on integers.

`std::conditional` is an if-statement in the compile-time language. It takes a boolean and two types, and returns one of them:

```cpp
// Runtime conditional
int x = (condition) ? a : b;

// Compile-time conditional
using T = std::conditional_t<sizeof(int) == 4, int32_t, int64_t>;
```

If `sizeof(int)` is 4 (which it usually is), `T` is `int32_t`. Otherwise, `T` is `int64_t`. The "if statement" ran during compilation. By the time the binary exists, `T` is just whichever concrete type was selected. The conditional doesn't exist at runtime.

Template specialization is pattern matching. `constexpr if` is branching. Parameter packs are lists. Fold expressions are reductions over those lists. Every construct you know from runtime programming has a compile-time analog. Different syntax. Same concepts.

## Turing-Completeness (A.K.A. The Source of All Power and Pain)

The compile-time language is Turing-complete. This means you can compute *anything* at compile time that you can compute at runtime. Loops, conditionals, recursion, data structures, algorithms. If the compiler can finish before the heat death of the universe (or more practically, before it hits its template instantiation depth limit), it will compute the answer.

You can implement a compile-time linked list:

```cpp
template<int... Values>
struct IntList {};

template<typename List, int V>
struct Append;

template<int... Values, int V>
struct Append<IntList<Values...>, V> {
    using type = IntList<Values..., V>;
};

// Append<IntList<1, 2, 3>, 4>::type is IntList<1, 2, 3, 4>
```

You can implement compile-time sorting. Compile-time string processing. Compile-time regular expression compilation. Compile-time state machines. People have implemented compile-time ray tracers (the error messages contained a rendered image, naturally). People have implemented compile-time Turing machines that simulate other Turing machines. It's turtles all the way down.

The old-style template metaprogramming (pre-C++11) was painful. Imagine writing `for` loops using only recursion, `if` statements using only template specialization, and variables using only `typedef`. Now imagine the error messages for all of that. They were bad. Really bad. "I wanted to add two numbers and got 300 lines of template instantiation backtrace" bad.

Modern C++ — especially C++20 and beyond — has been closing that gap aggressively. `constexpr` lets you write compile-time code that looks like normal code. `consteval` forces compile-time evaluation. Concepts give you readable constraints instead of SFINAE nightmares (don't worry if you don't know what SFINAE is — we'll get there, and then you'll wish you hadn't). But the underlying model is the same: there is a language that executes during compilation, and mastering it gives you capabilities that no runtime-only approach can match.

## What This Series Covers

This series treats compile-time C++ as a coherent programming language and teaches it systematically. Not "here's a template trick." Not "here's a weird thing you can do with SFINAE." A complete language, taught like a language.

Here is what's coming:

1. **Two Languages in One** — this post. The mental model.
2. **Types as Values** — how to think about types as the "data" of the compile-time language. Type aliases, type traits, type transformations.
3. **Template Functions** — templates as compile-time functions. Specialization as pattern matching. Overload resolution as dispatch.
4. **Compile-Time Control Flow** — `if constexpr`, tag dispatch, SFINAE, requires-clauses. Branching in the compile-time language.
5. **Compile-Time Data Structures** — type lists, value lists, tuples as heterogeneous containers. Operating on lists of types.
6. **constexpr Everything** — writing real algorithms in `constexpr`. What you can and cannot do. `consteval` and immediate functions.
7. **Concepts as Interfaces** — concepts as the type system of the compile-time language. Constraining what types your compile-time functions accept.
8. **Variadic Templates and Pack Expansion** — working with arbitrary numbers of compile-time arguments. Fold expressions as compile-time reductions.
9. **Template Template Parameters** — higher-order compile-time functions. Passing "functions" (templates) to other "functions" (templates).
10. **Compile-Time Strings and Parsing** — `constexpr` string processing. Building compile-time parsers. Fixed-string template parameters.
11. **Reflection and Code Generation** — what C++23/26 bring to compile-time introspection. Where the compile-time language is headed.
12. **Putting It All Together** — a real compile-time DSL, built step by step, using everything from the series.

Each post builds on the previous ones. By the end, the compile-time language will not feel like an advanced feature or a dark corner of C++. It will feel like what it is: a second language, running inside your compiler, that you now speak fluently.

## Prerequisites

This series assumes you're comfortable with runtime C++: functions, classes, the standard library, basic templates. If you've not read the [C++ series](/series/cpp), posts on [templates](/post/templates-and-generic-programming), [constexpr](/post/constexpr-consteval-and-compile-time), and [concepts](/post/concepts-and-constraints) provide the foundation.

You don't need prior experience with template metaprogramming. You don't need to have read Alexandrescu's *Modern C++ Design* (though if you have, you'll appreciate how far we've come). You don't need to understand Boost.MPL or any metaprogramming library.

You need to be willing to see C++ differently — as two languages sharing one syntax, one compiled into your binary, the other dissolved into it. One runs on your hardware. The other runs inside your compiler's head.

Let's teach you the second one.
