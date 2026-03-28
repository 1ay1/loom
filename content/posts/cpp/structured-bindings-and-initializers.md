---
title: "Structured Bindings, Designated Initializers, and Modern Syntax"
date: 2025-08-15
slug: structured-bindings-and-initializers
tags: cpp, modern-cpp, syntax, cpp17, cpp20
excerpt: C++17 and C++20 made the language read like it was designed this century. Here's the syntax that changed everything.
---

C++ has a reputation for being verbose. That reputation was earned — in C++11 and earlier, you'd write three lines of boilerplate to do what Python does in one. But C++17 and C++20 introduced syntax features that transformed how modern C++ reads. This post covers the three most impactful ones: structured bindings, designated initializers, and scoped enumerations.

## Structured Bindings

Before C++17, if you wanted to iterate over a map and use both the key and value, you wrote this:

```cpp
for (const auto& pair : connections_) {
    int fd = pair.first;
    Connection& conn = pair.second;
    // use fd and conn
}
```

With structured bindings, you write this:

```cpp
for (const auto& [fd, conn] : connections_) {
    // fd and conn are directly available
}
```

The `[fd, conn]` syntax destructures the pair into named variables. It works with any pair, tuple, or struct with public members.

Loom uses structured bindings throughout its codebase. Here's the inotify watcher cleaning up on shutdown:

```cpp
void stop() {
    for (auto& [wd, _] : watches_)
        inotify_rm_watch(fd_, wd);
    watches_.clear();
}
```

The `_` is a convention for "I don't care about this value." We only need the watch descriptor, not the WatchEntry. The structured binding makes this intent clear.

Here's another example from the CSS DSL, iterating over variable assignments:

```cpp
inline Rule vars(std::initializer_list<std::pair<const char*, Val>> assignments) {
    Rule r{":root", {}};
    for (const auto& [name, val] : assignments)
        r.decls.push_back({std::string("--") + name, val.v});
    return r;
}
```

Without structured bindings, you'd write `assignments.first` and `assignments.second`. With them, you write `name` and `val`. The code reads like prose.

Structured bindings also work with functions that return pairs or tuples:

```cpp
auto [prev, next] = engine.prev_next(post);
// prev is std::optional<PostSummary>
// next is std::optional<PostSummary>
```

And with structs:

```cpp
struct Attr {
    std::string name;
    std::string value;
    bool boolean = false;
};

auto [name, value, is_boolean] = some_attr;
```

The binding unpacks the struct fields in declaration order. This works because `Attr` has all public members. Private members would require a different approach (custom `get<>` specializations), but in practice, data structs with public fields are the common case.

## Designated Initializers

C++20 brought designated initializers from C99. They let you name the fields you're initializing:

```cpp
Post post{
    .id = PostId("001"),
    .title = Title("Hello World"),
    .slug = Slug("hello-world"),
    .content = Content("..."),
    .tags = {Tag("cpp"), Tag("tutorial")},
    .published = now,
    .draft = false,
    .excerpt = "A first post",
};
```

Compare this to positional initialization:

```cpp
Post post{PostId("001"), Title("Hello World"), Slug("hello-world"),
          Content("..."), {Tag("cpp")}, now, false, "A first post"};
```

The designated version is longer but self-documenting. You can see what each value means. You can skip fields that have defaults. You can't accidentally swap two fields of the same type.

Loom uses designated initializers everywhere, especially in component props:

```cpp
auto html = ctx(PostFull{.post = &post, .context = &post_ctx});
```

And in the component override system, where theme authors configure their overrides:

```cpp
.components = overrides({
    .header = [](const Header&, const Ctx& ctx, Children) {
        return dom::header(
            dom::div(class_("container"),
                dom::h1(dom::a(href("/"), ctx.site.title))));
    },
})
```

That `.header = ` makes it immediately clear which component is being overridden. Without designated initializers, you'd need to set each field separately or rely on position in a struct with 30+ fields. That's not just ugly — it's a source of bugs.

## enum class: Scoped Enumerations

Old C-style enums are dangerous. They leak their names into the enclosing scope, and they implicitly convert to integers:

```cpp
// Old style — don't do this
enum Color { Red, Green, Blue };
enum TrafficLight { Red, Yellow, Green };  // ERROR: Red and Green already defined
```

