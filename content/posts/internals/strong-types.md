---
title: "Strong Types — Making Illegal States Unrepresentable"
date: 2025-11-14
slug: strong-types
tags: [loom-internals, c++20, type-safety, templates]
excerpt: "How Loom uses phantom type tags, conditional concepts, structural enums, and token types to turn entire categories of bugs into compile errors."
---

If you have been following the C++ series, you know that the type system is not just a tool for catching mistakes — it is a design tool. In [post #7 on templates](/post/templates-and-generic-programming) we saw how generic code lets you write things once. In [post #10 on concepts](/post/concepts-and-constraints) we saw how constraints make generic code self-documenting. And in [post #15 on pointer-to-member](/post/pointer-to-member-and-type-level-programming) we saw how the type system can encode domain relationships.

Loom leans hard on all three. The goal: make it impossible to pass a slug where a title is expected, impossible to use a hex color where a font stack belongs, and impossible to forget a theme variant. Not through documentation. Through the compiler.

## The StrongType Template

Here is the entire implementation. It lives in `include/loom/core/strong_type.hpp`:

```cpp
template<typename T, typename Tag>
class StrongType
{
public:
    explicit StrongType(T value) : value_(std::move(value)) {}

    T get() const { return value_; }

    bool empty() const requires requires(const T& v) { v.empty(); }
    { return value_.empty(); }

    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    bool operator!=(const StrongType& other) const { return value_ != other.value_; }
    bool operator<(const StrongType& other) const { return value_ < other.value_; }

private:
    T value_;
};
```

Twenty-six lines. No inheritance, no virtual methods, no CRTP. Just a wrapper with a phantom tag.

The `Tag` type parameter is never used at runtime. It exists purely to create distinct types. A `StrongType<std::string, SlugTag>` and a `StrongType<std::string, TitleTag>` both hold strings, but the compiler treats them as unrelated types. You cannot pass one where the other is expected, you cannot compare them, and you cannot accidentally assign one to the other.

This is the phantom type pattern. If you have used Haskell's `newtype` or Rust's tuple structs, it is the same idea.

### The Double Requires

The most interesting line is `empty()`:

```cpp
bool empty() const requires requires(const T& v) { v.empty(); }
{ return value_.empty(); }
```

This is a *requires requires* expression — an ad-hoc constraint checked at the call site. The outer `requires` is a constraint on the member function. The inner `requires` is an expression that tests whether `v.empty()` is valid. If `T` does not have an `empty()` method, this member function simply does not exist. No SFINAE, no `std::enable_if`, no specialization. Just a constraint.

When `T` is `std::string`, `.empty()` works. If you ever made a `StrongType<int, SomeTag>`, calling `.empty()` on it would be a compile error because the method would not be part of the class.

This is the technique from [post #10 on concepts](/post/concepts-and-constraints) — conditional method availability based on the wrapped type's interface.

## The Domain Types

With the template in place, Loom defines its domain vocabulary in `include/loom/core/types.hpp`:

```cpp
struct SlugTag {};
struct TitleTag {};
struct PostIdTag {};
struct TagTag {};
struct ContentTag {};
struct SeriesTag {};

using Slug    = StrongType<std::string, SlugTag>;
using Title   = StrongType<std::string, TitleTag>;
using PostId  = StrongType<std::string, PostIdTag>;
using Tag     = StrongType<std::string, TagTag>;
using Content = StrongType<std::string, ContentTag>;
using Series  = StrongType<std::string, SeriesTag>;
```

Six tag structs, six type aliases. Every one of these wraps `std::string`, but they are all distinct types.

Consider what this prevents. A function signature like:

```cpp
std::string render_post(const std::string& title, const std::string& slug,
                        const std::string& content);
```

This is an invitation to swap arguments. You have three `string` parameters of similar meaning. Nothing stops you from writing `render_post(slug, title, content)` or even `render_post(content, slug, title)`. The compiler is happy. Your blog is broken.

With strong types:

```cpp
dom::Node render_post(const Title& title, const Slug& slug,
                      const Content& content);
```

Now swapping arguments is a compile error. The explicit `get()` accessor makes the unwrapping intentional — every time you access the raw string, you are making a conscious decision to leave the typed world.

### The TagTag Problem

Yes, one of the tags is called `TagTag`. That is the tag for the `Tag` type. It looks silly, but it is the correct name. A blog tag is a `Tag`. The phantom type tag for that blog tag is `TagTag`. Naming is hard, and sometimes the right answer is the obvious one.

## Theme Strong Types: Color vs FontStack

The domain types above all wrap `std::string`. But strong types shine even brighter when the domain has structurally identical values with different semantics. Colors and font stacks are both strings:

```cpp
// include/loom/render/theme/types.hpp
struct Color
{
    std::string value;
};

struct FontStack
{
    std::string value;
};
```

These are not `StrongType` instances — they are simple structs with a single member. But the effect is the same. A `Color` is not a `FontStack`. You cannot accidentally put `"ui-monospace, Menlo, monospace"` where `"#1a1a1a"` belongs.

The `Palette` struct uses `Color` for all five color tokens:

```cpp
struct Palette
{
    Color bg;
    Color text;
    Color muted;
    Color border;
    Color accent;
};
```

And `ThemeDef` uses `FontStack` for font fields:

```cpp
struct ThemeDef
{
    Palette   light;
    Palette   dark;
    FontStack font;
    // ...
};
```

When you define a theme, the types guide you:

```cpp
inline const ThemeDef terminal = {
    .light = {{"#1a1a1a"}, {"#d8d8d8"}, {"#777777"}, {"#2e2e2e"}, {"#5fba7d"}},
    .dark  = {{"#1a1a1a"}, {"#d8d8d8"}, {"#777777"}, {"#2e2e2e"}, {"#5fba7d"}},
    .font  = {"ui-monospace,'SF Mono',SFMono-Regular,Menlo,Consolas,monospace"},
    // ...
};
```

The single-brace `{"#1a1a1a"}` initializes a `Color`. The compiler knows the difference between the `Palette` fields and the `FontStack` field. Try putting a hex color string where the `font` field goes — you will get a confusing but correct error about aggregate initialization.

## Structural Enums: Sum Types for UI

Not everything is a string wrapper. Many theme knobs are enumerations — a fixed set of mutually exclusive options. Loom uses `enum class` for these, defined in `include/loom/render/theme/components.hpp`:

```cpp
enum class Corners { Soft, Sharp, Round };
enum class TagStyle { Pill, Rect, Bordered, Outline, Plain };
enum class LinkStyle { Underline, Dotted, Dashed, None };
enum class CardHover { Lift, Border, Glow, None };
```

There are sixteen of these enums in total. Each one represents a design axis — a single dimension of variation that a theme can control.

These are sum types in the algebraic sense. A `Corners` value is exactly one of `Soft`, `Sharp`, or `Round`. Not a string that might be "soft" or "Soft" or "SOFT" or "round-ish". Not an integer that might be 0, 1, 2, or 47. One of three values, enforced at compile time.

The theme compiler (`include/loom/render/theme/compiler.hpp`) pattern-matches on these enums with switch statements:

```cpp
inline void emit_corners(std::string& css, Corners c)
{
    switch (c)
    {
        case Corners::Soft: break;  // default — no CSS needed
        case Corners::Sharp:
            css += ":root{--border-radius:0;--card-radius:0;--tag-radius:0;}";
            break;
        case Corners::Round:
            css += ":root{--border-radius:12px;--card-radius:16px;--tag-radius:999px;}";
            break;
    }
}
```

Each variant maps to a specific CSS output. `Soft` is the default, so it emits nothing. `Sharp` zeroes all radii. `Round` cranks them up. No string parsing, no validation, no "what if someone passes an invalid value" — the compiler has already eliminated that possibility.

This pattern repeats for all sixteen axes. The `compile()` function in `compiler.hpp` calls each emitter in sequence:

```cpp
inline std::string compile(const ThemeDef& t)
{
    std::string css;
    css += ":root{";
    detail::emit_palette(css, t.light, detail::ColorBindings{});
    css += Font::var; css += ':'; css += t.font.value; css += ';';
    // ...
    css += '}';

    detail::emit_corners(css, t.corners);
    detail::emit_density(css, t.density);
    detail::emit_tag_style(css, t.tag_style);
    detail::emit_link_style(css, t.link_style);
    // ... 12 more emitters
    return css;
}
```

## Token Types: Compile-Time CSS Variable Names

The last category of strong types in Loom is the most unusual. The color tokens in `include/loom/render/theme/tokens.hpp` are types that carry their CSS custom property name as a `static constexpr` member:

```cpp
struct Bg     { static constexpr auto var = "--bg"; };
struct Text   { static constexpr auto var = "--text"; };
struct Muted  { static constexpr auto var = "--muted"; };
struct Border { static constexpr auto var = "--border"; };
struct Accent { static constexpr auto var = "--accent"; };
struct Font     { static constexpr auto var = "--font"; };
struct FontSize { static constexpr auto var = "--font-size"; };
struct MaxWidth { static constexpr auto var = "--max-width"; };
```

These types exist solely to be template parameters. They are never instantiated. They have no methods, no data members (at the instance level). They are pure compile-time entities — type-level strings.

The `ColorBinding` template in the theme compiler uses them:

```cpp
template<typename Token, Color Palette::* Member>
struct ColorBinding
{
    static void emit(std::string& css, const Palette& p)
    {
        css += Token::var;
        css += ':';
        css += (p.*Member).value;
        css += ';';
    }
};
```

This is the pointer-to-member technique from [post #15](/post/pointer-to-member-and-type-level-programming). `Color Palette::* Member` is a pointer to a `Color` member of `Palette`. At compile time, we bind each token type to its corresponding palette member:

```cpp
using ColorBindings = std::tuple<
    ColorBinding<Bg,     &Palette::bg>,
    ColorBinding<Text,   &Palette::text>,
    ColorBinding<Muted,  &Palette::muted>,
    ColorBinding<Border, &Palette::border>,
    ColorBinding<Accent, &Palette::accent>
>;
```

And then a fold expression (from [post #8 on variadic templates](/post/variadic-templates-and-fold-expressions)) iterates over the tuple:

```cpp
template<typename... Bs>
void emit_palette(std::string& css, const Palette& p, std::tuple<Bs...>)
{
    (Bs::emit(css, p), ...);
}
```

The `std::tuple<Bs...>` parameter exists only for type deduction — the actual tuple value is never used. The fold expression `(Bs::emit(css, p), ...)` expands to five static function calls, one per binding. Each call emits exactly one CSS custom property.

This is a pattern where the token type, the CSS variable name, and the palette member are all connected at the type level. Adding a new color token means adding a struct, a palette member, and a binding entry. Forget any one of them and you get a compile error.

## Why Not Just Use Strings?

The alternative to all of this is `std::string` everywhere. It would be less code. It would compile faster. And it would be a minefield.

In a blog engine, the domain objects flow through many layers: parsing, validation, storage, rendering, serialization. A slug appears in frontmatter parsing, URL construction, route matching, template rendering, RSS generation, and sitemap building. At every boundary, there is an opportunity to mix it up with a title, a tag, or a post ID.

Strong types move the verification from "did I check the right thing at the right time" to "the compiler checked everything, always." The cost is a few extra `.get()` calls when you need the raw string. The benefit is that an entire class of bugs — wrong argument order, mixed-up field assignments, accidental comparisons between unrelated values — simply cannot happen.

## The Design Philosophy

Loom's strong types follow a consistent philosophy:

1. **Phantom tags for same-underlying-type distinctions.** Slug, Title, PostId — all strings, all distinct.
2. **Simple structs for same-shape-different-meaning values.** Color vs FontStack — both hold strings, but they are not interchangeable.
3. **Enum classes for finite choice sets.** Corners, TagStyle, LinkStyle — sum types that the compiler can exhaustively match.
4. **Token types for compile-time metadata.** Bg, Text, Accent — types that carry their CSS variable name as a static member.

None of these techniques are novel. Phantom types come from ML and Haskell. Enum classes are standard C++11. Token types are just structs with static members. What makes them effective is the discipline of using them consistently — not just in the "important" parts of the codebase, but everywhere a value has domain meaning.

The strongest guarantee a type system can give you is that illegal states are unrepresentable. Not just caught, not just warned about — impossible to construct. Loom's strong types are a small step toward that ideal, and they cost almost nothing at runtime. The wrappers are optimized away. The enum switches become jump tables. The token types vanish entirely. All that remains is correct code.
