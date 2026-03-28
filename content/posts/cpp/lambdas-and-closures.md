---
title: "Lambdas — Functions as Values"
date: 2025-08-22
slug: lambdas-and-closures
tags: cpp, lambdas, functional, closures
excerpt: Lambdas gave C++ something it never had — the ability to define behavior right where you need it.
---

Before C++11, if you wanted to pass a function to an algorithm, you had three options: a function pointer, a functor class (a struct with `operator()`), or a macro. Function pointers can't capture local state. Functor classes require boilerplate. Macros are macros. None of them were good.

Lambdas changed everything. A lambda is a function defined inline, right where you need it, that can capture variables from its surrounding scope. It's the feature that turned C++ into a language where you actually *want* to use higher-order functions.

## Basic Syntax

A lambda looks like this:

```cpp
auto greet = [](const std::string& name) {
    return "Hello, " + name;
};

std::string msg = greet("world");  // "Hello, world"
```

The square brackets `[]` are the capture list (empty here — more on that shortly). The parentheses hold parameters. The body is just like a function body. The return type is deduced automatically.

You can specify the return type explicitly when the compiler can't deduce it:

```cpp
auto parse = [](const std::string& s) -> std::optional<int> {
    try { return std::stoi(s); }
    catch (...) { return std::nullopt; }
};
```

## Lambdas as Algorithm Predicates

The most common use of lambdas is with standard algorithms. Here's Loom's blog engine sorting posts:

```cpp
std::sort(result.begin(), result.end(),
    [](const auto& a, const auto& b) {
        if (a.published != b.published) return a.published > b.published;
        return a.modified_at > b.modified_at;
    });
```

The lambda defines the comparison function inline. You can read the sort and its criteria in one glance, without jumping to a separate function definition. This matters enormously for readability.

Loom's `related_posts` function uses a lambda to sort by tag overlap score:

```cpp
std::sort(scored.begin(), scored.end(),
    [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        if (a.second.published != b.second.published)
            return a.second.published > b.second.published;
        return a.second.modified_at > b.second.modified_at;
    });
```

Three-level tiebreaking, expressed as a single inline expression. Try doing that cleanly with a functor class.

## Captures: Closing Over State

A lambda becomes a *closure* when it captures variables from its enclosing scope. This is the feature that makes lambdas truly powerful:

```cpp
auto now = std::chrono::system_clock::now();

auto is_published = [&now](const Post& p) {
    return !p.draft && p.published <= now;
};

// Use it:
for (const auto& p : posts) {
    if (is_published(p))
        result.push_back(p);
}
```

The `[&now]` means "capture `now` by reference." The lambda can read `now` from the enclosing scope without taking a copy. This is critical for performance when the captured variable is large.

### Capture Modes

There are several ways to capture:

```cpp
[&]       // capture everything by reference
[=]       // capture everything by value (copy)
[&cache]  // capture only cache, by reference
[=, &out] // capture everything by value, except out by reference
```

**By reference** (`[&]`): The lambda sees the original variables. Fast, no copies. But dangerous if the lambda outlives the variables it captures.

**By value** (`[=]`): The lambda gets its own copies. Safe, but potentially expensive for large objects.

**Named captures** (`[&cache]`): The explicit form. You name exactly what you capture and how. This is what I recommend for all non-trivial lambdas, because it documents the lambda's dependencies.

Loom's inotify watcher uses a capturing lambda to classify file changes:

```cpp
watch_dir(content_dir_ + "/posts",
    [](const std::string& f) -> ChangeEvent {
        return PostsChanged{{f}};
    });
```

This one captures nothing — it's a pure function from filename to event type. But look at the hot reloader's poll loop:

```cpp
thread_ = std::thread([this] {
    ChangeSet pending;
    while (running_.load()) {
        auto changes = watcher_.poll();
        if (changes) {
            pending = pending | *changes;
            // ... rebuild ...
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
});
```

The `[this]` capture gives the lambda access to the HotReloader's member variables. The lambda runs on a separate thread, using `running_`, `watcher_`, `source_`, and `cache_` from the parent object. This is the bread and butter of C++ concurrency: spawn a thread with a lambda that captures the state it needs.

### Move Captures (C++14)

Sometimes you want to *move* a value into the lambda rather than copy or reference it:

```cpp
auto data = std::make_unique<std::vector<int>>(1000);

auto process = [data = std::move(data)]() {
    // data is now owned by the lambda
    // the original data is empty (moved-from)
    for (auto& x : *data) { /* ... */ }
};
```

The `data = std::move(data)` syntax creates a new capture variable named `data` and initializes it by moving from the outer `data`. This is essential for `unique_ptr`, which can't be copied — you have to move it.

You can also use this to rename captures or compute values:

```cpp
auto handler = [slug = request.path.substr(6)]() {
    // slug is a string_view computed at capture time
};
```

## Generic Lambdas

A generic lambda uses `auto` for its parameters, making it a template:

```cpp
auto try_one = [&](const auto& route) {
    if (!found && route.try_dispatch(req, result))
        found = true;
};
```

This is from Loom's compile-time router. The lambda works with any route type — `Route<HttpMethod::GET, "/", H1>`, `Route<HttpMethod::POST, "/api", H2>`, whatever. The compiler generates a separate version of the lambda for each type it's called with.

Loom's DOM DSL uses generic lambdas for iteration:

```cpp
template<typename Container, typename Fn>
Node each(const Container& items, Fn&& fn) {
    Node n{Node::Fragment};
    n.children.reserve(items.size());
    for (const auto& item : items)
        n.children.push_back(fn(item));
    return n;
}

// Usage:
each(posts, [](const auto& p) {
    return article(class_("post-card"),
        h2(a(href("/post/" + p.slug), p.title)));
});
```

The lambda takes `const auto&` — it works with any element type. The `each` function template accepts any callable `Fn`. The compiler monomorphizes everything: no virtual calls, no indirection, no overhead.

## std::function: Type-Erased Callables

Sometimes you need to store a lambda for later. `std::function` is a type-erased wrapper that can hold any callable:

```cpp
#include <functional>

using Dispatch = std::function<HttpResponse(HttpRequest&)>;

class HttpServer {
public:
    void set_dispatch(Dispatch fn);
private:
    Dispatch dispatch_;
};
```

Loom's HTTP server stores its dispatch function as a `std::function`. This is necessary because the actual dispatch function is a complex template type (the `Compiled` struct from the router), and the server can't name that type in its header.

`std::function` has a cost: it uses type erasure, which typically means a heap allocation and a virtual call. For hot paths, prefer templates. For configuration and callbacks that are set once and called many times, `std::function` is fine.

The component system uses `std::function` for theme overrides:

```cpp
template<typename C>
using RenderFn = std::function<Node(const C&, const Ctx&, Children)>;

struct ComponentOverrides {
    RenderFn<Header> header{};
    RenderFn<Footer> footer{};
    RenderFn<PostCard> post_card{};
    // ... 30 more slots
};
```

Each slot is either empty (use the default renderer) or holds a lambda that replaces the component's HTML. This is the "WordPress-style structural override" pattern: themes don't just change colors — they can replace the entire HTML structure of any component.

## Lambdas as Fold Operations

One pattern that appears constantly in Loom is using lambdas with fold expressions. The DOM's `elem` function uses this to sort arguments:

```cpp
template<typename... Args>
Node elem(const char* tag, Args&&... args) {
    Node n{Node::Element, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}
```

The `(detail::add(n, ...), ...)` is a fold expression (we'll cover those in detail later). Each argument gets dispatched through `detail::add`, which overloads on type — `Attr` goes to the attrs list, `Node` goes to children, `const char*` becomes a text node. The lambda-like dispatching happens at compile time based on types.

## When to Use What

- **Inline lambda**: When the function is short and used once. Sort predicates, filter conditions, callbacks.
- **Named lambda (auto variable)**: When you use it more than once or it's complex enough to deserve a name.
- **std::function**: When you need to store a callable whose type you can't name. Callbacks, plugin slots, configuration.
- **Function template**: When you want the compiler to monomorphize and inline. Hot paths, generic algorithms.

The key insight is that a lambda is not just a convenient syntax for writing functions. It's a way to create values that *are* behavior. You can store them, pass them, compose them. Once you internalize this — that functions are values — C++ becomes a very different language than the one people fear.

Next: ownership and move semantics. The single most important concept for writing correct, performant C++.
