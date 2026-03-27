---
title: Strong Types — Making Illegal States Unrepresentable
date: 2025-10-27
slug: strong-types
tags: internals, cpp, architecture
excerpt: A slug is not a title is not a tag. Loom wraps every domain value in a StrongType<T, Tag> so the compiler catches misuse that code review misses.
---

Consider this function signature:

```cpp
Post load_post(const std::string& title,
               const std::string& slug,
               const std::string& series);
```

Nothing stops a caller from accidentally passing arguments in the wrong order. `load_post(slug, title, series)` compiles fine and produces a broken post that's hard to debug. Loom solves this with strong types.

## The StrongType Wrapper

```cpp
template<typename T, typename Tag>
class StrongType {
public:
    explicit StrongType(T value) : value_(std::move(value)) {}

    const T& get() const noexcept { return value_; }

    bool empty() const
        requires requires(const T& v) { v.empty(); }
    { return value_.empty(); }

    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    bool operator!=(const StrongType& other) const { return value_ != other.value_; }
    bool operator< (const StrongType& other) const { return value_ < other.value_; }

private:
    T value_;
};
```

`Tag` is a phantom type — it exists only to make each instantiation distinct. `StrongType<std::string, SlugTag>` and `StrongType<std::string, TitleTag>` are different types even though both wrap `std::string`.

## All Strong Types in Loom

```cpp
// types.hpp
struct PostIdTag {};
struct TitleTag {};
struct SlugTag {};
struct TagTag {};
struct ContentTag {};
struct SeriesTag {};

using PostId  = StrongType<std::string, PostIdTag>;
using Title   = StrongType<std::string, TitleTag>;
using Slug    = StrongType<std::string, SlugTag>;
using Tag     = StrongType<std::string, TagTag>;
using Content = StrongType<std::string, ContentTag>;
using Series  = StrongType<std::string, SeriesTag>;
```

Now `Post` is:

```cpp
struct Post {
    PostId  id;
    Title   title;
    Slug    slug;
    Content content;   // rendered HTML
    std::vector<Tag> tags;
    std::chrono::system_clock::time_point published;
    bool    draft;
    std::string excerpt;
    std::string image;
    int     reading_time_minutes;
    Series  series;
    std::filesystem::file_time_type mtime;
};
```

And `load_post` becomes:

```cpp
Post load_post(const std::string& path,
               const std::string& series_name,
               int& counter,
               const std::string& content_dir);
```

Within that function, you can't accidentally assign `slug` to `title`:

```cpp
Title t = Title("My Post");
Slug  s = Slug("my-post");
t = s; // compile error: cannot convert Slug to Title
```

## The C++20 `requires` Clause

The `empty()` method uses a C++20 *requires clause* on a member function:

```cpp
bool empty() const
    requires requires(const T& v) { v.empty(); }
```

This says: "this method only exists if `T` has an `empty()` member." For `StrongType<std::string, ...>`, `std::string` has `empty()`, so the method is available. For a hypothetical `StrongType<int, ...>`, it would be silently excluded from the type.

The outer `requires` gates the member. The inner `requires(...)` is a *requires expression* — a compile-time boolean that checks whether the expression `v.empty()` is valid. This is a compound requires expression.

Without C++20:
```cpp
// C++17 SFINAE equivalent — harder to read, harder to write
template<typename U = T, std::enable_if_t</* has_empty<U> */> = 0>
bool empty() const { return value_.empty(); }
```

## Why `explicit` on the Constructor

```cpp
explicit StrongType(T value) : value_(std::move(value)) {}
```

`explicit` prevents implicit conversion. Without it:

```cpp
void render_tag_page(const Tag& tag) { ... }
render_tag_page("cpp"); // would compile without explicit — wrong type, wrong place
```

With `explicit`, the call fails to compile. You must write `render_tag_page(Tag("cpp"))`, making the intent visible.

`std::move(value)` moves the argument into the wrapper rather than copying. For strings, this is zero cost if the caller passes an rvalue — the string's internal buffer is transferred, no allocation.

## Using `.get()` to Extract Values

When you need the underlying string — to build a URL, compare against a raw string, pass to a C API — you call `.get()`:

```cpp
std::string url = "/post/" + post.slug.get();
std::string heading = post.title.get();
cache->pages["/tag/" + tag.get()] = make_cached(...);
```

The `.get()` suffix is intentionally a tiny friction. It makes raw-value access visible in code review, nudging you toward strong-typed APIs at boundaries.

## Comparison and Sorting

Strong types support `==`, `!=`, and `<`. This means they work naturally in standard algorithms:

```cpp
// Sort posts by title
std::sort(posts.begin(), posts.end(),
    [](const Post& a, const Post& b) { return a.title < b.title; });

// Find a post by slug
auto it = std::find_if(posts.begin(), posts.end(),
    [&](const Post& p) { return p.slug == Slug(slug_str); });

// Use Tag in a std::set for deduplication
std::set<Tag> unique_tags;
for (const auto& post : posts)
    unique_tags.insert(post.tags.begin(), post.tags.end());
```

The `<` operator delegates to `T`'s `<`, so `Tag("cpp") < Tag("rust")` is a string comparison. This is correct for alphabetical sorting; it's not meaningful as a domain ordering, but that's fine because Tag values are just labels.

## What Strong Types Don't Give You

