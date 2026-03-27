#pragma once
// ═══════════════════════════════════════════════════════════════════════
//  css.hpp — compile-time type-safe CSS DSL for C++20
//
//  Themes read like stylesheets:
//
//    .styles = sheet(
//        "::selection"_s          | bg(hex("#0c4a6e")) | color(hex("#fafaf8")),
//        "::selection"_s.dark()   | bg(hex("#7dd3fc")) | color(hex("#0c0c0c")),
//
//        ".post-content"_s.also(".page-content").nest(
//            "a"_s         | color(v::accent),
//            "a"_s.hover() | text_decoration(underline),
//            "pre"_s       | border(1_px, solid, v::border)
//        ),
//
//        media(max_width(768_px),
//            ".sidebar"_s | display(none)
//        ),
//
//        keyframes("fade-in",
//            from() | opacity(0),
//            to()   | opacity(1.0)
//        )
//    );
//
// ═══════════════════════════════════════════════════════════════════════

#include <string>
#include <vector>

namespace loom::css
{

// ─────────────────────────────────────────────────────────────────────
//  Values
// ─────────────────────────────────────────────────────────────────────

struct Val { std::string v; };

// Space-separated compound: 3_px + solid + v::accent
inline Val operator+(Val a, const Val& b) { a.v += ' '; a.v += b.v; return a; }

// ── Constructors ──

inline Val px(int n) { return {std::to_string(n) + "px"}; }
inline Val px(double n) {
    auto s = std::to_string(n);
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s + "px"};
}
inline Val em(double n) {
    auto s = std::to_string(n);
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s + "em"};
}
inline Val pct(int n)         { return {std::to_string(n) + "%"}; }
inline Val hex(const char* c) { return {c}; }
inline Val raw(const char* s) { return {s}; }
inline Val str(const char* s) { return {"\"" + std::string(s) + "\""}; }
inline Val num(int n)         { return {std::to_string(n)}; }
inline Val num(double n) {
    auto s = std::to_string(n);
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s};
}

// ── User-defined literals ──

inline Val operator""_px(unsigned long long n)  { return {std::to_string(n) + "px"}; }
inline Val operator""_em(long double n) {
    auto s = std::to_string(static_cast<double>(n));
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s + "em"};
}
inline Val operator""_pct(unsigned long long n) { return {std::to_string(n) + "%"}; }

// ── Keywords ──

inline const Val solid{"solid"};
inline const Val dashed{"dashed"};
inline const Val dotted{"dotted"};
inline const Val none{"none"};
inline const Val transparent{"transparent"};
inline const Val uppercase{"uppercase"};
inline const Val lowercase{"lowercase"};
inline const Val italic{"italic"};
inline const Val normal{"normal"};
inline const Val inherit{"inherit"};
inline const Val hidden{"hidden"};
inline const Val underline{"underline"};
inline const Val block{"block"};
inline const Val center{"center"};

// ── CSS variable references ──

namespace v
{
    inline const Val bg{"var(--bg)"};
    inline const Val text{"var(--text)"};
    inline const Val accent{"var(--accent)"};
    inline const Val muted{"var(--muted)"};
    inline const Val border{"var(--border)"};
}

// ── Compound value helpers ──

inline Val mix(const Val& color, int p, const Val& base) {
    return {"color-mix(in srgb, " + color.v + " " + std::to_string(p) + "%, " + base.v + ")"};
}
inline Val gradient(const Val& from, const Val& to) {
    return {"linear-gradient(" + from.v + ", " + to.v + ")"};
}

// ─────────────────────────────────────────────────────────────────────
//  Declarations — property: value pairs
// ─────────────────────────────────────────────────────────────────────

struct Decl { std::string prop, val; };

inline Decl prop(const char* name, const Val& value) { return {name, value.v}; }
inline Decl set(const char* name, const Val& value)  { return {std::string("--") + name, value.v}; }

// ── Color ──
inline Decl color(const Val& c)           { return {"color", c.v}; }
inline Decl background(const Val& c)      { return {"background", c.v}; }
inline Decl bg(const Val& c)              { return {"background", c.v}; }
inline Decl background_size(const Val& v) { return {"background-size", v.v}; }

