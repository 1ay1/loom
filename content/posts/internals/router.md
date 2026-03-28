---
title: "Compile-Time Routing — Zero-Overhead Dispatch"
date: 2025-12-19
slug: router
tags: [loom-internals, c++20, routing, constexpr, templates]
excerpt: "How Loom encodes routes as non-type template parameters, analyzes patterns at compile time, and generates an optimal dispatch chain with zero runtime overhead."
---

Routing in a web framework is usually a runtime data structure. A trie, a hash map, a regex table — something that takes a URL path and finds the handler. Loom does not have any of those. Its routes are template parameters. The patterns are analyzed at compile time. The dispatch is a fold expression that the compiler turns into a chain of `if` comparisons at `-O2`.

This post builds on [post #9 on constexpr and consteval](/post/constexpr-consteval-and-compile-time), [post #8 on fold expressions](/post/variadic-templates-and-fold-expressions), and [post #7 on templates](/post/templates-and-generic-programming). The key C++20 feature is non-type template parameters (NTTPs) of class type, also discussed in [post #9](/post/constexpr-consteval-and-compile-time).

## The Problem

Loom has about 10 routes:

```
GET /                    → index page
GET /tags                → tag index
GET /archives            → archives page
GET /series              → series index
GET /sitemap.xml         → sitemap
GET /robots.txt          → robots.txt
GET /feed.xml            → RSS feed
GET /post/:slug          → individual post
GET /tag/:slug           → tag page
GET /series/:slug        → series page
```

Seven of these are exact matches (static routes). Three have a parameter (`:slug`). The fallback handles static files.

A trie would work. But a trie is a heap-allocated data structure that gets populated at startup and traversed at runtime. For 10 routes, that is massive overkill. The optimal code for this route table is a linear chain of `if` comparisons — and that is exactly what the compiler generates when the routes are template parameters.

## Lit: Compile-Time Strings

C++20 allows class types as non-type template parameters, provided they are "structural" — all members are public, non-mutable, and of structural types. `Lit<N>` is Loom's structural string:

```cpp
template<size_t N>
struct Lit {
    char buf[N]{};

    constexpr Lit(const char (&s)[N]) noexcept
    { for (size_t i = 0; i < N; ++i) buf[i] = s[i]; }

    constexpr std::string_view sv() const noexcept { return {buf, N - 1}; }
    constexpr size_t size()         const noexcept { return N - 1; }
    constexpr char operator[](size_t i) const noexcept { return buf[i]; }
    constexpr bool operator==(const Lit&) const = default;
};

template<size_t N> Lit(const char (&)[N]) -> Lit<N>;
```

The deduction guide lets you write `Lit("/post/:slug")` and the compiler deduces `N` from the string literal. The constructor copies the literal into the internal buffer at compile time. `N` includes the null terminator, so `sv()` returns `N - 1` characters.

The key property: `Lit` is a structural type. It can be a template parameter. This means `Route<Method, Lit{"/post/:slug"}, Handler>` is a valid type — the route pattern is part of the type itself.

## Traits: Compile-Time Pattern Analysis

Given a pattern, we want to know two things at compile time: is it static (no parameters)? and if parameterized, how long is the literal prefix?

```cpp
template<Lit P>
struct Traits {
    static consteval bool is_static() noexcept
    {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return false;
        return true;
    }

    static consteval size_t prefix_len() noexcept
    {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return i;
        return P.size();
    }
};
```

Both functions are `consteval` — they must execute at compile time. For `"/post/:slug"`, `is_static()` returns `false` and `prefix_len()` returns 6 (the length of `"/post/"`). For `"/"`, `is_static()` returns `true` and `prefix_len()` returns 1.

The runtime `match()` and `param()` methods use these compile-time results:

```cpp
static bool match(std::string_view path) noexcept
{
    if constexpr (is_static())
    {
        return path == P.sv();
    }
    else
    {
        constexpr std::string_view prefix{P.buf, prefix_len()};
        return path.size() > prefix.size() && path.starts_with(prefix);
    }
}

static std::string_view param(std::string_view path) noexcept
{
    return path.substr(prefix_len());
}
```

The `if constexpr` from [post #7](/post/templates-and-generic-programming) selects the match strategy at compile time. Static routes do an exact string comparison. Parameterized routes check that the path starts with the prefix and is longer than it (so there is actually a parameter value).

The `prefix` is a `constexpr` local — the compiler computes it at compile time from the pattern. At runtime, `starts_with()` is a `memcmp` of the known prefix length. The parameter extraction is a single `substr` from the known offset.

## Route: The Type-Level Triple

A route is a method, a pattern, and a handler:

```cpp
template<HttpMethod M, Lit P, typename H>
struct Route {
    H handler;

    constexpr explicit Route(H h) : handler(std::move(h)) {}

    bool try_dispatch(HttpRequest& req, HttpResponse& out) const
    {
        if (req.method != M) return false;
        if (!Traits<P>::match(req.path)) return false;

        if constexpr (!Traits<P>::is_static())
            req.params.emplace_back(Traits<P>::param(req.path));

        out = handler(req);
        return true;
    }
};
```

`try_dispatch` checks the method, then the pattern. If both match, it extracts the parameter (if any) and calls the handler. The `if constexpr` ensures that parameter extraction is only compiled for parameterized routes — static routes skip it entirely.

## The DSL: get<"/path">(handler)

Route creation uses variable templates and a two-step invocation:

```cpp
template<Lit P> struct get_t {
    template<typename H> constexpr auto operator()(H h) const
    { return Route<HttpMethod::GET, P, H>{std::move(h)}; }
};

template<Lit P> inline constexpr get_t<P> get{};
```

`get<"/post/:slug">` is a `constexpr` instance of `get_t<Lit{"/post/:slug"}>`. Calling it with a handler produces a `Route`. The pattern is encoded in the type at the template instantiation point.

This two-step pattern (variable template + call operator) is necessary because C++ does not allow partial specialization of function templates with NTTPs. The variable template captures the pattern, and the call operator captures the handler.

## compile(): Assembling the Dispatch

The `compile()` function takes a fallback and a set of routes, and returns a callable:

```cpp
template<typename H, typename... Routes>
constexpr auto compile(Fallback<H> fb, Routes... routes)
{
    return detail::Compiled<Fallback<H>, Routes...>(
        std::move(fb), std::move(routes)...);
}
```

The `Compiled` struct stores the routes in a tuple and the fallback separately:

```cpp
template<typename FB, typename... Rs>
struct Compiled {
    std::tuple<Rs...> routes;
    FB fb;

    HttpResponse operator()(HttpRequest& req) const
    {
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

The dispatch is a fold expression inside `std::apply`. `std::apply` unpacks the tuple into a parameter pack, and the fold expression calls `try_one` for each route. The `found` flag ensures that only the first match runs — subsequent routes are skipped with a cheap boolean check.

In practice, this is what the actual route table looks like:

```cpp
using namespace loom::route;

auto dispatch = compile(
    fallback(fb),
    get<"/">(cached),
    get<"/tags">(cached),
    get<"/archives">(cached),
    get<"/series">(cached),
    get<"/sitemap.xml">(sitemap),
    get<"/robots.txt">(robots),
    get<"/feed.xml">(rss),
    get<"/post/:slug">(cached),
    get<"/tag/:slug">(cached),
    get<"/series/:slug">(cached)
);
```

## What the Compiler Generates

At `-O2`, the compiler sees through all the abstraction. Each `try_dispatch` is inlined. The `Traits<P>::match()` calls become direct string comparisons against known constants. The fold expression becomes a linear if-else chain.

For the static routes, the generated code looks approximately like:

```
if (method == GET && path == "/")       { result = cached(req); return result; }
if (method == GET && path == "/tags")   { result = cached(req); return result; }
if (method == GET && path == "/archives") { ... }
// ...
```

For the parameterized routes:

```
if (method == GET && path.size() > 6 && memcmp(path.data(), "/post/", 6) == 0)
{
    req.params.emplace_back(path.substr(6));
    result = cached(req);
    return result;
}
```

The `Lit` buffer, the `Traits` analysis, the `Route` template, the tuple, the `std::apply`, the fold expression — all of it compiles away. What remains is the code you would write by hand if you were writing the dispatch manually. But you did not have to write it manually. You wrote a declarative route table, and the compiler did the rest.

## Why Not a Trie?

Loom previously had a runtime trie router (commit `4956528` removed it: "Remove old runtime trie router"). The trie was ~150 lines of code, allocated nodes on the heap, and traversed pointers at dispatch time. It was correct and fast enough, but it was also unnecessary.

For 10 routes, a linear scan is not a performance problem. Each comparison is a short string compare — the longest pattern is 14 characters. The CPU's branch predictor will learn the common paths within a few requests. And the compile-time approach has benefits beyond raw speed:

1. **Zero heap allocation.** The route table is part of the binary. No `new`, no `make_unique`, no startup cost.
2. **Full inlining.** The handler call is inlined directly after the match. No function pointer, no virtual dispatch.
3. **Dead code elimination.** If a route handler is unused (hypothetically), the compiler can eliminate it entirely.
4. **Type safety.** You cannot register a route with an invalid method or a malformed pattern — the pattern is checked at compile time.

The trie would start to win at maybe 100+ routes with shared prefixes. A blog does not have 100 routes. If Loom ever needs to route to hundreds of endpoints, the architecture has bigger problems than the dispatch strategy.

## The Fallback

The fallback handler catches everything that does not match a route:

```cpp
template<typename H>
struct Fallback {
    H handler;
    constexpr explicit Fallback(H h) : handler(std::move(h)) {}
};

template<typename H>
constexpr auto fallback(H h) { return Fallback<H>{std::move(h)}; }
```

In Loom, the fallback serves static files. For filesystem mode, it reads from disk. For git mode, it either redirects to GitHub's raw content URL or reads from the local git repository. The fallback always runs last — after all typed routes have been tried.

## The Full Picture

The routing system is about 200 lines of code. It handles Loom's entire URL space with zero runtime overhead. The key insight is that a blog's route table is static — it does not change between requests, and it is small enough that the optimal dispatch is a linear scan, not a tree traversal.

C++20's NTTPs make the ergonomics work. Without them, you would need something like `Route<GET, decltype(make_lit("/post/:slug"))>` with a separate constexpr variable. With NTTPs, you write `get<"/post/:slug">(handler)` and the pattern is carried as a template parameter — visible to the compiler at every point in the dispatch chain.

The routing system is a good example of Loom's design philosophy: use the type system at compile time so there is nothing left to do at runtime. Routes are types. Patterns are template parameters. Dispatch is a fold expression. And the generated code is the same code you would write by hand — just produced automatically from a declarative specification.