Strong types catch *mixing* of values with different semantic roles. They don't validate the *content* of a value. `Slug("this has spaces!")` is a valid `Slug` that will produce a broken URL.

For that, you'd add validation in the constructor — or use a separate validation step at the content-loading boundary. Loom takes the latter approach: slugs are validated/normalised when loading content from disk, before they become `Slug` values.

## Strong Types in the Theme System

The same philosophy drives Loom's theming engine. The old theme system stored everything as raw strings:

```cpp
struct ThemeColors {
    std::string bg, text, muted, border, accent;
    std::string font;
    std::string font_size;
    std::string max_width;
    std::string dark_bg, dark_text, dark_muted, dark_border, dark_accent;
    std::string extra_css;
};
```

Nothing prevents you from assigning a font stack to `bg`, or accidentally swapping `muted` and `border`. Every field is `std::string` — the compiler can't help.

The new system introduces strong types for values and semantic token types for CSS variables:

```cpp
// Strong value types — a color is not a font stack
struct Color     { std::string value; };
struct FontStack { std::string value; };
```

Colors group into a `Palette` — one for light mode, one for dark:

```cpp
struct Palette {
    Color bg;
    Color text;
    Color muted;
    Color border;
    Color accent;
};
```

Now you can't assign a `FontStack` to a `Color` field. And you can't forget a color — the struct requires all five.

The full theme definition composes these:

```cpp
struct ThemeDef {
    Palette   light;
    Palette   dark;
    FontStack font;
    std::string font_size;
    std::string max_width;
    std::string extra_css;
};
```

### Token Types for CSS Variables

Each CSS variable (`--bg`, `--text`, etc.) is represented by a tag type that carries its own name:

```cpp
struct Bg     { static constexpr auto var = "--bg"; };
struct Text   { static constexpr auto var = "--text"; };
struct Muted  { static constexpr auto var = "--muted"; };
struct Border { static constexpr auto var = "--border"; };
struct Accent { static constexpr auto var = "--accent"; };
```

This is the single source of truth — the CSS variable name lives with the type, not scattered across string-building code.

### The Compiler: Fold Expressions over Token Bindings

The old CSS compiler manually concatenated each variable:

```cpp
css += "--bg:" + t.bg + ";";
css += "--text:" + t.text + ";";
// ...eight more lines of the same pattern
```

The new system binds each token type to its palette accessor at compile time:

```cpp
template<typename Token, Color Palette::* Member>
struct ColorBinding {
    static void emit(std::string& css, const Palette& p) {
        css += Token::var;  // "--bg", "--text", etc.
        css += ':';
        css += (p.*Member).value;
        css += ';';
    }
};

using ColorBindings = std::tuple<
    ColorBinding<Bg,     &Palette::bg>,
    ColorBinding<Text,   &Palette::text>,
    ColorBinding<Muted,  &Palette::muted>,
    ColorBinding<Border, &Palette::border>,
    ColorBinding<Accent, &Palette::accent>
>;
```

The fold expression iterates over all bindings at compile time:

```cpp
template<typename... Bs>
void emit_palette(std::string& css, const Palette& p, std::tuple<Bs...>) {
    (Bs::emit(css, p), ...);
}
```

Adding a new color token means adding one line to `ColorBindings`. The compiler verifies the pointer-to-member matches the `Palette` struct. If you misspell a field name or point to the wrong type, it's a compile error — not a runtime bug producing broken CSS.

### Theme Definitions as Data

Each builtin theme is a header file with a single `inline const ThemeDef`:

```cpp
// builtin/gruvbox.hpp
inline const ThemeDef gruvbox = {
    .light = {{"#fbf1c7"}, {"#3c3836"}, {"#766a60"}, {"#d5c4a1"}, {"#d65d0e"}},
    .dark  = {{"#282828"}, {"#ebdbb2"}, {"#a89984"}, {"#3c3836"}, {"#fe8019"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(...)CSS",
};
```

C++20 designated initializers make the mapping explicit. Miss a field and the compiler warns. Swap `.light` and `.dark` and the type system catches it — `Palette` expects `Color` values, not `FontStack`.

### Theme Composition

Because `ThemeDef` is a regular struct, deriving a variant is just copy-and-mutate:

```cpp
ThemeDef derive(ThemeDef base, auto&& overrides) {
    overrides(base);
    return base;
}

auto my_gruvbox = derive(gruvbox, [](ThemeDef& t) {
    t.light.accent = {"#cc241d"};
    t.dark.accent  = {"#fb4934"};
});
```

No inheritance hierarchies, no trait objects. Just value semantics.

## The Bigger Pattern: Making Illegal States Unrepresentable

Strong types are one instance of a broader C++ idiom: encode invariants in types so the compiler enforces them.

Other examples in Loom:
- `Color` vs `FontStack` — a color can't become a font family
- `Palette` requires all five color tokens — you can't define a theme with a missing `muted`
- Token types carry their CSS variable names — rename `--bg` in one place and it updates everywhere
- `const Site&` parameter to renderers — the site is read-only during rendering
- `std::shared_ptr<const SiteCache>` — the cache is immutable once built
- `std::unique_ptr<TrieNode>` — each trie node has exactly one owner

The pattern is: **if something should not be done, make the type system prevent it rather than the programmer remember not to.**