// ── Border ──
inline Decl border(const Val& w, const Val& s, const Val& c) { return {"border", w.v + " " + s.v + " " + c.v}; }
inline Decl border(const Val& v)              { return {"border", v.v}; }
inline Decl border_left(const Val& w, const Val& s, const Val& c) { return {"border-left", w.v + " " + s.v + " " + c.v}; }
inline Decl border_left(const Val& v)         { return {"border-left", v.v}; }
inline Decl border_right(const Val& w, const Val& s, const Val& c) { return {"border-right", w.v + " " + s.v + " " + c.v}; }
inline Decl border_top(const Val& w, const Val& s, const Val& c) { return {"border-top", w.v + " " + s.v + " " + c.v}; }
inline Decl border_bottom(const Val& w, const Val& s, const Val& c) { return {"border-bottom", w.v + " " + s.v + " " + c.v}; }
inline Decl border_color(const Val& c)        { return {"border-color", c.v}; }
inline Decl border_left_color(const Val& c)   { return {"border-left-color", c.v}; }
inline Decl border_width(const Val& w)        { return {"border-width", w.v}; }
inline Decl border_left_width(const Val& w)   { return {"border-left-width", w.v}; }
inline Decl border_top_width(const Val& w)    { return {"border-top-width", w.v}; }
inline Decl border_bottom_width(const Val& w) { return {"border-bottom-width", w.v}; }
inline Decl border_style(const Val& s)        { return {"border-style", s.v}; }
inline Decl border_radius(const Val& r)       { return {"border-radius", r.v}; }

// ── Spacing ──
inline Decl padding(const Val& v)                    { return {"padding", v.v}; }
inline Decl padding(const Val& y, const Val& x)      { return {"padding", y.v + " " + x.v}; }
inline Decl padding_top(const Val& v)                { return {"padding-top", v.v}; }
inline Decl padding_bottom(const Val& v)             { return {"padding-bottom", v.v}; }
inline Decl padding_left(const Val& v)               { return {"padding-left", v.v}; }
inline Decl padding_right(const Val& v)              { return {"padding-right", v.v}; }
inline Decl margin(const Val& v)                     { return {"margin", v.v}; }
inline Decl margin(const Val& y, const Val& x)       { return {"margin", y.v + " " + x.v}; }
inline Decl margin_top(const Val& v)                 { return {"margin-top", v.v}; }
inline Decl margin_bottom(const Val& v)              { return {"margin-bottom", v.v}; }
inline Decl margin_left(const Val& v)                { return {"margin-left", v.v}; }
inline Decl margin_right(const Val& v)               { return {"margin-right", v.v}; }

// ── Typography ──
inline Decl font_size(const Val& v)            { return {"font-size", v.v}; }
inline Decl font_weight(int w)                 { return {"font-weight", std::to_string(w)}; }
inline Decl font_weight(const Val& v)          { return {"font-weight", v.v}; }
inline Decl font_style(const Val& v)           { return {"font-style", v.v}; }
inline Decl text_transform(const Val& v)       { return {"text-transform", v.v}; }
inline Decl text_decoration(const Val& v)      { return {"text-decoration", v.v}; }
inline Decl text_decoration_style(const Val& v){ return {"text-decoration-style", v.v}; }
inline Decl text_underline_offset(const Val& v){ return {"text-underline-offset", v.v}; }
inline Decl letter_spacing(const Val& v)       { return {"letter-spacing", v.v}; }
inline Decl line_height(const Val& v)          { return {"line-height", v.v}; }

// ── Visual ──
inline Decl opacity(double o) {
    auto s = std::to_string(o);
    s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {"opacity", s};
}
inline Decl box_shadow(const Val& v)  { return {"box-shadow", v.v}; }
inline Decl transition(const Val& v)  { return {"transition", v.v}; }

// ── Layout ──
inline Decl display(const Val& v)   { return {"display", v.v}; }
inline Decl width(const Val& v)     { return {"width", v.v}; }
inline Decl max_width(const Val& v) { return {"max-width", v.v}; }
inline Decl gap(const Val& v)       { return {"gap", v.v}; }
inline Decl order(const Val& v)     { return {"order", v.v}; }
inline Decl content(const Val& v)   { return {"content", v.v}; }

// ─────────────────────────────────────────────────────────────────────
//  Rules
// ─────────────────────────────────────────────────────────────────────

