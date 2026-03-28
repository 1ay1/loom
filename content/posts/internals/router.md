---
title: Compile-Time Routing — Zero-Overhead Dispatch with C++20 NTTPs
date: 2026-03-28
slug: router
tags: internals, architecture, cpp
excerpt: Loom's route table is a compile-time expression. Patterns are non-type template parameters, dispatch is a fold expression, and the compiler generates an optimal branch chain. No trie, no hash map, no heap.
---

URL routing sounds simple until it isn't. You need literal matches (`/tags`), parameter captures (`/post/:slug`), and a sane fallback for everything else. Most frameworks solve this with a runtime trie, a hash map, or — worst case — a linear regex scan. Loom solves it at compile time.

Every route pattern is a non-type template parameter. The compiler sees the entire route table as a single expression and generates an optimal if-else chain. No trie nodes, no hash maps, no heap allocations, no virtual dispatch. The dispatch logic lives in `.text`, not in a data structure.

## The DSL

```cpp
using namespace loom::route;

auto dispatch = compile(
    fallback(fallback_handler),
    // Static routes — exact match
    get<"/">(cached),
    get<"/tags">(cached),
    get<"/archives">(cached),
    get<"/series">(cached),
    get<"/sitemap.xml">(sitemap_handler),
    get<"/robots.txt">(robots_handler),
    get<"/feed.xml">(rss_handler),
    // Parameterized routes — prefix match
    get<"/post/:slug">(cached),
    get<"/tag/:slug">(cached),
    get<"/series/:slug">(cached)
);
```

`dispatch` is a callable. The server calls `dispatch(request)` and gets back an `HttpResponse`. The type of `dispatch` is a template instantiation that encodes every route — the compiler can see through it completely.

## Compile-Time String Literals

C++20 allows class types as non-type template parameters, as long as they're structural types (public members, no mutable state). `Lit` is a fixed-size char array that captures a string literal at compile time:

```cpp
template<size_t N>
struct Lit {
    char buf[N]{};

    constexpr Lit(const char (&s)[N]) noexcept
    { for (size_t i = 0; i < N; ++i) buf[i] = s[i]; }

    constexpr std::string_view sv() const noexcept { return {buf, N - 1}; }
    constexpr size_t size() const noexcept { return N - 1; }
};
```

When you write `get<"/post/:slug">`, the compiler deduces `Lit<13>` and embeds the string `"/post/:slug"` into the type. Every operation on this string — finding the `:`, measuring the prefix, comparing segments — can happen at compile time.

## Compile-Time Pattern Analysis

`Traits<P>` analyses a pattern at compile time using `consteval` functions:

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

    static bool match(std::string_view path) noexcept {
        if constexpr (is_static()) {
            return path == P.sv();
        } else {
            constexpr std::string_view prefix{P.buf, prefix_len()};
            return path.size() > prefix.size() && path.starts_with(prefix);
        }
    }
};
```

`consteval` means these functions *must* run at compile time — not "can", *must*. The compiler evaluates `is_static()` and `prefix_len()` during template instantiation. The `match()` function uses `if constexpr` to select the right code path based on the compile-time analysis, and the dead branch is discarded entirely.

For `"/tags"`: `is_static()` is true, `match()` compiles to `path == "/tags"`.

For `"/post/:slug"`: `is_static()` is false, `prefix_len()` is 6, `match()` compiles to `path.size() > 6 && path.starts_with("/post/")`. The prefix string `"/post/"` is a compile-time constant in `.rodata`.

## Route Types

A `Route` is a `(Method × Pattern × Handler)` triple encoded at the type level:

```cpp
template<HttpMethod M, Lit P, typename H>
struct Route {
    H handler;

