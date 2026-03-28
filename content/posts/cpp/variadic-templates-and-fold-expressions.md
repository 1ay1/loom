---
title: "Variadic Templates and Fold Expressions — The Compiler's For Loop"
date: 2025-09-19
slug: variadic-templates-and-fold-expressions
tags: cpp, templates, variadic, fold-expressions, parameter-packs
excerpt: A fold expression is a loop that runs at compile time, generating exactly the code you need and nothing more.
---

The previous post showed templates with one or two parameters. But what if you need a template that works with *any number* of parameters? A function that takes 3 arguments, or 7, or 20, each potentially a different type?

This is what variadic templates do. And fold expressions are how you operate on all those parameters at once. Together, they're the backbone of Loom's DSLs — the mechanism that lets `div(class_("container"), h1("Hello"), p_("World"))` accept any combination of attributes, elements, and text.

## Parameter Packs

A parameter pack is a template parameter that represents zero or more types:

```cpp
template<typename... Args>
void print_all(const Args&... args);
```

The `...` after `typename` says "this is a pack of types." The `...` after `args` says "this is a pack of values." You can't index into a pack like an array — you have to *expand* it.

The simplest expansion uses recursion (pre-C++17):

```cpp
// Base case: no arguments
void print_all() {}

// Recursive case: handle first, recurse on rest
template<typename First, typename... Rest>
void print_all(const First& first, const Rest&... rest) {
    std::cout << first << "\n";
    print_all(rest...);  // expand the remaining pack
}

print_all(42, "hello", 3.14);
// Generates: print_all(42, "hello", 3.14)
//          → print_all("hello", 3.14)
//          → print_all(3.14)
//          → print_all()
```

This works, but it's verbose. C++17 gave us something much better.

## Fold Expressions

A fold expression applies a binary operator to all elements of a parameter pack:

```cpp
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);  // right fold over +
}

sum(1, 2, 3, 4);  // 1 + (2 + (3 + 4)) = 10
```

The `(args + ...)` expands to `arg1 + (arg2 + (arg3 + arg4))`. The compiler generates exactly the additions needed — no loop, no recursion, no overhead.

There are four fold forms:

```cpp
(args + ...)      // right fold: a + (b + (c + d))
(... + args)      // left fold:  ((a + b) + c) + d
(args + ... + 0)  // right fold with init: a + (b + (c + (d + 0)))
(0 + ... + args)  // left fold with init:  (((0 + a) + b) + c) + d
```

But the most powerful fold uses the comma operator.

## The Comma Fold: Loom's Secret Weapon

The comma operator in C++ evaluates its left operand, discards the result, then evaluates its right operand. A comma fold calls a function for each element in the pack:

```cpp
template<typename... Args>
Node elem(const char* tag, Args&&... args) {
    Node n{Node::Element, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}
```

This is the heart of Loom's DOM DSL. The fold `(detail::add(n, std::forward<Args>(args)), ...)` expands to:

```cpp
detail::add(n, arg1), (detail::add(n, arg2), (detail::add(n, arg3)));
```

Which means: call `detail::add` for each argument. The `detail::add` function is overloaded by type:

```cpp
inline void add(Node& n, Attr a)               { n.attrs.push_back(std::move(a)); }
inline void add(Node& n, Node c)               { n.children.push_back(std::move(c)); }
inline void add(Node& n, const std::string& s) { n.children.push_back({Node::Text, {}, {}, {}, s}); }
inline void add(Node& n, const char* s)         { n.children.push_back({Node::Text, {}, {}, {}, s}); }
inline void add(Node& n, std::vector<Node> cs)  { for (auto& c : cs) n.children.push_back(std::move(c)); }
```

When you write `div(class_("container"), h1("Hello"), p_("World"))`, the compiler:

1. Sees `div` is `elem("div", class_("container"), h1("Hello"), p_("World"))`
2. Deduces the parameter pack as `{Attr, Node, Node}`
3. Generates three `detail::add` calls, each dispatching to the correct overload
4. The `Attr` goes to the attributes list, the two `Node`s go to children

No runtime type checking. No `if (arg is Attr)` branching. The compiler resolves the types at compile time and generates direct function calls. This is why the DOM DSL has zero overhead — the type dispatch is eliminated entirely.

## The CSS sheet() Function

The CSS DSL uses the same pattern:

```cpp
template<typename... Args>
Sheet sheet(Args&&... args) {
    Sheet s;
    (detail::flatten(s, std::forward<Args>(args)), ...);
    return s;
}
```

The `flatten` function is overloaded for each CSS construct:

```cpp
inline void flatten(Sheet& s, Rule&& r)       { s.rules.push_back(std::move(r)); }
inline void flatten(Sheet& s, RulePack&& p)    { for (auto& r : p.rules) s.rules.push_back(std::move(r)); }
inline void flatten(Sheet& s, Nest&& n)        { /* expand nested selectors */ }
inline void flatten(Sheet& s, MediaBlock&& m)  { /* compile @media block */ }
inline void flatten(Sheet& s, KeyframeBlock&& k) { /* compile @keyframes block */ }
```

