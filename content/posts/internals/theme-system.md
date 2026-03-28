---
title: "The Theme Compiler — From ThemeDef to CSS"
date: 2025-11-28
slug: theme-system
tags: [loom-internals, c++20, themes, css, dsl]
excerpt: "How Loom compiles typed theme definitions into CSS using pointer-to-member bindings, fold expressions, structural emitters, a full CSS DSL, and component overrides."
---

Most blog themes are CSS files. Maybe SCSS if you are feeling fancy. You edit variables, override selectors, and hope nothing breaks when you change a color. Loom takes a completely different approach: themes are C++ structs. Colors, fonts, spacing, component styles, even HTML structure overrides — everything lives in a typed definition that the compiler verifies before a single byte of CSS is generated.

This post covers the full pipeline from `ThemeDef` to CSS output. It builds on [post #15 on pointer-to-member](/post/pointer-to-member-and-type-level-programming), [post #8 on fold expressions](/post/variadic-templates-and-fold-expressions), [post #3 on designated initializers](/post/structured-bindings-and-initializers), and [post #9 on constexpr](/post/constexpr-consteval-and-compile-time).

## ThemeDef: The Typed Surface

A theme is a `ThemeDef` struct. Here is the real definition, abbreviated:

```cpp
struct ThemeDef
{
    // Colors
    Palette   light;
    Palette   dark;

    // Typography
    FontStack   font;
    std::string font_size;
    std::string max_width;
    FontStack   heading_font   = {};
    FontStack   code_font      = {};
    std::string line_height    = {};
    std::string heading_weight = {};
    std::string header_size    = {};

    // Shape & density
    Corners      corners      = Corners::Soft;
    Density      density      = Density::Normal;
    BorderWeight border_weight = BorderWeight::Normal;

    // Component styles (CSS-level)
    TagStyle     tag_style    = TagStyle::Pill;
    LinkStyle    link_style   = LinkStyle::Underline;
    CodeBlockStyle code_style = CodeBlockStyle::Plain;
    // ... 13 more enum fields

    // Custom styles (typed CSS DSL)
    css::Sheet styles = {};

    // Raw escape hatch
    std::string extra_css = {};

    // Component overrides (structural / HTML-level)
    std::shared_ptr<component::ComponentOverrides> components = {};
};
```

Every field has a sensible default. Only `light`, `dark`, `font`, `font_size`, and `max_width` need to be specified. Everything else falls back to the base CSS behavior.

Themes are defined using C++20 designated initializers from [post #3](/post/structured-bindings-and-initializers):

```cpp
inline const ThemeDef terminal = {
    .light = {{"#1a1a1a"}, {"#d8d8d8"}, {"#777777"}, {"#2e2e2e"}, {"#5fba7d"}},
    .dark  = {{"#1a1a1a"}, {"#d8d8d8"}, {"#777777"}, {"#2e2e2e"}, {"#5fba7d"}},
    .font  = {"ui-monospace,'SF Mono',SFMono-Regular,Menlo,Consolas,monospace"},
    .font_size = "13.5px",
    .max_width = "720px",
    .corners = Corners::Sharp,
    .border_weight = BorderWeight::Thin,
    .tag_style = TagStyle::Bordered,
    .link_style = LinkStyle::None,
    .card_hover = CardHover::Border,
    // ...
};
```

This reads like a configuration file, but it compiles. You cannot set `.corners` to `"sharp"` (a string) — it must be `Corners::Sharp` (an enum value). You cannot put a color where a font stack goes. The compiler enforces the schema.

## ColorBinding: Pointer-to-Member Meets Fold Expressions

The most interesting piece of the theme compiler is how colors are emitted. Each color token (Bg, Text, Muted, Border, Accent) needs to produce a CSS custom property from the corresponding palette field. The `ColorBinding` template encodes this relationship:

```cpp
template<typename Token, Color Palette::* Member>
struct ColorBinding
{
    static void emit(std::string& css, const Palette& p)
    {
        css += Token::var;    // e.g. "--bg"
        css += ':';
        css += (p.*Member).value;  // e.g. "#1a1a1a"
        css += ';';
    }
};
```

`Token` is one of the token types from the previous post — `Bg`, `Text`, etc. — each carrying a `static constexpr var` member with its CSS variable name. `Color Palette::* Member` is a pointer-to-member: a compile-time reference to a specific `Color` field within `Palette`.

The expression `(p.*Member).value` dereferences the pointer-to-member on the palette instance `p`, giving us the `Color` struct, and then accesses its `.value` string. This is the technique from [post #15](/post/pointer-to-member-and-type-level-programming).

All five bindings are collected into a tuple:

```cpp
using ColorBindings = std::tuple<
    ColorBinding<Bg,     &Palette::bg>,
    ColorBinding<Text,   &Palette::text>,
    ColorBinding<Muted,  &Palette::muted>,
    ColorBinding<Border, &Palette::border>,
    ColorBinding<Accent, &Palette::accent>
>;
```

And emitted with a fold expression:

```cpp
template<typename... Bs>
void emit_palette(std::string& css, const Palette& p, std::tuple<Bs...>)
{
    (Bs::emit(css, p), ...);
}
```

The tuple parameter is only used for type deduction — its runtime value is never touched. The fold expression expands to:

```cpp
ColorBinding<Bg, &Palette::bg>::emit(css, p);
ColorBinding<Text, &Palette::text>::emit(css, p);
ColorBinding<Muted, &Palette::muted>::emit(css, p);
ColorBinding<Border, &Palette::border>::emit(css, p);
ColorBinding<Accent, &Palette::accent>::emit(css, p);
```

Five function calls, zero indirection, zero runtime overhead. The compiler can inline all of them.

## Structural Emitters

The sixteen `enum class` fields in `ThemeDef` each have a corresponding emitter function. These are simple switch statements that map enum values to CSS strings:

```cpp
inline void emit_tag_style(std::string& css, TagStyle s)
{
    switch (s)
    {
        case TagStyle::Pill: break;  // default
        case TagStyle::Rect:
            css += ":root{--tag-radius:0;}";
            break;
        case TagStyle::Bordered:
            css += ":root{--tag-bg:transparent;--tag-text:var(--accent);--tag-radius:0;}";
            css += ".tag{border:1px solid var(--accent);}";
            break;
        case TagStyle::Outline:
            css += ":root{--tag-bg:transparent;--tag-text:var(--muted);--tag-radius:0;}";
            css += ".tag{border:1px solid var(--muted);}";
            css += ".tag:hover{border-color:var(--accent);color:var(--accent);}";
            break;
        case TagStyle::Plain:
            css += ":root{--tag-bg:transparent;--tag-text:var(--muted);}";
            css += ".tag{padding:0 4px;}";
            break;
    }
}
```

The pattern is consistent: the first enum value is the default (the base CSS handles it), so the case body is `break`. Other values emit CSS that overrides the defaults.

Some emitters are more involved. `emit_density()` changes line-height, container padding, paragraph margins, heading margins, listing padding, and widget margins — six separate CSS rules for `Compact`, another six for `Airy`.

The `compile()` function calls all of them in sequence:

```cpp
inline std::string compile(const ThemeDef& t)
{
    std::string css;

    // Light mode
    css += ":root{";
    detail::emit_palette(css, t.light, detail::ColorBindings{});
    css += Font::var; css += ':'; css += t.font.value; css += ';';
    css += FontSize::var; css += ':'; css += t.font_size; css += ';';
    css += MaxWidth::var; css += ':'; css += t.max_width; css += ';';
    detail::emit_typography(css, t);
    css += '}';

    // Dark mode
    css += "[data-theme=\"dark\"]{";
    detail::emit_palette(css, t.dark, detail::ColorBindings{});
    css += '}';

    // All structural emitters
    detail::emit_corners(css, t.corners);
    detail::emit_density(css, t.density);
    detail::emit_border_weight(css, t.border_weight);
    detail::emit_nav_style(css, t.nav_style);
    detail::emit_tag_style(css, t.tag_style);
    // ... 12 more

    // Typed styles
    if (!t.styles.empty())
        css += t.styles.compile();

    // Raw escape hatch
    if (!t.extra_css.empty())
        css += t.extra_css;

    return css;
}
```

The output is a single CSS string, no pretty-printing, no whitespace. It gets minified anyway during the build pipeline.

## The CSS DSL

For anything the structural emitters do not cover, Loom has a full type-safe CSS DSL in `include/loom/render/theme/css.hpp`. The terminal theme uses it extensively:

```cpp
.styles = sheet(
    "header h1"_s | font_weight(700) | font_size(32_px) | letter_spacing(1_px),
    link_colors("nav a", dim, green),
    content_area().nest(
        "a"_s         | color(green),
        "a"_s.hover() | text_decoration(underline),
        "pre"_s       | border(1_px, solid, line)
    ),
    keyframes("blink",
        frame(raw("0%,50%"))    | opacity(1.0),
        frame(raw("50.01%,100%")) | opacity(0.0)
    )
)
```

Let me break down the layers.

### Values

```cpp
struct Val { std::string v; };

inline Val px(int n)      { return {std::to_string(n) + "px"}; }
inline Val hex(const char* c) { return {c}; }
inline Val raw(const char* s) { return {s}; }

// User-defined literals
inline Val operator""_px(unsigned long long n) { return {std::to_string(n) + "px"}; }
inline Val operator""_em(long double n)        { /* ... */ }
```

Values are typed wrappers around strings. `32_px` is not a string — it is a `Val` containing `"32px"`. The user-defined literals from C++11 make this readable.

CSS variable references are pre-defined:

```cpp
namespace v {
    inline const Val bg{"var(--bg)"};
    inline const Val accent{"var(--accent)"};
    inline const Val muted{"var(--muted)"};
}
```

### Declarations

```cpp
struct Decl { std::string prop, val; };

inline Decl color(const Val& c)      { return {"color", c.v}; }
inline Decl bg(const Val& c)         { return {"background", c.v}; }
inline Decl font_size(const Val& v)  { return {"font-size", v.v}; }
inline Decl border(const Val& w, const Val& s, const Val& c)
    { return {"border", w.v + " " + s.v + " " + c.v}; }
```

Each CSS property is a function that returns a `Decl`. The function name matches the CSS property name (with underscores for hyphens).

### Selectors

```cpp
struct Sel { std::string s; };

inline Sel operator""_s(const char* str, size_t) { return {str}; }
```

The `_s` literal turns a string into a selector. Selectors chain:

```cpp
".card"_s               // .card
".card"_s.hover()       // .card:hover
".card"_s.dark()        // [data-theme="dark"] .card
".x"_s.also(".y")       // .x,.y
```

### Rules

A rule is a selector plus declarations, built with the pipe operator:

```cpp
inline Rule operator|(Sel sel, Decl d)
{ return {std::move(sel.s), {std::move(d)}}; }

inline Rule operator|(Rule&& r, Decl d)
{ r.decls.push_back(std::move(d)); return std::move(r); }
```

So `"header"_s | font_size(32_px) | color(hex("#fff"))` builds a `Rule` with selector `"header"` and two declarations.

### Nesting

The `nest()` method on `Sel` provides SCSS-like nesting:

```cpp
content_area().nest(
    "a"_s         | color(v::accent),
    "a"_s.hover() | text_decoration(underline)
)
```

`content_area()` returns `Sel{".post-content,.page-content"}`. The `nest()` method prepends the parent selector to each child, expanding comma-separated parents correctly:

```
.post-content a,.page-content a { color: var(--accent); }
.post-content a:hover,.page-content a:hover { text-decoration: underline; }
```

### Helpers

The DSL includes several convenience helpers:

```cpp
// Two rules for base + hover colors
inline RulePack link_colors(const char* sel, const Val& base, const Val& hover);

// CSS variables on :root
inline Rule vars(std::initializer_list<std::pair<const char*, Val>> assignments);
```

`link_colors("nav a", dim, green)` expands to two rules: `nav a { color: <dim>; }` and `nav a:hover { color: <green>; }`. This pattern appears dozens of times in themes.

### Sheet Compilation

A `Sheet` collects rules and special blocks (media queries, keyframes):

```cpp
template<typename... Args>
Sheet sheet(Args&&... args)
{
    Sheet s;
    (detail::flatten(s, std::forward<Args>(args)), ...);
    return s;
}
```

Another fold expression. Each argument is flattened into the sheet — `Rule` values are added directly, `Nest` values are expanded, `MediaBlock` and `KeyframeBlock` values are compiled into string blocks.

## Theme Composition with derive()

Sometimes you want a theme that is mostly like another theme but with a few changes. The `derive()` function supports this:

```cpp
template<typename F>
ThemeDef derive(ThemeDef base, F&& overrides)
{
    overrides(base);
    return base;
}
```

It copies the base theme and lets you mutate it:

```cpp
auto my_theme = derive(terminal, [](ThemeDef& t) {
    t.corners = Corners::Round;
    t.tag_style = TagStyle::Pill;
});
```

The copy happens by value, so the original is untouched. The lambda receives a mutable reference to the copy.

## Component Overrides in Themes

The `components` field on `ThemeDef` is a `shared_ptr<ComponentOverrides>`. It uses `shared_ptr` because `ThemeDef` needs to be copyable (for `derive()`), and `ComponentOverrides` is a large struct with many `std::function` fields.

The hacker theme demonstrates structural overrides in action:

```cpp
.components = overrides({
    .header = [](const Header&, const Ctx& ctx, Children) {
        const auto& s = ctx.site;
        return dom::header(
            div(class_("container header-bar"),
                div(class_("header-left"),
                    h1(a(href("/"), s.title)),
                    when(s.layout.show_description && !s.description.empty(),
                        p_(class_("site-description"), s.description)),
                    ctx(Nav{}))));
    },

    .post_listing = [](const PostListing& props, const Ctx&, Children) {
        if (!props.post) return empty();
        const auto& p = *props.post;
        return article(class_("post-listing"),
            a(href("/post/" + p.slug.get()), p.title.get()),
            span(class_("ls-inline"),
                fmt_date_short(p.published) + "  "
                + fmt_size(p.reading_time_minutes)));
    },
})
```

This theme replaces both the header HTML structure and the post listing format. The header adds a description paragraph. The listing shows metadata inline in a unix-ls style format. These are not CSS changes — they are entirely different HTML trees.

## The Full Pipeline

When a theme is applied, the pipeline works like this:

1. `ThemeDef` is defined as a `constexpr`-ish global (not actually `constexpr` because of `std::string`, but initialized at startup).
2. `compile(theme_def)` produces the CSS string. This happens once at build time.
3. The CSS is injected into the `<style>` tag via the `Head` component.
4. The `ComponentOverrides` pointer is stored on the `Ctx` render context.
5. Every `ctx(SomeComponent{...})` call checks for an override before falling back to the default.
6. The full HTML tree is rendered, minified, gzip-compressed, and cached.

The theme definition is not interpreted at request time. It is fully compiled into CSS and component dispatch at build time. At request time, the server just writes pre-built bytes.

This is what makes the system zero-overhead at runtime: all the type-safe abstraction — the token types, the pointer-to-member bindings, the fold expressions, the CSS DSL — collapses into a flat CSS string and a set of function pointers. The types guide the programmer. The output is minimal.