struct Rule
{
    std::string selector;
    std::vector<Decl> decls;
};

inline Rule operator|(Rule&& r, Decl d)
{
    r.decls.push_back(std::move(d));
    return std::move(r);
}

// ─────────────────────────────────────────────────────────────────────
//  Selectors — the expressive core
//
//    ".card"_s               → .card
//    ".card"_s.hover()       → .card:hover
//    ".card"_s.dark()        → [data-theme="dark"] .card
//    ".card"_s.dark().hover()→ [data-theme="dark"] .card:hover
//    ".x"_s.also(".y")       → .x,.y
//    ".x"_s.also(".y").nest( → .x a,.y a { ... }
//        "a"_s | color(...)  →
//    )                       →
//
// ─────────────────────────────────────────────────────────────────────

struct Nest;  // forward

struct Sel
{
    std::string s;

    // ── Pseudo-classes & pseudo-elements ──
    Sel hover()       const { return {s + ":hover"}; }
    Sel focus()       const { return {s + ":focus"}; }
    Sel active()      const { return {s + ":active"}; }
    Sel visited()     const { return {s + ":visited"}; }
    Sel first_child() const { return {s + ":first-child"}; }
    Sel last_child()  const { return {s + ":last-child"}; }
    Sel even()        const { return {s + ":nth-child(even)"}; }
    Sel odd()         const { return {s + ":nth-child(odd)"}; }
    Sel before()      const { return {s + "::before"}; }
    Sel after()       const { return {s + "::after"}; }
    Sel placeholder() const { return {s + "::placeholder"}; }

    // ── Comma-join: ".a"_s.also(".b") → ".a,.b" ──
    Sel also(const char* other) const { return {s + "," + other}; }

    // ── Dark mode: prefix each comma-part with [data-theme="dark"] ──
    Sel dark() const
    {
        std::string result;
        size_t pos = 0;
        while (pos < s.size())
        {
            size_t comma = s.find(',', pos);
            if (comma == std::string::npos) comma = s.size();
            if (!result.empty()) result += ',';
            result += "[data-theme=\"dark\"] ";
            size_t start = pos;
            while (start < comma && s[start] == ' ') ++start;
            result.append(s, start, comma - start);
            pos = comma + 1;
        }
        return {std::move(result)};
    }

    // ── Nesting: parent.nest(child | ..., child | ...) ──
    template<typename... Rs>
    Nest nest(Rs&&... rules) const;
};

// ── Selector literal: "selector"_s ──
inline Sel operator""_s(const char* str, size_t) { return {str}; }

// Sel | Decl → starts a Rule
inline Rule operator|(Sel sel, Decl d)
{
    return {std::move(sel.s), {std::move(d)}};
}

// Keep legacy sel() for compat
inline Sel sel(const char* s) { return {s}; }
inline Sel sel(const char* a, const char* b) { return {std::string(a) + "," + b}; }
inline Sel sel(const char* a, const char* b, const char* c) { return {std::string(a) + "," + b + "," + c}; }

// :root and [data-theme="dark"] for variable blocks
inline Sel root()      { return {":root"}; }
inline Sel dark_root() { return {"[data-theme=\"dark\"]"}; }

// ─────────────────────────────────────────────────────────────────────
//  Nesting — SCSS-like parent/child expansion
//
//    ".content"_s.also(".page").nest(
//        "a"_s         | color(v::accent),
//        "a"_s.hover() | text_decoration(underline)
//    )
//    → .content a,.page a { color: var(--accent); }
//    → .content a:hover,.page a:hover { text-decoration: underline; }
//
// ─────────────────────────────────────────────────────────────────────

struct Nest
{
    std::string parent;
    std::vector<Rule> children;
};

template<typename... Rs>
Nest Sel::nest(Rs&&... rules) const
{
    Nest n;
    n.parent = s;
    (n.children.push_back(std::forward<Rs>(rules)), ...);
    return n;
}

// ─────────────────────────────────────────────────────────────────────
//  Media queries
//
//    media("max-width:768px",
//        ".sidebar"_s | display(none)
//    )
//
//    media(max_width(768_px),
//        ".sidebar"_s | display(none)
//    )
//
// ─────────────────────────────────────────────────────────────────────

struct MediaBlock
{
    std::string condition;
    std::vector<Rule> rules;
};

