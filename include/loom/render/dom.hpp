#pragma once
// ═══════════════════════════════════════════════════════════════════════
//  dom.hpp — compile-time type-safe HTML DSL for C++20
//
//  HTML as C++ expressions:
//
//    using namespace dom;
//
//    auto page = document(
//        head(
//            meta(charset("utf-8")),
//            title("My Blog")
//        ),
//        body(class_("has-sidebar"),
//            header(
//                div(class_("container"),
//                    h1(a(href("/"), "Loom")),
//                    nav(ul(
//                        li(a(href("/"), "Home")),
//                        li(a(href("/about"), "About"))
//                    ))
//                )
//            ),
//            main_(
//                each(posts, [](auto& p) {
//                    return article(class_("post-card"),
//                        h2(a(href("/post/" + p.slug), p.title)),
//                        span(class_("date"), p.date),
//                        when(!p.excerpt.empty(),
//                            p_(class_("excerpt"), p.excerpt))
//                    );
//                })
//            ),
//            footer(div(class_("container"), "© 2025"))
//        )
//    );
//
//    std::string html = page.render();
//
// ═══════════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <initializer_list>
#include <utility>

namespace loom::dom
{

// ─────────────────────────────────────────────────────────────────────
//  Attributes
// ─────────────────────────────────────────────────────────────────────

struct Attr
{
    std::string name;
    std::string value;
    bool boolean = false; // true = render as name only (no ="value")
};

// ── Common attributes ──

inline Attr class_(const char* c)              { return {"class", c}; }
inline Attr class_(const std::string& c)       { return {"class", c}; }
inline Attr id(const char* i)                  { return {"id", i}; }
inline Attr id(const std::string& i)           { return {"id", i}; }
inline Attr href(const char* h)                { return {"href", h}; }
inline Attr href(const std::string& h)         { return {"href", h}; }
inline Attr src(const char* s)                 { return {"src", s}; }
inline Attr src(const std::string& s)          { return {"src", s}; }
inline Attr alt(const char* a)                 { return {"alt", a}; }
inline Attr alt(const std::string& a)          { return {"alt", a}; }
inline Attr type(const char* t)                { return {"type", t}; }
inline Attr rel(const char* r)                 { return {"rel", r}; }
inline Attr charset(const char* c)             { return {"charset", c}; }
inline Attr lang(const char* l)                { return {"lang", l}; }
inline Attr name(const char* n)                { return {"name", n}; }
inline Attr name(const std::string& n)         { return {"name", n}; }
inline Attr onclick(const char* c)             { return {"onclick", c}; }
inline Attr target(const char* t)              { return {"target", t}; }
inline Attr aria_label(const char* l)          { return {"aria-label", l}; }
inline Attr role(const char* r)                { return {"role", r}; }

// ── Generic & data attributes ──

inline Attr attr(const char* n, const char* v)          { return {n, v}; }
inline Attr attr(const char* n, const std::string& v)   { return {n, v}; }
inline Attr data(const char* key, const char* v)        { return {"data-" + std::string(key), v}; }
inline Attr data(const char* key, const std::string& v) { return {"data-" + std::string(key), v}; }

// ── Boolean attributes ──

inline Attr checked()  { return {"checked",  "", true}; }
inline Attr disabled() { return {"disabled", "", true}; }
inline Attr hidden()   { return {"hidden",   "", true}; }

// ── Conditional classes ──
//
//    classes({{"active", is_active}, {"sidebar", has_sidebar}})
//
inline Attr classes(std::initializer_list<std::pair<const char*, bool>> cls)
{
    std::string result;
    for (auto& [name, active] : cls)
    {
        if (active)
        {
            if (!result.empty()) result += ' ';
            result += name;
        }
    }
    return {"class", std::move(result)};
}

// ─────────────────────────────────────────────────────────────────────
//  Nodes
// ─────────────────────────────────────────────────────────────────────

struct Node
{
    enum Kind { Element, Void, Text, Raw, Fragment } kind = Fragment;
    std::string tag;
    std::vector<Attr> attrs;
    std::vector<Node> children;
    std::string content;

    // Render this node tree to an HTML string
    std::string render() const
    {
        std::string out;
        out.reserve(512);
        render_to(out);
        return out;
    }

