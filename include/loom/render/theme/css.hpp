#pragma once
//
// css.hpp — compile-time type-safe CSS DSL
//
// Usage:
//   using namespace loom::css;
//
//   auto styles = sheet(
//       sel("::selection") | bg(hex("#0c4a6e")) | color(hex("#fafaf8")),
//       dark(sel("::selection")) | bg(hex("#7dd3fc")) | color(hex("#0c0c0c")),
//       sel(".tag") | border(px(1), solid, v::accent) | bg(transparent)
//   );
//
//   std::string css_text = styles.compile();
//

#include <string>
#include <vector>

namespace loom::css
{

// ═══════════════════════════════════════════════════════════
//  Values
// ═══════════════════════════════════════════════════════════

struct Val { std::string v; };

// Space-separated compound: px(3) + solid + v::accent → "3px solid var(--accent)"
inline Val operator+(Val a, const Val& b) { a.v += ' '; a.v += b.v; return a; }

// ── Constructors ──

inline Val px(int n)     { return {std::to_string(n) + "px"}; }
inline Val px(double n) {
    auto s = std::to_string(n); s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s + "px"};
}
inline Val em(double n) {
    auto s = std::to_string(n); s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s + "em"};
}
inline Val pct(int n)    { return {std::to_string(n) + "%"}; }
inline Val hex(const char* c) { return {c}; }
inline Val raw(const char* s) { return {s}; }
inline Val str(const char* s) { return {"\"" + std::string(s) + "\""}; }
inline Val num(int n)    { return {std::to_string(n)}; }
inline Val num(double n) {
    auto s = std::to_string(n); s.erase(s.find_last_not_of('0') + 1);
    if (s.back() == '.') s.pop_back();
    return {s};
}

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

inline Val mix(const Val& color, int pct_val, const Val& base) {
    return {"color-mix(in srgb, " + color.v + " " + std::to_string(pct_val) + "%, " + base.v + ")"};
}

inline Val gradient(const Val& from, const Val& to) {
    return {"linear-gradient(" + from.v + ", " + to.v + ")"};
}

// ═══════════════════════════════════════════════════════════
//  Declarations — property: value pairs
// ═══════════════════════════════════════════════════════════

struct Decl { std::string prop, val; };

// Generic property (escape hatch for uncommon properties)
inline Decl prop(const char* name, const Val& value) { return {name, value.v}; }

// CSS variable setter — for :root / [data-theme="dark"] blocks
inline Decl set(const char* name, const Val& value) { return {std::string("--") + name, value.v}; }

// ── Color ──
inline Decl color(const Val& c)      { return {"color", c.v}; }
inline Decl background(const Val& c) { return {"background", c.v}; }
inline Decl bg(const Val& c)         { return {"background", c.v}; }
inline Decl background_size(const Val& v) { return {"background-size", v.v}; }

// ── Border shorthands ──
inline Decl border(const Val& w, const Val& s, const Val& c) { return {"border", w.v + " " + s.v + " " + c.v}; }
inline Decl border(const Val& v)     { return {"border", v.v}; }
inline Decl border_left(const Val& w, const Val& s, const Val& c) { return {"border-left", w.v + " " + s.v + " " + c.v}; }
inline Decl border_left(const Val& v) { return {"border-left", v.v}; }
inline Decl border_right(const Val& w, const Val& s, const Val& c) { return {"border-right", w.v + " " + s.v + " " + c.v}; }
inline Decl border_top(const Val& w, const Val& s, const Val& c) { return {"border-top", w.v + " " + s.v + " " + c.v}; }
inline Decl border_bottom(const Val& w, const Val& s, const Val& c) { return {"border-bottom", w.v + " " + s.v + " " + c.v}; }

// ── Border properties ──
inline Decl border_color(const Val& c)      { return {"border-color", c.v}; }
inline Decl border_left_color(const Val& c)  { return {"border-left-color", c.v}; }
inline Decl border_width(const Val& w)       { return {"border-width", w.v}; }
inline Decl border_left_width(const Val& w)  { return {"border-left-width", w.v}; }
inline Decl border_top_width(const Val& w)   { return {"border-top-width", w.v}; }
inline Decl border_bottom_width(const Val& w){ return {"border-bottom-width", w.v}; }
inline Decl border_style(const Val& s)       { return {"border-style", s.v}; }
inline Decl border_radius(const Val& r)      { return {"border-radius", r.v}; }