So when you write a theme's stylesheet:

```cpp
.styles = sheet(
    ".post-content"_s | color(v::text),
    content_area().nest(
        "a"_s         | color(v::accent),
        "a"_s.hover() | text_decoration(underline)
    ),
    media(max_w(768_px),
        ".sidebar"_s | display(none)
    ),
    keyframes("fade-in",
        from() | opacity(0),
        to()   | opacity(1.0)
    )
)
```

The `sheet()` call receives four arguments of four different types: `Rule`, `Nest`, `MediaBlock`, `KeyframeBlock`. The comma fold calls `flatten` for each, and each overload knows how to compile its type into CSS. Rules get appended directly. Nests expand parent-child selectors. Media blocks compile to `@media(...){}` strings. Keyframes compile to `@keyframes...{}` strings.

The entire stylesheet is built by a single variadic function call with a fold expression. No builder pattern, no method chaining, no runtime type dispatch.

## std::tuple: Storing a Parameter Pack

Sometimes you need to store a parameter pack. You can't store a `...` directly, but you can store it in a `std::tuple`:

```cpp
template<typename... Rs>
struct Compiled {
    std::tuple<Rs...> routes;
    // ...
};
```

This is Loom's compiled router. Each route in the route table is a different type (because each route has a different pattern and handler). The tuple stores all of them:

```cpp
// This tuple might be:
// std::tuple<
//     Route<GET, "/", IndexHandler>,
//     Route<GET, "/post/:slug", PostHandler>,
//     Route<GET, "/tag/:slug", TagHandler>
// >
```

Each element is a different type, with different compile-time pattern analysis and different handler code.

## std::apply: Iterating Over a Tuple

To iterate over a tuple's elements, you use `std::apply` with a fold expression:

```cpp
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
```

This is the compiled router's dispatch function. `std::apply` unpacks the tuple into a parameter pack, and the comma fold calls `try_one` for each route. The compiler generates a linear chain of if-else checks — one for each route — with no loop, no virtual dispatch, and no hash table lookup.

If you have three routes, the generated code is equivalent to:

```cpp
if (route1.try_dispatch(req, result)) { found = true; }
if (!found && route2.try_dispatch(req, result)) { found = true; }
if (!found && route3.try_dispatch(req, result)) { found = true; }
if (!found) return fallback(req);
return result;
```

And each `try_dispatch` itself is inlined — the pattern match is a string comparison against a compile-time constant, the handler call is a direct function call. The entire router compiles to a handful of branch instructions.

## Perfect Forwarding

You've seen `std::forward<Args>(args)...` in several examples. This is *perfect forwarding* — preserving the value category (lvalue vs rvalue) of each argument:

```cpp
template<typename... Args>
Node elem(const char* tag, Args&&... args) {
    Node n{Node::Element, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}
```

The `Args&&...` is a forwarding reference (also called a universal reference). It accepts both lvalues and rvalues. `std::forward<Args>(args)` preserves the original value category when passing to `detail::add`.

Why does this matter? Because `detail::add` has overloads that take by value and by move:

```cpp
inline void add(Node& n, Node c)           { n.children.push_back(std::move(c)); }
inline void add(Node& n, std::string&& s)  { n.children.push_back({Node::Text, {}, {}, {}, std::move(s)}); }
```

If you pass an rvalue (a temporary), `forward` preserves it as an rvalue, enabling the move overload. If you pass an lvalue, `forward` preserves it as an lvalue, triggering a copy. This means `div(h1("Hello"))` moves the h1 node (it's a temporary), while `div(existing_node)` copies it (it's an lvalue that the caller still owns).

Perfect forwarding + fold expressions + overloaded dispatch = the DOM DSL. The compiler sees the types of all arguments at compile time, forwards each optimally, and generates a sequence of direct pushes to the Node's attrs and children vectors. No waste.

## The Component System's Collect Pattern

Loom's component system uses the same technique for building children lists:

```cpp
template<typename C, typename... Args>
Node operator()(const C& component, Args&&... children) const {
    Children ch;
    if constexpr (sizeof...(Args) > 0) {
        ch.reserve(sizeof...(Args));
        (detail::collect(ch, std::forward<Args>(children)), ...);
    }
    // ... dispatch to renderer
}
```

The `sizeof...(Args)` is a compile-time expression giving the number of arguments in the pack. If it's zero, the `if constexpr` branch is eliminated entirely — no vector allocation, no reserve call. If it's nonzero, the vector is pre-sized to exactly the right capacity.

This is the pattern: variadic templates + fold expressions + overloaded dispatch + perfect forwarding. It appears in the DOM DSL (`elem`), the CSS DSL (`sheet`), the router (`compile`), the component system (`operator()`), and the nesting system (`Sel::nest`). Once you recognize it, you see it everywhere.

The compiler treats a fold expression as an unrolled loop. It knows every type, every overload, every branch — and it generates exactly the code needed, nothing more. That's why these DSLs have zero overhead. They look like runtime builders, but they compile to direct, sequential, fully-inlined code.

Next: constexpr and consteval — making the compiler do even more of your work, at compile time.
