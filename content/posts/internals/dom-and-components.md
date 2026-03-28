---
title: "The DOM DSL and Component System — HTML as C++ Expressions"
date: 2025-11-21
slug: dom-and-components
tags: [loom-internals, c++20, dsl, templates, components]
excerpt: "How Loom turns HTML generation into composable C++ expressions with a fold-based DOM builder, control flow primitives, and a JSX-like component system with theme-overridable slots."
---

Most blog engines generate HTML with string templates. Jinja, Handlebars, Liquid — they all embed control flow in strings, catch errors at runtime, and give you no help when you misspell an attribute name or forget to close a tag.

Loom takes a different approach. HTML is built from C++ expressions. A `div` is a function call. Attributes and children are arguments. The compiler type-checks the tree. And components are structs with static render methods that themes can override at the function level.

This post builds on [post #8 on variadic templates and fold expressions](/post/variadic-templates-and-fold-expressions), [post #4 on lambdas](/post/lambdas-and-closures), and [post #4's coverage of std::function](/post/lambdas-and-closures).

## The Node Tree

Every piece of HTML in Loom is a `Node`. The structure lives in `include/loom/render/dom.hpp`:

```cpp
struct Node
{
    enum Kind { Element, Void, Text, Raw, Fragment } kind = Fragment;
    std::string tag;
    std::vector<Attr> attrs;
    std::vector<Node> children;
    std::string content;

    std::string render() const;
    void render_to(std::string& out) const;
};
```

Five kinds of node. `Element` is a normal HTML element like `<div>...</div>`. `Void` is a self-closing element like `<img>` or `<br>`. `Text` is escaped text content. `Raw` is pre-rendered HTML (like markdown output that has already been converted). `Fragment` is an invisible container — multiple children with no wrapper element.

Attributes are simple structs:

```cpp
struct Attr
{
    std::string name;
    std::string value;
    bool boolean = false;  // true = render as name only (no ="value")
};
```

Boolean attributes like `checked` and `disabled` render without a value. Regular attributes render as `name="value"` with proper escaping.

## The Fold Expression Trick

The magic of the DSL is in how `elem()` works:

```cpp
template<typename... Args>
Node elem(const char* tag, Args&&... args)
{
    Node n{Node::Element, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}
```

That `(detail::add(n, std::forward<Args>(args)), ...)` is a fold expression. It calls `detail::add` once for each argument, left to right. And `detail::add` is overloaded to sort arguments into the right slot:

```cpp
namespace detail
{
    inline void add(Node& n, Attr a)               { n.attrs.push_back(std::move(a)); }
    inline void add(Node& n, Node c)               { n.children.push_back(std::move(c)); }
    inline void add(Node& n, const std::string& s) { n.children.push_back({Node::Text, {}, {}, {}, s}); }
    inline void add(Node& n, const char* s)         { n.children.push_back({Node::Text, {}, {}, {}, s}); }
    inline void add(Node& n, std::vector<Node> cs)  { for (auto& c : cs) n.children.push_back(std::move(c)); }
    inline void add(Node& n, int v)                 { n.children.push_back({Node::Text, {}, {}, {}, std::to_string(v)}); }
}
```

If the argument is an `Attr`, it goes into `attrs`. If it is a `Node`, it goes into `children`. If it is a string or `const char*`, it becomes a `Text` node. If it is a `vector<Node>`, the children are flattened in. If it is an `int`, it becomes a text node via `to_string`.

This means you can mix attributes and children in any order:

```cpp
div(class_("post-card"), h2("Title"), span(class_("date"), "2025-11-21"))
```

The `class_("post-card")` returns an `Attr`. The `h2("Title")` returns a `Node`. The fold expression sorts them out. Attributes end up in `attrs`, children in `children`. No explicit separation needed.

This is the variadic template + fold expression pattern from [post #8](/post/variadic-templates-and-fold-expressions), applied to DOM construction.

## Element Factories

Each HTML element is a thin wrapper over `elem()`:

```cpp
template<typename... A> Node div(A&&... a)    { return elem("div", std::forward<A>(a)...); }
template<typename... A> Node span(A&&... a)   { return elem("span", std::forward<A>(a)...); }
template<typename... A> Node h1(A&&... a)     { return elem("h1", std::forward<A>(a)...); }
template<typename... A> Node a(A&&... a_)     { return elem("a", std::forward<A>(a_)...); }
template<typename... A> Node article(A&&... a){ return elem("article", std::forward<A>(a)...); }
```

There are about 40 of these — one for every common HTML element. Void elements use `void_elem()` instead, which creates a `Void` node that renders without a closing tag:

```cpp
template<typename... A> Node img(A&&... a)  { return void_elem("img", std::forward<A>(a)...); }
template<typename... A> Node br(A&&... a)   { return void_elem("br", std::forward<A>(a)...); }
template<typename... A> Node meta(A&&... a) { return void_elem("meta", std::forward<A>(a)...); }
```

Attributes have similar factories:

```cpp
inline Attr class_(const char* c)   { return {"class", c}; }
inline Attr href(const char* h)     { return {"href", h}; }
inline Attr src(const char* s)      { return {"src", s}; }
inline Attr alt(const char* a)      { return {"alt", a}; }
```

The trailing underscore on `class_` is necessary because `class` is a C++ keyword. Same for `main_`, `p_` (to avoid conflict with pointer conventions), and others.

There is also a `classes()` helper for conditional class lists:

```cpp
inline Attr classes(std::initializer_list<std::pair<const char*, bool>> cls)
{
    std::string result;
    for (auto& [name, active] : cls)
        if (active)
        {
            if (!result.empty()) result += ' ';
            result += name;
        }
    return {"class", std::move(result)};
}
```

Use it like: `classes({{"active", is_active}, {"sidebar", has_sidebar}})`. Only the truthy class names make it into the output.

## Control Flow

String templates have `{% if %}` and `{% for %}`. Loom has functions:

```cpp
inline Node when(bool cond, Node n)
{
    return cond ? std::move(n) : Node{Node::Fragment, {}, {}, {}, {}};
}

template<typename Fn>
Node when(bool cond, Fn&& fn)
{
    return cond ? fn() : Node{Node::Fragment, {}, {}, {}, {}};
}

inline Node unless(bool cond, Node n)
{
    return cond ? Node{Node::Fragment, {}, {}, {}, {}} : std::move(n);
}
```

`when(condition, node)` renders the node if the condition is true, otherwise returns an empty fragment. The lazy overload `when(condition, lambda)` only constructs the node when needed — important when the node construction itself might be expensive or have side effects.

Iteration uses the same pattern:

```cpp
template<typename Container, typename Fn>
Node each(const Container& items, Fn&& fn)
{
    Node n{Node::Fragment, {}, {}, {}, {}};
    n.children.reserve(items.size());
    for (const auto& item : items)
        n.children.push_back(fn(item));
    return n;
}

template<typename Container, typename Fn>
Node each_i(const Container& items, Fn&& fn)
{
    Node n{Node::Fragment, {}, {}, {}, {}};
    n.children.reserve(items.size());
    int i = 0;
    for (const auto& item : items)
        n.children.push_back(fn(item, i++));
    return n;
}
```

`each()` maps a collection to nodes. `each_i()` provides an index. Both return fragments.

Put it all together and you get something that reads like JSX but is pure C++:

```cpp
auto page = document(
    head(meta(charset("utf-8")), title("My Blog")),
    body(class_("has-sidebar"),
        main_(
            each(posts, [](auto& p) {
                return article(class_("post-card"),
                    h2(a(href("/post/" + p.slug), p.title)),
                    span(class_("date"), p.date),
                    when(!p.excerpt.empty(),
                        p_(class_("excerpt"), p.excerpt))
                );
            })
        )
    )
);
```

Every function call returns a `Node`. The compiler verifies the types. The lambdas from [post #4](/post/lambdas-and-closures) capture what they need. And the fold expressions from [post #8](/post/variadic-templates-and-fold-expressions) sort the pieces into place.

## The Component System

The DOM DSL gives you building blocks. Components give you reusable, overridable pieces. They live in `include/loom/render/component.hpp`.

A component in Loom is a struct with props and a static `render` method:

```cpp
struct PostCard  { const PostSummary* post = nullptr;
                   static Node render(const PostCard&, const Ctx&, Children); };
struct Header    { static Node render(const Header&, const Ctx&, Children); };
struct Footer    { static Node render(const Footer&, const Ctx&, Children); };
struct Index     { const std::vector<PostSummary>* posts = nullptr;
                   static Node render(const Index&, const Ctx&, Children); };
```

Every component follows the same signature: `static Node render(const Self&, const Ctx&, Children)`. The first parameter is the component instance (which carries its props). The second is the render context. The third is any children passed from outside.

The `Ctx` struct is the render context — it holds the site data and the active component overrides:

```cpp
struct Ctx
{
    const Site& site;
    const ComponentOverrides* overrides = nullptr;
    const theme::ThemeDef* theme_def = nullptr;

    template<typename C, typename... Args>
    Node operator()(const C& component, Args&&... children) const;
};
```

The `operator()` is where dispatch happens:

```cpp
template<typename C, typename... Args>
Node operator()(const C& component, Args&&... children) const
{
    Children ch;
    if constexpr (sizeof...(Args) > 0)
    {
        ch.reserve(sizeof...(Args));
        (detail::collect(ch, std::forward<Args>(children)), ...);
    }

    using Comp = std::decay_t<C>;
    if (overrides)
    {
        const auto& fn = overrides->get<Comp>();
        if (fn) return fn(component, *this, std::move(ch));
    }
    return Comp::render(component, *this, std::move(ch));
}
```

First, children are collected using another fold expression. Then, if the active theme has an override for this component type, it is called. Otherwise, the component's own static `render` method runs.

You use it like this:

```cpp
auto ctx = Ctx::from(site);
auto html = ctx(PostCard{.post = &summary}).render();
```

The `ctx(PostCard{...})` call checks if there is an override, and falls back to the default. It looks like a function call but it is doing type-level dispatch.

## ComponentOverrides

The override system uses `std::function` slots — one per component:

```cpp
template<typename C>
using RenderFn = std::function<Node(const C&, const Ctx&, Children)>;

struct ComponentOverrides
{
    RenderFn<Header>    header{};
    RenderFn<Footer>    footer{};
    RenderFn<PostCard>  post_card{};
    RenderFn<Index>     index{};
    // ... 25+ more slots
};
```

Empty slots (default-constructed `std::function`) are falsy. The `get<C>()` method uses `if constexpr` to resolve the component type to its slot:

```cpp
template<typename C>
const RenderFn<C>& get() const
{
    if constexpr (std::is_same_v<C, Header>) return header;
    else if constexpr (std::is_same_v<C, Footer>) return footer;
    else if constexpr (std::is_same_v<C, PostCard>) return post_card;
    // ...
}
```

This is a compile-time switch. There is no vtable, no runtime map lookup. The compiler knows exactly which field to access for each component type.

Themes set overrides using designated initializers:

```cpp
.components = overrides({
    .header = [](const Header&, const Ctx& ctx, Children) {
        return dom::header(
            div(class_("container header-bar"),
                h1(a(href("/"), ctx.site.title)),
                ctx(Nav{})));
    },

    .post_listing = [](const PostListing& props, const Ctx&, Children) {
        if (!props.post) return empty();
        const auto& p = *props.post;
        return article(class_("post-listing"),
            a(href("/post/" + p.slug.get()), p.title.get()),
            span(class_("date"), format_date(p.published)));
    },
})
```

This is the lambda pattern from [post #4](/post/lambdas-and-closures) combined with the `std::function` type erasure also discussed there. The lambda captures nothing (or captures by reference where needed) and is stored in a `std::function` slot.

## The defaults() Escape Hatch

Sometimes you want to wrap the default rendering, not replace it entirely:

```cpp
template<typename C>
Node defaults(const C& props, const Ctx& ctx, Children ch)
{
    return C::render(props, ctx, std::move(ch));
}
```

In an override:

```cpp
.post_full = [](const PostFull& p, const Ctx& ctx, Children ch) {
    return div(class_("my-wrapper"), defaults(p, ctx, std::move(ch)));
}
```

You get the default rendering inside your custom wrapper. This is the decorator pattern, but expressed as a function call rather than an inheritance hierarchy.

## The ui Namespace

Writing theme overrides requires using both DOM functions and component types. To avoid namespace clutter, Loom provides a convenience namespace:

```cpp
namespace loom::ui
{
    using component::Ctx;
    using component::Children;
    using component::overrides;
    using component::defaults;
    using dom::div;
    using dom::span;
    using dom::h1;
    using dom::class_;
    using dom::href;
    using dom::when;
    using dom::each;
    // ... many more
}
```

A single `using namespace ui;` at the top of a theme file gives you everything you need. But note the careful exclusions: `dom::raw` is not re-exported (it clashes with `css::raw`), and `dom::header`, `dom::footer`, `dom::nav` are excluded because those names are used for the component structs `Header`, `Footer`, `Nav`. When you need the HTML element in a theme override, you use `dom::header()`.

## Rendering

The `Node::render_to()` method produces HTML:

```cpp
void render_to(std::string& out) const
{
    switch (kind)
    {
        case Text:     escape_text(out, content); break;
        case Raw:      out += content; break;
        case Fragment:
            for (const auto& c : children) c.render_to(out);
            break;
        case Void:
            out += '<'; out += tag;
            render_attrs(out);
            out += '>';
            break;
        case Element:
            out += '<'; out += tag;
            render_attrs(out);
            out += '>';
            for (const auto& c : children) c.render_to(out);
            out += "</"; out += tag; out += '>';
            break;
    }
}
```

A simple recursive tree walk. Text is HTML-escaped. Raw content passes through unmodified (used for pre-rendered markdown). Fragments render their children without a wrapper element. Void elements have no closing tag. Elements render open tag, children, close tag.

No `std::ostringstream`. No temporary strings for intermediate results. Just direct string appending into a pre-reserved buffer. The `render()` convenience method reserves 512 bytes upfront — a good starting point that avoids most reallocations for small components.

## Why Not Templates?

You might wonder why Loom does not use a proper template engine with files like `header.html`, `footer.html`, and `post.html`. There are a few reasons.

First, C++ template engines (like inja or ctemplate) are runtime systems. They parse template files, evaluate expressions, and concatenate strings. Errors show up at runtime — a misspelled variable name, a missing loop end tag, a wrong type. Loom's approach catches all of these at compile time.

Second, the component system gives themes structural control. A string template can change what text appears where, but the HTML structure is fixed by the template. Loom's overrides can completely change the HTML — adding elements, removing elements, rearranging the tree. The hacker theme, for instance, replaces the default post listing with a completely different structure that includes inline metadata spans and unix-style formatting.

Third, the DOM tree is data. You can inspect it, transform it, serialize it. The component system builds trees, and the rendering step serializes them. This clean separation makes it straightforward to do things like HTML minification as a post-processing step.

The trade-off is compilation time and the fact that changing a template requires recompiling. For Loom, this is acceptable — themes change rarely, and the compile-test cycle is fast enough for iteration.