// ── Spacing ──
inline Decl padding(const Val& v)        { return {"padding", v.v}; }
inline Decl padding(const Val& y, const Val& x) { return {"padding", y.v + " " + x.v}; }
inline Decl padding_top(const Val& v)    { return {"padding-top", v.v}; }
inline Decl padding_bottom(const Val& v) { return {"padding-bottom", v.v}; }
inline Decl padding_left(const Val& v)   { return {"padding-left", v.v}; }
inline Decl padding_right(const Val& v)  { return {"padding-right", v.v}; }
inline Decl margin(const Val& v)         { return {"margin", v.v}; }
inline Decl margin_top(const Val& v)     { return {"margin-top", v.v}; }
inline Decl margin_bottom(const Val& v)  { return {"margin-bottom", v.v}; }
inline Decl margin_left(const Val& v)    { return {"margin-left", v.v}; }
inline Decl margin_right(const Val& v)   { return {"margin-right", v.v}; }

// ── Typography ──
inline Decl font_size(const Val& v)        { return {"font-size", v.v}; }
inline Decl font_weight(int w)             { return {"font-weight", std::to_string(w)}; }
inline Decl font_weight(const Val& v)      { return {"font-weight", v.v}; }
inline Decl font_style(const Val& v)       { return {"font-style", v.v}; }
inline Decl text_transform(const Val& v)   { return {"text-transform", v.v}; }
inline Decl text_decoration(const Val& v)  { return {"text-decoration", v.v}; }
inline Decl text_decoration_style(const Val& v) { return {"text-decoration-style", v.v}; }
inline Decl text_underline_offset(const Val& v) { return {"text-underline-offset", v.v}; }
inline Decl letter_spacing(const Val& v)   { return {"letter-spacing", v.v}; }
inline Decl line_height(const Val& v)      { return {"line-height", v.v}; }

// ── Visual ──
inline Decl opacity(double o) {
    auto s = std::to_string(o); s.erase(s.find_last_not_of('0') + 1);
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

// ── Pseudo content ──
inline Decl content(const Val& v) { return {"content", v.v}; }

// ═══════════════════════════════════════════════════════════
//  Rules — selector { decl; decl; ... }
// ═══════════════════════════════════════════════════════════

struct Rule
{
    std::string selector;
    std::vector<Decl> decls;
};

// Pipe operator: sel(".foo") | color(hex("#fff")) | bg(hex("#000"))
inline Rule operator|(Rule&& r, Decl d)
{
    r.decls.push_back(std::move(d));
    return std::move(r);
}

// ── Selector constructors ──

inline Rule sel(const char* s) { return {s, {}}; }

inline Rule sel(const char* a, const char* b)
{
    return {std::string(a) + "," + b, {}};
}

inline Rule sel(const char* a, const char* b, const char* c)
{
    return {std::string(a) + "," + b + "," + c, {}};
}

inline Rule sel(const char* a, const char* b, const char* c, const char* d)
{
    return {std::string(a) + "," + b + "," + c + "," + d, {}};
}

// Dark mode: prefixes each part of a comma-separated selector
inline Rule dark(Rule r)
{
    std::string result;
    std::string& s = r.selector;
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
    r.selector = std::move(result);
    return r;
}

// :root and [data-theme="dark"] for variable blocks
inline Rule root()      { return {":root", {}}; }
inline Rule dark_root() { return {"[data-theme=\"dark\"]", {}}; }

// ═══════════════════════════════════════════════════════════
//  Stylesheet — compile to CSS string
// ═══════════════════════════════════════════════════════════

struct Sheet
{
    std::vector<Rule> rules;

    bool empty() const { return rules.empty(); }

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
        return css;
    }
};

// Combine two sheets
inline Sheet operator+(Sheet a, const Sheet& b)
{
    a.rules.insert(a.rules.end(), b.rules.begin(), b.rules.end());
    return a;
}

template<typename... Rs>
Sheet sheet(Rs&&... rules)
{
    Sheet s;
    s.rules.reserve(sizeof...(Rs));
    (s.rules.push_back(std::forward<Rs>(rules)), ...);
    return s;
}

} // namespace loom::css
