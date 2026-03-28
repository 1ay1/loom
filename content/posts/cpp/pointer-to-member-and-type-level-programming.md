---
title: "Pointer-to-Member, Tag Types, and Type-Level Programming"
date: 2025-11-07
slug: pointer-to-member-and-type-level-programming
tags: cpp, metaprogramming, type-level, advanced
excerpt: When your types carry enough information, the code writes itself. This is the endgame of C++ generics.
---

We've traveled a long road: from values and types, through containers and lambdas, ownership and templates, concepts and variants, atomics and system programming. This final post takes us to the frontier — where C++ types aren't just describing data, they're doing computation. Where the compiler generates code by reasoning about your types at compile time.

This isn't theoretical. Loom uses every technique here in production. The theme system, the component overrides, the route dispatch — all of them operate at the type level.

## Phantom Types and Tag Types

We saw tag types in the first post:

```cpp
struct SlugTag {};
struct TitleTag {};
struct PostIdTag {};

using Slug = StrongType<std::string, SlugTag>;
using Title = StrongType<std::string, TitleTag>;
using PostId = StrongType<std::string, PostIdTag>;
```

The tag structs — `SlugTag`, `TitleTag`, `PostIdTag` — carry no data. They're empty. They exist purely as type-level markers. The `StrongType` template uses them to generate distinct types that share the same runtime representation.

At runtime, `Slug` and `Title` are both `std::string` wrappers with identical memory layouts. The tags are erased — they occupy zero bytes. But at compile time, they're different types, and the compiler rejects mixing them.

This is a phantom type — a type parameter that affects the type system but not the runtime. It's the simplest form of type-level programming: encoding information in types that the compiler uses for checking but that disappears in the generated code.

Loom's theme system uses the same pattern for color tokens:

```cpp
namespace loom::theme {
    struct Bg     { static constexpr auto var = "--bg"; };
    struct Text   { static constexpr auto var = "--text"; };
    struct Muted  { static constexpr auto var = "--muted"; };
    struct Border { static constexpr auto var = "--border"; };
    struct Accent { static constexpr auto var = "--accent"; };
}
```

Each token is a type that carries a CSS custom property name as a compile-time constant. You can write code that is generic over token types, and each instantiation knows its own CSS variable name.

## Pointer-to-Member: Naming a Field as a Value

Here's a C++ feature that few programmers know about: you can take the address of a class member and store it as a value:

```cpp
struct Palette {
    Color bg;
    Color text;
    Color muted;
    Color border;
    Color accent;
};

// A pointer-to-member: "the bg field of some Palette"
Color Palette::* field = &Palette::bg;

// Use it to access the field on any Palette instance:
Palette p = { /* ... */ };
Color c = p.*field;  // reads p.bg
```

The type `Color Palette::*` means "a pointer to a Color member of Palette." It's not bound to any particular `Palette` instance — it names the *field itself*. You can store it, pass it to functions, and use it to access that field on any Palette.

This becomes powerful when combined with templates. Imagine a theme system that binds CSS tokens to palette fields:

```cpp
template<typename Token>
struct ColorBinding {
    Color Palette::* field;  // which palette field this token uses
};
```

A `ColorBinding<Bg>` says: "the background token maps to this palette field." A `ColorBinding<Accent>` says: "the accent token maps to this palette field." The token type carries the CSS variable name; the pointer-to-member carries the palette field. Together, they have all the information needed to generate a CSS variable declaration.

You could then write a function that generates CSS from a binding:

```cpp
template<typename Token>
std::string make_css_var(const ColorBinding<Token>& binding, const Palette& palette) {
    return std::string(Token::var) + ": " + (palette.*binding.field).value + ";";
}
```

For `ColorBinding<Accent>{&Palette::accent}` with a palette where accent is `"#4fc1ff"`, this produces `--accent: #4fc1ff;`. The token type provides the variable name. The pointer-to-member provides the value. No switches, no string maps, no runtime dispatch.

## Type-Level Lists via tuple

In the variadic templates post, we saw `std::tuple` as a way to store heterogeneous packs. Tuples can also serve as type-level lists — compile-time sequences of types that you can iterate over with fold expressions.

Consider binding all five color tokens to their palette fields:

```cpp
using ColorBindings = std::tuple<
    ColorBinding<Bg>,
    ColorBinding<Text>,
    ColorBinding<Muted>,
    ColorBinding<Border>,
    ColorBinding<Accent>
>;
```

This tuple is a compile-time list of bindings. You can iterate over it:

```cpp
std::string generate_css_vars(const Palette& palette) {
    std::string css;
    ColorBindings bindings{
        {&Palette::bg}, {&Palette::text}, {&Palette::muted},
        {&Palette::border}, {&Palette::accent}
    };

    std::apply([&](const auto&... binding) {
        ((css += make_css_var(binding, palette) + "\n"), ...);
    }, bindings);

    return css;
}
```