    bool try_dispatch(HttpRequest& req, HttpResponse& out) const {
        if (req.method != M) return false;
        if (!Traits<P>::match(req.path)) return false;

        if constexpr (!Traits<P>::is_static())
            req.params.emplace_back(Traits<P>::param(req.path));

        out = handler(req);
        return true;
    }
};
```

Parameter extraction is also conditional on `if constexpr`. For static routes, `req.params` is never touched — the compiler doesn't even emit the code. For parameterized routes, `param()` returns `path.substr(prefix_len())` — a `string_view` slice, no allocation.

## Fold Expression Dispatch

`compile()` returns a `Compiled` struct that holds all routes in a `std::tuple` and dispatches with a fold expression:

```cpp
template<typename FB, typename... Rs>
struct Compiled {
    std::tuple<Rs...> routes;
    FB fb;

    HttpResponse operator()(HttpRequest& req) const {
        HttpResponse result;
        bool found = false;

        auto try_one = [&](const auto& route) {
            if (!found && route.try_dispatch(req, result))
                found = true;
        };

        std::apply([&](const auto&... route) {
            (try_one(route), ...);
        }, routes);

        return found ? result : fb.handler(req);
    }
};
```

The fold expression `(try_one(route), ...)` expands at compile time into:

```cpp
try_one(route_0);  // get<"/">
try_one(route_1);  // get<"/tags">
try_one(route_2);  // get<"/archives">
// ...
try_one(route_9);  // get<"/series/:slug">
```

Each `try_one` call is a separate function body with its own compile-time-specialized `match()`. The compiler inlines everything and generates an optimal branch chain — no function pointers, no vtable, no indirection.

Because `found` short-circuits, routes are tried in definition order. Static routes (exact match) come first, then parameterized routes (prefix match). This means `/series` (exact) is checked before `/series/:slug` (prefix), so they don't conflict.

## The Builder Syntax

The `get<"/path">` syntax works through a two-step mechanism:

```cpp
template<Lit P>
struct get_t {
    template<typename H>
    constexpr auto operator()(H h) const {
        return Route<HttpMethod::GET, P, H>{std::move(h)};
    }
};

template<Lit P>
inline constexpr get_t<P> get{};
```

`get<"/post/:slug">` is a `constexpr` variable template — a zero-size object with `operator()`. Calling `get<"/post/:slug">(handler)` constructs a `Route<GET, "/post/:slug", Handler>`. The pattern is baked into the type; the handler is stored by value.

## What the Compiler Sees

For a request to `/post/hello-world`, the generated code (at -O2) is roughly:

```
if path == "/"            → call handler_0
if path == "/tags"        → call handler_1
if path == "/archives"    → call handler_2
if path == "/series"      → call handler_3
if path == "/sitemap.xml" → call handler_4
if path == "/robots.txt"  → call handler_5
if path == "/feed.xml"    → call handler_6
if path.starts_with("/post/")   && len > 6  → call handler_7
if path.starts_with("/tag/")    && len > 5  → call handler_8
if path.starts_with("/series/") && len > 8  → call handler_9
→ call fallback
```

No heap. No hash. No indirection. Just comparisons against constants in `.rodata` and direct function calls. The branch predictor handles the rest.

## Why Not a Runtime Trie

The previous version of Loom used a hand-built trie: `std::unordered_map<std::string, std::unique_ptr<TrieNode>>` at each level, with parameter children as special nodes. It worked, but:

1. **Heap allocations at startup** — `make_unique<TrieNode>` for every segment of every route.
2. **Hash map lookup per segment** — runtime hash computation, cache-line misses chasing pointers.
3. **`std::istringstream` for path splitting** — heap-allocated stream just to tokenize on `/`.
4. **`std::function` for handlers** — type-erased callable with a potential heap allocation per route.

The compile-time router eliminates all of these. The "trie" exists only in the compiler's template instantiation — it's flattened into straight-line code in the binary.

For 10 routes, the performance difference is small. But the design difference is fundamental: the route table is verified at compile time, dispatch is inlined, and there's zero runtime overhead for the routing layer itself.
