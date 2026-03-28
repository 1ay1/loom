---
title: "constexpr, consteval, and if constexpr — Making the Compiler Do Your Work"
date: 2025-09-26
slug: constexpr-consteval-and-compile-time
tags: cpp, constexpr, consteval, compile-time, metaprogramming
excerpt: The best code is code that runs before your program starts. constexpr makes the compiler your runtime.
---

The first post in this series said that `const` is the most important keyword in C++. Here's the sequel: `constexpr` might be the most important keyword in *modern* C++.

`const` says "this value won't change." `constexpr` says "this value *can be computed at compile time*." That's a profound difference. A `const` variable still occupies memory and is initialized at runtime. A `constexpr` variable exists only in the compiler's mind — by the time your program runs, the computation is already done and the result is baked into the binary.

## constexpr Variables

The simplest use is constant values:

```cpp
constexpr int MAX_EVENTS = 256;
constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
constexpr int KEEPALIVE_TIMEOUT_MS = 5000;
```

These are from Loom's HTTP server. They're not just "constants" in the way `const` provides — they're compile-time constants that the compiler can use in template arguments, array sizes, and static_assert checks. They never occupy runtime memory. They're folded directly into the instruction stream.

## constexpr Functions

A `constexpr` function can be evaluated at compile time *or* at runtime. The compiler chooses based on context:

```cpp
static constexpr std::string_view mime_type(std::string_view path) noexcept {
    auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return "application/octet-stream";
    auto ext = path.substr(dot);

    if (ext == ".png")  return "image/png";
    if (ext == ".css")  return "text/css";
    // ...
    return "application/octet-stream";
}
```

If you call `mime_type(".png")` in a constexpr context (like initializing a constexpr variable), the compiler evaluates it at compile time. If you call it with a runtime value, it runs at runtime like a normal function. Same code, two execution contexts.

This dual nature is powerful. You write the logic once, and the compiler optimizes it maximally for each call site. Constant arguments get folded away. Runtime arguments get normal code generation.

## consteval: Compile-Time Only

Sometimes you want to guarantee that a function runs at compile time — never at runtime. That's `consteval`:

```cpp
template<Lit P>
struct Traits {
    static consteval bool is_static() noexcept {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return false;
        return true;
    }

    static consteval size_t prefix_len() noexcept {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return i;
        return P.size();
    }
};
```

This is from Loom's route system. `is_static()` scans a URL pattern for `:` characters to determine if the route has parameters. `prefix_len()` finds the byte offset of the first parameter.

Both functions are `consteval` — they *must* be evaluated at compile time. If you tried to call `Traits<P>::is_static()` with a runtime value, the compiler would refuse. This guarantees that the pattern analysis never runs at runtime. The result is a compile-time constant, usable in `if constexpr` branches and template arguments.

Why does this matter? Because the result feeds into the matching logic:

```cpp
static bool match(std::string_view path) noexcept {
    if constexpr (is_static()) {
        return path == P.sv();  // exact string comparison
    } else {
        constexpr std::string_view prefix{P.buf, prefix_len()};
        return path.size() > prefix.size() && path.starts_with(prefix);
    }
}
```

For a static route like `"/"`, the compiler generates a simple equality check. For a parameterized route like `"/post/:slug"`, it generates a prefix check. The `if constexpr` eliminates the dead branch — the generated code contains only the relevant path. The `consteval` functions ensure the branching decision is made at compile time.

## if constexpr: Compile-Time Branch Elimination

`if constexpr` is the most important control flow feature in template metaprogramming. It evaluates the condition at compile time and *discards* the false branch — not just "doesn't execute it" but *removes it from the generated code entirely*.

This matters because the discarded branch might not even be valid code for the given type:

```cpp
template<typename C>
Node operator()(const C& component, Args&&... children) const {
    Children ch;
    if constexpr (sizeof...(Args) > 0) {
        ch.reserve(sizeof...(Args));
        (detail::collect(ch, std::forward<Args>(children)), ...);
    }
    // ...
}
```

When `sizeof...(Args)` is 0, the reserve and collect code is eliminated entirely. This isn't just an optimization — the fold expression `(detail::collect(ch, ...), ...)` is *invalid* when the pack is empty. `if constexpr` makes it safe by discarding the branch before it's type-checked.

Loom's route dispatch uses `if constexpr` to conditionally extract parameters:

```cpp
bool try_dispatch(HttpRequest& req, HttpResponse& out) const {
    if (req.method != M) return false;
    if (!Traits<P>::match(req.path)) return false;

    if constexpr (!Traits<P>::is_static())
        req.params.emplace_back(Traits<P>::param(req.path));

    out = handler(req);
    return true;
}
```

For a static route, the `params.emplace_back` line doesn't exist in the generated code. For a parameterized route, it does. The same template source generates two different functions, each containing only the logic it needs.

The component override system uses `if constexpr` with `std::is_same_v` to dispatch by component type:

```cpp
template<typename C>
const RenderFn<C>& get() const {
         if constexpr (std::is_same_v<C, Header>)     return header;
    else if constexpr (std::is_same_v<C, Footer>)     return footer;
    else if constexpr (std::is_same_v<C, PostCard>)   return post_card;
    else if constexpr (std::is_same_v<C, Index>)      return index;
    // ... 30 more
}
```

This is a compile-time switch statement. When you call `overrides.get<Header>()`, the compiler evaluates the `is_same_v` conditions, finds that `C == Header` matches, and generates a function that directly returns the `header` field. All the other comparisons and branches are eliminated. The generated code is a single field access — exactly what you'd write by hand.

## static_assert: Compile-Time Assertions

`static_assert` checks a condition at compile time and fails compilation if it's false:

```cpp
static_assert(WatchPolicy<InotifyWatcher>);
```

This line in Loom verifies that `InotifyWatcher` satisfies the `WatchPolicy` concept at the point of declaration. If someone modifies `InotifyWatcher` and breaks the interface (removes the `poll()` method, changes its return type), the compilation fails with a clear error message — not a cryptic template error 500 lines deep.

You can use `static_assert` with any compile-time boolean:

```cpp
static_assert(sizeof(HttpMethod) == 1, "HttpMethod should fit in one byte");
static_assert(MAX_EVENTS > 0, "Must have at least one event slot");
```

## NTTPs with Class Types: Structural Types

C++20 allows class types as non-type template parameters, with one restriction: the type must be *structural* — all members public, no custom constructors that prevent aggregate initialization, no virtual functions.

The `Lit` struct is structural:

```cpp
template<size_t N>
struct Lit {
    char buf[N]{};

    constexpr Lit(const char (&s)[N]) noexcept {
        for (size_t i = 0; i < N; ++i) buf[i] = s[i];
    }

    constexpr std::string_view sv() const noexcept { return {buf, N - 1}; }
    constexpr size_t size() const noexcept { return N - 1; }
    constexpr char operator[](size_t i) const noexcept { return buf[i]; }
    constexpr bool operator==(const Lit&) const = default;
};
```

It has a public `char buf[N]` member and a constexpr constructor that copies a string literal into the buffer at compile time. This means you can use it as a template parameter:

```cpp
template<Lit P>
struct Route { /* ... */ };

// Usage:
get<"/post/:slug">(handler)
// The string "/post/:slug" is copied into a Lit<13> at compile time
// and embedded in the Route type as a template parameter
```

The string literal becomes part of the *type*. `Route<GET, "/", H>` and `Route<GET, "/about", H>` are different types because their `Lit` parameters have different content. The compiler can inspect the string at compile time, analyze it, and generate specialized matching code.

This is compile-time programming at its most powerful. A string literal in your source code becomes a compile-time value that templates can inspect character by character, generating optimized code based on the string's content. The route `"/post/:slug"` generates prefix-matching code. The route `"/"` generates exact-matching code. All decided at compile time.

## The Constexpr Pipeline

Here's how all these pieces fit together in Loom's route system:

1. **String literal** `"/post/:slug"` is passed to `Lit`'s constexpr constructor
2. **NTTP** `Lit<13>` becomes a template parameter of `Route`
3. **consteval** functions in `Traits` analyze the pattern at compile time
4. **if constexpr** generates different matching code based on the analysis
5. **Fold expression** in `Compiled::operator()` unrolls the dispatch loop
6. **static_assert** verifies the watcher satisfies the WatchPolicy concept

Each step runs at compile time. By the time your program starts, the route table is fully compiled — patterns analyzed, branches eliminated, dispatch chain generated. The runtime cost is a sequence of string comparisons and direct function calls. That's it.

This is the constexpr philosophy: do as much as possible before the program runs. Every computation moved from runtime to compile time is a computation that happens once (during compilation) instead of millions of times (during execution). The compiler is the fastest interpreter you'll ever have — use it.

Next: C++20 concepts, where we give names to the requirements that templates impose on their arguments.