C++11 introduced `enum class`, which fixes both problems:

```cpp
enum class HttpMethod : uint8_t {
    GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, UNKNOWN
};
```

This is Loom's HTTP method enum. The values are scoped — you write `HttpMethod::GET`, not just `GET`. There's no implicit conversion to integer. And the `: uint8_t` specifies the underlying storage type, so each method takes exactly one byte.

Loom's theme system uses enum classes extensively for configuration tokens:

```cpp
enum class Corners { Sharp, Soft, Round };
enum class Density { Compact, Normal, Spacious };
enum class NavStyle { Default, Centered, Minimal };
enum class TagStyle { Pill, Square, Minimal, Outline, Badge };
enum class LinkStyle { Underline, Color, Subtle };
enum class CodeBlockStyle { Plain, Framed, Terminal };
```

Each enum class is a closed set of named options. You can't accidentally pass a `Corners` value where a `Density` is expected. You can't compare a `TagStyle` to an integer. The type system protects you.

Using them in a theme definition reads beautifully:

```cpp
ThemeDef nord_theme{
    .light = { /* ... */ },
    .dark = { /* ... */ },
    .font = {"'Inter', sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .corners = Corners::Soft,
    .density = Density::Normal,
    .tag_style = TagStyle::Pill,
    .link_style = LinkStyle::Underline,
    .code_style = CodeBlockStyle::Framed,
};
```

Every option is readable, typesafe, and IDE-autocomplete-friendly. You never need to remember whether the value for soft corners is `0`, `1`, or `"soft"`. It's `Corners::Soft`, and the compiler won't accept anything else.

## initializer_list: List Initialization

`std::initializer_list` enables the brace-enclosed list syntax for your own types:

```cpp
#include <initializer_list>

inline Attr classes(std::initializer_list<std::pair<const char*, bool>> cls) {
    std::string result;
    for (auto& [name, active] : cls) {
        if (active) {
            if (!result.empty()) result += ' ';
            result += name;
        }
    }
    return {"class", std::move(result)};
}
```

This is from Loom's DOM DSL. The function takes a list of (class-name, condition) pairs and builds a class attribute string from only the active ones:

```cpp
dom::classes({{"active", is_active}, {"sidebar", has_sidebar}})
```

Without `initializer_list`, you'd need to pass a vector (heap allocation) or use variadic templates (more complex). The `initializer_list` gives you the cleanest call syntax.

The CSS DSL uses the same pattern for defining CSS variables:

```cpp
vars({{"tag-bg", transparent}, {"tag-radius", raw("0")}})
```

## The if-with-initializer

C++17 also added the ability to declare a variable inside an `if` statement:

```cpp
if (auto it = cache.find(request.path); it != cache.end()) {
    return it->second;  // found in cache
}
// it is not in scope here — no accidental use after the check
```

The variable `it` is scoped to the if-else block. This prevents a common bug where you declare an iterator before the check and accidentally use it after the check fails.

Loom's blog engine uses this pattern for post lookups:

```cpp
if (auto post = engine.get_post(Slug(param)); post) {
    // render the post
} else {
    // 404
}
```

## Combining Everything

Here's a function that uses all the syntax we've covered:

```cpp
std::vector<PostSummary> posts_by_tag(const Tag& tag) const {
    std::vector<PostSummary> result;
    auto now = std::chrono::system_clock::now();

    for (const auto& p : site_.posts) {
        if (p.draft || p.published > now)
            continue;
        for (const auto& t : p.tags) {
            if (t.get() == tag.get()) {
                result.push_back(make_summary(p));
                break;
            }
        }
    }

    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            if (a.published != b.published) return a.published > b.published;
            return a.modified_at > b.modified_at;
        });

    return result;
}
```

Range-for with `const auto&`. Structured bindings would appear if we were iterating maps. The sort uses a lambda with `const auto&` parameters. The function returns by value (the compiler will move it — no copy). Every piece of modern syntax serves a purpose: readability, safety, performance.

These features aren't syntactic sugar. They change how you think about code. Designated initializers make you design structs with clear, named fields. Structured bindings make you return multiple values instead of using output parameters. Scoped enums make you define closed vocabularies. The syntax shapes the design, and the design is better for it.

Next: lambdas. The feature that turned C++ from an object-oriented language into a multiparadigm one.