inline std::string max_w(const Val& v) { return "max-width:" + v.v; }
inline std::string min_w(const Val& v) { return "min-width:" + v.v; }

template<typename... Rs>
MediaBlock media(const std::string& condition, Rs&&... rules)
{
    MediaBlock m;
    m.condition = condition;
    (m.rules.push_back(std::forward<Rs>(rules)), ...);
    return m;
}

// ─────────────────────────────────────────────────────────────────────
//  Keyframes
//
//    keyframes("fade-in",
//        from() | opacity(0),
//        to()   | opacity(1.0)
//    )
//
// ─────────────────────────────────────────────────────────────────────

struct Frame
{
    std::string position;
    std::vector<Decl> decls;
};

inline Frame operator|(Frame&& f, Decl d)
{
    f.decls.push_back(std::move(d));
    return std::move(f);
}

inline Frame frame(const Val& pos) { return {pos.v, {}}; }
inline Frame from()                { return {"from", {}}; }
inline Frame to()                  { return {"to", {}}; }

struct KeyframeBlock
{
    std::string name;
    std::vector<Frame> frames;
};

template<typename... Fs>
KeyframeBlock keyframes(const char* name, Fs&&... frames)
{
    KeyframeBlock k;
    k.name = name;
    (k.frames.push_back(std::forward<Fs>(frames)), ...);
    return k;
}

// ─────────────────────────────────────────────────────────────────────
//  Sheet — collects rules, nests, media, keyframes → compiles to CSS
// ─────────────────────────────────────────────────────────────────────

struct Sheet
{
    std::vector<Rule> rules;
    std::vector<std::string> blocks;

    bool empty() const { return rules.empty() && blocks.empty(); }

    std::string compile() const
    {
        std::string css;
        for (const auto& r : rules)
        {
            css += r.selector;
            css += '{';
            for (const auto& d : r.decls)
            {
                css += d.prop;
                css += ':';
                css += d.val;
                css += ';';
            }
            css += '}';
        }
        for (const auto& b : blocks) css += b;
        return css;
    }
};

inline Sheet operator+(Sheet a, Sheet b)
{
    for (auto& r : b.rules)  a.rules.push_back(std::move(r));
    for (auto& b2 : b.blocks) a.blocks.push_back(std::move(b2));
    return a;
}

// ── Flatten dispatch: each type knows how to add itself to a Sheet ──

namespace detail
{

inline void flatten(Sheet& s, Rule&& r)
{
    s.rules.push_back(std::move(r));
}

inline void flatten(Sheet& s, Nest&& n)
{
    for (auto& child : n.children)
    {
        std::string expanded;
        size_t pos = 0;
        while (pos < n.parent.size())
        {
            size_t comma = n.parent.find(',', pos);
            if (comma == std::string::npos) comma = n.parent.size();
            if (!expanded.empty()) expanded += ',';
            size_t start = pos;
            while (start < comma && n.parent[start] == ' ') ++start;
            expanded.append(n.parent, start, comma - start);
            expanded += ' ';
            expanded += child.selector;
            pos = comma + 1;
        }
        child.selector = std::move(expanded);
        s.rules.push_back(std::move(child));
    }
}

inline void flatten(Sheet& s, MediaBlock&& m)
{
    std::string css = "@media(" + m.condition + "){";
    for (const auto& r : m.rules)
    {
        css += r.selector;
        css += '{';
        for (const auto& d : r.decls)
        {
            css += d.prop;
            css += ':';
            css += d.val;
            css += ';';
        }
        css += '}';
    }
    css += '}';
    s.blocks.push_back(std::move(css));
}

inline void flatten(Sheet& s, KeyframeBlock&& k)
{
    std::string css = "@keyframes " + k.name + "{";
    for (const auto& f : k.frames)
    {
        css += f.position;
        css += '{';
        for (const auto& d : f.decls)
        {
            css += d.prop;
            css += ':';
            css += d.val;
            css += ';';
        }
        css += '}';
    }
    css += '}';
    s.blocks.push_back(std::move(css));
}

} // namespace detail

template<typename... Args>
Sheet sheet(Args&&... args)
{
    Sheet s;
    (detail::flatten(s, std::forward<Args>(args)), ...);
    return s;
}

} // namespace loom::css
