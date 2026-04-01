---
title: "Two Languages in One — The Compile-Time Language Inside C++"
date: 2026-02-01
slug: two-languages-in-one
tags: compile-time-cpp, metaprogramming, templates, constexpr
excerpt: C++ isn't one language. It's two. One runs on your machine. The other runs inside the compiler. This series teaches the second one.
---

Most people learn C++ as one language. They learn variables, functions, loops, classes, pointers, inheritance. They learn the standard library. Eventually they encounter templates and constexpr and think of them as "advanced features" — ornamental additions to the language they already know.

This framing is wrong. It obscures what is actually happening.

C++ is not one language. It is two. One runs at runtime on your hardware. The other runs at compile time inside the compiler itself. They coexist in the same source files, share some syntax, and interact at well-defined boundaries — but they are fundamentally different languages with different rules, different values, and different computational models.

This series teaches the second language. Not as a collection of tricks. Not as "template metaprogramming tips." As a complete, coherent programming language that happens to execute during compilation.

## The Runtime Language

The runtime language is the one everyone learns first. Its values are the familiar ones: integers, floats, strings, objects, pointers. Its operations are what you'd expect: arithmetic, function calls, memory allocation, I/O. Control flow is `if`, `for`, `while`, `switch`. Data structures are arrays, vectors, maps, linked lists — things that live in memory at runtime.

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

This is straightforward. `factorial` takes a runtime value, does runtime computation, returns a runtime value. The CPU executes it instruction by instruction. Nothing surprising.

## The Compile-Time Language

The compile-time language looks different. Its values are not ints and strings — they are **types** and **compile-time constants**. Its "functions" are templates, type traits, and constexpr functions. Its control flow is template specialization, SFINAE, `if constexpr`, and requires-clauses. Its data structures are type lists, value packs, and recursive template instantiations.

Here is the same factorial, written in the compile-time language — first the old way, using template recursion:

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
// The binary contains only the final number: 479001600.
```

And here is the modern version, using `constexpr`:

```cpp
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

constexpr int x = factorial(12);  // computed at compile time
```

The `constexpr` version looks almost identical to the runtime version. That is deliberate — C++11 through C++23 have been steadily making the compile-time language look more like the runtime language. But do not be fooled by the surface similarity. What is happening underneath is categorically different. No CPU executes that loop. The compiler itself evaluates it, during compilation, and embeds the result directly into the binary.

## Different Values, Different "Functions"

This is the key conceptual shift. In the runtime language:

- **Values** are ints, doubles, strings, objects — data that lives in memory.
- **Functions** take values and return values: `int factorial(int n)`.

In the compile-time language:

- **Values** are **types** (`int`, `std::string`, `std::vector<double>`) and **compile-time constants** (`42`, `true`, `sizeof(int)`).
- **Functions** take types and return types.

That second point is worth sitting with. A type trait like `std::remove_const` is a *function* in the compile-time language. It takes a type as input and returns a type as output:

```cpp
// Runtime function: takes a value, returns a value
int square(int x) { return x * x; }

// Compile-time "function": takes a type, returns a type
using T = std::remove_const_t<const int>;  // T = int
```

`std::conditional` is an if-statement in the compile-time language. It takes a boolean and two types, and returns one of them:

```cpp
// Runtime conditional
int x = (condition) ? a : b;

// Compile-time conditional
using T = std::conditional_t<sizeof(int) == 4, int32_t, int64_t>;
```

Template specialization is pattern matching. `constexpr if` is branching. Parameter packs are lists. Fold expressions are reductions over those lists. Every construct you know from runtime programming has a compile-time analog.

## Turing-Completeness

The compile-time language is Turing-complete. This was not a design goal. It was an accident — discovered after the fact. But it means you can compute *anything* at compile time that you can compute at runtime: loops, conditionals, recursion, data structures, algorithms.

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

You can implement compile-time sorting. Compile-time string processing. Compile-time regular expression compilation. Compile-time state machines. If the compiler can finish before the heat death of the universe, it will compute the answer.

The old-style template metaprogramming (pre-C++11) was painful to write and painful to read. It looked nothing like normal code. Modern C++ — especially C++20 and C++23 — has been closing that gap aggressively. `constexpr` lets you write compile-time code that looks like runtime code. `consteval` forces it. Concepts give you readable constraints instead of SFINAE nightmares. But the underlying model is the same: there is a language that executes during compilation, and mastering it gives you capabilities that no runtime-only approach can match.

## Why This Matters

You might ask: why bother? Why not just compute everything at runtime?

**Zero-cost abstractions.** The compile-time language lets you build powerful abstractions that dissolve entirely during compilation. A type-safe unit system, a compile-time regex engine, a statically-checked state machine — these can provide rich safety guarantees and expressive APIs while generating the exact same machine code you'd write by hand. The abstraction exists only in the source code. The binary knows nothing of it.

**Errors before execution.** When computation happens at compile time, errors are caught at compile time. A type mismatch, an invalid state transition, a violated precondition — these become compilation failures instead of runtime crashes. You find bugs before your code ever runs. Before it ships. Before it reaches a user.

**Optimal code generation.** When the compiler knows values at compile time, it can generate better code. Loop unrolling, constant propagation, dead code elimination — all of these benefit from compile-time knowledge. A `constexpr` lookup table is embedded directly in the binary. A template-generated dispatch table has no indirection. The compiler is not just checking your code; it is *specializing* it.

**Embedded DSLs.** The compile-time language lets you embed domain-specific languages inside C++. HTML generation, SQL queries, routing tables, serialization formats — you can build type-safe, zero-overhead interfaces for all of these. The DSL is processed at compile time; only the efficient result survives into the binary.

This is what Loom does. Its [HTML component system](/post/dom-and-components), [CSS DSL](/post/theme-system), and [routing](/post/router) are all compile-time constructs. The expressiveness lives in the source. The binary is tight, specialized, fast.

## An Accidental Discovery

The compile-time language was not designed. It was discovered.

In 1994, Erwin Unruh presented a C++ program to the C++ standards committee. The program did not compile — and that was the point. The error messages it produced contained a list of prime numbers. The computation happened entirely within the compiler's template instantiation engine. Nobody had intended templates to be a computation system. They were meant for generic programming — writing `vector<int>` and `vector<string>` without duplicating code. But the template instantiation rules turned out to be Turing-complete, and Unruh demonstrated that by computing primes as a side effect of failed compilation.

This accident shaped the next three decades of C++. Template metaprogramming became a discipline. Libraries like Boost.MPL and Boost.Hana formalized compile-time computation. The standards committee, recognizing what had emerged, began making the compile-time language more deliberate: `constexpr` in C++11, relaxed `constexpr` in C++14, `if constexpr` in C++17, concepts and `consteval` in C++20, expanded `constexpr` containers in C++23.

The language that was discovered by accident is now being designed on purpose. But to use it well, you need to understand it as a language — not as a bag of features.

## What This Series Covers

This series treats compile-time C++ as a coherent programming language and teaches it systematically. Here is what is coming:

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

This series assumes you are comfortable with runtime C++: functions, classes, the standard library, basic templates. If you have not read the [C++ series](/series/cpp), posts on [templates](/post/templates-and-generic-programming), [constexpr](/post/constexpr-consteval-and-compile-time), and [concepts](/post/concepts-and-constraints) provide the foundation.

You do not need prior experience with template metaprogramming. You do not need to have read Alexandrescu or the Boost.MPL documentation. You need to be willing to see C++ differently — as two languages sharing one syntax, one compiled into your binary, the other dissolved into it.

Let's begin.