    void render_to(std::string& out) const
    {
        switch (kind)
        {
            case Text:
                escape_text(out, content);
                break;
            case Raw:
                out += content;
                break;
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

private:
    void render_attrs(std::string& out) const
    {
        for (const auto& a : attrs)
        {
            out += ' ';
            out += a.name;
            if (!a.boolean)
            {
                out += "=\"";
                escape_attr(out, a.value);
                out += '"';
            }
        }
    }

    static void escape_text(std::string& out, const std::string& s)
    {
        for (char c : s)
            switch (c)
            {
                case '&': out += "&amp;";  break;
                case '<': out += "&lt;";   break;
                case '>': out += "&gt;";   break;
                default:  out += c;
            }
    }

    static void escape_attr(std::string& out, const std::string& s)
    {
        for (char c : s)
            switch (c)
            {
                case '"': out += "&quot;"; break;
                case '&': out += "&amp;";  break;
                case '<': out += "&lt;";   break;
                case '>': out += "&gt;";   break;
                default:  out += c;
            }
    }
};

// ── Node concatenation: a + b → fragment ──

inline Node operator+(Node a, Node b)
{
    if (a.kind == Node::Fragment)
    {
        a.children.push_back(std::move(b));
        return a;
    }
    Node f{Node::Fragment, {}, {}, {}, {}};
    f.children.push_back(std::move(a));
    f.children.push_back(std::move(b));
    return f;
}

// ─────────────────────────────────────────────────────────────────────
//  Dispatch — args are sorted into attrs or children at compile time
// ─────────────────────────────────────────────────────────────────────

namespace detail
{

inline void add(Node& n, Attr a)                  { n.attrs.push_back(std::move(a)); }
inline void add(Node& n, Node c)                  { n.children.push_back(std::move(c)); }
inline void add(Node& n, const std::string& s)    { n.children.push_back({Node::Text, {}, {}, {}, s}); }
inline void add(Node& n, const char* s)            { n.children.push_back({Node::Text, {}, {}, {}, s}); }
inline void add(Node& n, std::string&& s)          { n.children.push_back({Node::Text, {}, {}, {}, std::move(s)}); }
inline void add(Node& n, std::vector<Node> cs)     { for (auto& c : cs) n.children.push_back(std::move(c)); }
inline void add(Node& n, int v)                    { n.children.push_back({Node::Text, {}, {}, {}, std::to_string(v)}); }

} // namespace detail

// ─────────────────────────────────────────────────────────────────────
//  Element factories
// ─────────────────────────────────────────────────────────────────────

template<typename... Args>
Node elem(const char* tag, Args&&... args)
{
    Node n{Node::Element, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}

template<typename... Args>
Node void_elem(const char* tag, Args&&... args)
{
    Node n{Node::Void, tag, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}

// ── Standard elements ──

template<typename... A> Node html(A&&... a)        { return elem("html", std::forward<A>(a)...); }
template<typename... A> Node head(A&&... a)        { return elem("head", std::forward<A>(a)...); }
template<typename... A> Node body(A&&... a)        { return elem("body", std::forward<A>(a)...); }
template<typename... A> Node div(A&&... a)         { return elem("div", std::forward<A>(a)...); }
template<typename... A> Node span(A&&... a)        { return elem("span", std::forward<A>(a)...); }
template<typename... A> Node p_(A&&... a)          { return elem("p", std::forward<A>(a)...); }
template<typename... A> Node h1(A&&... a)          { return elem("h1", std::forward<A>(a)...); }
template<typename... A> Node h2(A&&... a)          { return elem("h2", std::forward<A>(a)...); }
template<typename... A> Node h3(A&&... a)          { return elem("h3", std::forward<A>(a)...); }
template<typename... A> Node h4(A&&... a)          { return elem("h4", std::forward<A>(a)...); }
template<typename... A> Node h5(A&&... a)          { return elem("h5", std::forward<A>(a)...); }
template<typename... A> Node h6(A&&... a)          { return elem("h6", std::forward<A>(a)...); }
template<typename... A> Node a(A&&... a_)          { return elem("a", std::forward<A>(a_)...); }
template<typename... A> Node article(A&&... a)     { return elem("article", std::forward<A>(a)...); }
template<typename... A> Node section(A&&... a)     { return elem("section", std::forward<A>(a)...); }
template<typename... A> Node nav(A&&... a)         { return elem("nav", std::forward<A>(a)...); }
template<typename... A> Node header(A&&... a)      { return elem("header", std::forward<A>(a)...); }
template<typename... A> Node footer(A&&... a)      { return elem("footer", std::forward<A>(a)...); }
template<typename... A> Node main_(A&&... a)       { return elem("main", std::forward<A>(a)...); }
template<typename... A> Node aside(A&&... a)       { return elem("aside", std::forward<A>(a)...); }
template<typename... A> Node ul(A&&... a)          { return elem("ul", std::forward<A>(a)...); }
template<typename... A> Node ol(A&&... a)          { return elem("ol", std::forward<A>(a)...); }
template<typename... A> Node li(A&&... a)          { return elem("li", std::forward<A>(a)...); }
template<typename... A> Node button(A&&... a)      { return elem("button", std::forward<A>(a)...); }
template<typename... A> Node time_(A&&... a)       { return elem("time", std::forward<A>(a)...); }
template<typename... A> Node strong(A&&... a)      { return elem("strong", std::forward<A>(a)...); }
template<typename... A> Node em_(A&&... a)         { return elem("em", std::forward<A>(a)...); }
template<typename... A> Node code(A&&... a)        { return elem("code", std::forward<A>(a)...); }
template<typename... A> Node pre(A&&... a)         { return elem("pre", std::forward<A>(a)...); }
template<typename... A> Node blockquote(A&&... a)  { return elem("blockquote", std::forward<A>(a)...); }
template<typename... A> Node table(A&&... a)       { return elem("table", std::forward<A>(a)...); }
template<typename... A> Node thead(A&&... a)       { return elem("thead", std::forward<A>(a)...); }
template<typename... A> Node tbody(A&&... a)       { return elem("tbody", std::forward<A>(a)...); }
template<typename... A> Node tr(A&&... a)          { return elem("tr", std::forward<A>(a)...); }
template<typename... A> Node th(A&&... a)          { return elem("th", std::forward<A>(a)...); }
template<typename... A> Node td(A&&... a)          { return elem("td", std::forward<A>(a)...); }
template<typename... A> Node style(A&&... a)       { return elem("style", std::forward<A>(a)...); }
template<typename... A> Node script(A&&... a)      { return elem("script", std::forward<A>(a)...); }
template<typename... A> Node title(A&&... a)       { return elem("title", std::forward<A>(a)...); }
template<typename... A> Node label(A&&... a)       { return elem("label", std::forward<A>(a)...); }
template<typename... A> Node form(A&&... a)        { return elem("form", std::forward<A>(a)...); }

// ── Void elements (self-closing) ──

template<typename... A> Node img(A&&... a)         { return void_elem("img", std::forward<A>(a)...); }
template<typename... A> Node br(A&&... a)          { return void_elem("br", std::forward<A>(a)...); }
template<typename... A> Node hr(A&&... a)          { return void_elem("hr", std::forward<A>(a)...); }
template<typename... A> Node meta(A&&... a)        { return void_elem("meta", std::forward<A>(a)...); }
template<typename... A> Node link(A&&... a)        { return void_elem("link", std::forward<A>(a)...); }
template<typename... A> Node input(A&&... a)       { return void_elem("input", std::forward<A>(a)...); }

// ─────────────────────────────────────────────────────────────────────
//  Special nodes
// ─────────────────────────────────────────────────────────────────────

// Raw HTML (not escaped — for pre-rendered content like markdown)
inline Node raw(const std::string& s)  { return {Node::Raw, {}, {}, {}, s}; }
inline Node raw(std::string&& s)       { return {Node::Raw, {}, {}, {}, std::move(s)}; }
inline Node raw(const char* s)         { return {Node::Raw, {}, {}, {}, s}; }

// Text (escaped)
inline Node text(const std::string& s) { return {Node::Text, {}, {}, {}, s}; }
inline Node text(std::string&& s)      { return {Node::Text, {}, {}, {}, std::move(s)}; }

// Fragment (multiple children, no wrapper element)
template<typename... Args>
Node fragment(Args&&... args)
{
    Node n{Node::Fragment, {}, {}, {}, {}};
    (detail::add(n, std::forward<Args>(args)), ...);
    return n;
}

// DOCTYPE
inline Node doctype() { return raw("<!DOCTYPE html>"); }

// Full HTML document
template<typename... Args>
Node document(Args&&... args)
{
    return fragment(doctype(), html(lang("en"), std::forward<Args>(args)...));
}

// ─────────────────────────────────────────────────────────────────────
//  Control flow
// ─────────────────────────────────────────────────────────────────────

// Conditional rendering: when(condition, node)
inline Node when(bool cond, Node n)
{
    return cond ? std::move(n) : Node{Node::Fragment, {}, {}, {}, {}};
}

// Lazy conditional: when(condition, []{ return node; })
template<typename Fn>
Node when(bool cond, Fn&& fn)
{
    return cond ? fn() : Node{Node::Fragment, {}, {}, {}, {}};
}

// Unless (inverse conditional)
inline Node unless(bool cond, Node n)
{
    return cond ? Node{Node::Fragment, {}, {}, {}, {}} : std::move(n);
}

// Iteration: each(collection, [](auto& item) { return node; })
template<typename Container, typename Fn>
Node each(const Container& items, Fn&& fn)
{
    Node n{Node::Fragment, {}, {}, {}, {}};
    n.children.reserve(items.size());
    for (const auto& item : items)
        n.children.push_back(fn(item));
    return n;
}

// Indexed iteration: each_i(collection, [](auto& item, int index) { return node; })
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

// ─────────────────────────────────────────────────────────────────────
//  Component abstraction
//
//  A Component is a function that takes data and returns a Node tree.
//  Themes override components to change HTML structure — not just CSS.
//
//    // Default post card
//    dom::Node default_card(const PostData& p) {
//        return article(class_("post-card"),
//            a(href(p.url), p.title),
//            span(class_("date"), p.date)
//        );
//    }
//
//    // Brutalist post card — completely different structure
//    dom::Node brutalist_card(const PostData& p) {
//        return article(class_("post-card"),
//            div(class_("card-idx"), p.index),
//            h3(a(href(p.url), p.title)),
//            div(class_("card-meta"), p.date)
//        );
//    }
//
// ─────────────────────────────────────────────────────────────────────

// Component = any callable that returns a Node
// Use std::function or function pointers in theme definitions.
// The dom:: namespace provides the building blocks; themes compose them.

} // namespace loom::dom