The `std::apply` unpacks the tuple. The fold expression calls `make_css_var` for each binding. Each call is fully typed — the compiler knows the token type, the palette field, and the CSS variable name at compile time. The generated code is a sequence of string concatenations with no branching or dispatch.

## if constexpr + is_same_v: Compile-Time Dispatch

We saw this pattern in the concepts post — Loom's `ComponentOverrides::get<C>()` method:

```cpp
template<typename C>
const RenderFn<C>& get() const {
         if constexpr (std::is_same_v<C, Document>)    return document;
    else if constexpr (std::is_same_v<C, Head>)        return head;
    else if constexpr (std::is_same_v<C, Body>)        return body;
    else if constexpr (std::is_same_v<C, Header>)      return header;
    else if constexpr (std::is_same_v<C, Footer>)      return footer;
    // ... 25 more
}
```

This is a type-level switch statement. The template parameter `C` is a type, and the function returns different fields based on which type it is. Each branch is evaluated at compile time — the generated function for `get<Header>()` is a single field access, with all other branches eliminated.

This pattern connects the component system's type parameters to the override struct's named fields. You call `overrides.get<Header>()`, and the compiler routes you to `overrides.header`. No hash map of strings, no enum-to-field mapping, no virtual dispatch.

The reason this works is that every component in Loom is a distinct type: `Header`, `Footer`, `PostCard`, `Index`, etc. The type carries identity. The `if constexpr` chain maps type identity to struct fields. The compiler does the routing at compile time.

## The Component Dispatch Pipeline

Let's trace the full pipeline when a theme renders a page:

```cpp
auto html = ctx(Header{});
```

1. `ctx.operator()` is called with `C = Header`
2. The template deduces `Comp = Header`
3. If `overrides` is non-null, `overrides->get<Header>()` is called
4. `get<Header>()` evaluates the `if constexpr` chain, returning the `header` slot
5. If the slot is non-empty (a theme has overridden the header), the lambda is called
6. Otherwise, `Header::render()` (the default renderer) is called

Steps 2-4 happen at compile time. The generated code is: check if the override function is non-null (a pointer comparison), call it or the default. Two instructions. No virtual dispatch, no type erasure overhead on the lookup side.

This is type-level programming in service of runtime flexibility. The types route the compile-time dispatch. The `std::function` slots provide runtime customization. The combination gives you the performance of static dispatch with the flexibility of dynamic overrides.

## Fold Over Type Bindings

The CSS DSL's `sheet()` function is a fold over heterogeneous types:

```cpp
template<typename... Args>
Sheet sheet(Args&&... args) {
    Sheet s;
    (detail::flatten(s, std::forward<Args>(args)), ...);
    return s;
}
```

When a theme defines its stylesheet:

```cpp
.styles = sheet(
    root()      | set("bg", hex("#fafaf8")),
    dark_root() | set("bg", hex("#1a1a2e")),
    ".post-content"_s | color(v::text),
    content_area().nest(
        "a"_s | color(v::accent)
    ),
    media(max_w(768_px),
        ".sidebar"_s | display(none)
    )
)
```

The `sheet()` call receives five arguments of four different types: `Rule`, `Rule`, `Rule`, `Nest`, `MediaBlock`. The fold calls `detail::flatten` for each, and overload resolution picks the right implementation. The compiler generates five sequential calls — two rule pushes, one rule push, one nested expansion, one media block compilation. No runtime type dispatch. The types drive the code generation.

## The Journey from Data to Metaprogramming

Looking back across these fifteen posts, the progression is clear:

**Posts 1-3**: Data lives in values with types. Structs group values. Enums name possibilities.

**Posts 4-6**: Behavior attaches to data via lambdas. Ownership determines lifetime. Views enable zero-copy access.

**Posts 7-9**: Templates parameterize code by type. Fold expressions iterate at compile time. constexpr moves computation to the compiler.

**Posts 10-12**: Concepts name type requirements. Variants model sum types. Atomics enable safe concurrency.

**Posts 13-14**: System interfaces (epoll, inotify) connect C++ to the OS kernel.

**Post 15** (this one): Types become a programming language. Tag types carry identity. Pointer-to-member names fields as values. Tuples are type-level lists. `if constexpr` is a type-level switch. The compiler generates specialized code from type-level descriptions.

Each layer builds on the previous. You can't write templates without understanding values and types. You can't write fold expressions without understanding lambdas and parameter packs. You can't write the component system without understanding constexpr, concepts, and type dispatch.

But the payoff is extraordinary. Loom is a full-featured blog engine — HTTP server, hot reloading, theme system, component overrides, HTML DSL, CSS DSL, compile-time routing — in a few thousand lines of C++. It compiles to a single binary with no dependencies. It starts in milliseconds. It serves cached pages with zero allocations. And the code reads like a domain-specific language, not like systems programming.

That's the promise of C++ type-level programming: you describe *what* you want, the compiler figures out *how* to do it efficiently, and the result is code that is simultaneously high-level and zero-cost. The types aren't just for safety — they're for performance. They're for expressiveness. They're the foundation of everything.
