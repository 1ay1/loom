#pragma once

#include <string>
#include <tuple>
#include "tokens.hpp"
#include "palette.hpp"
#include "components.hpp"
#include "theme_def.hpp"

namespace loom::theme
{

namespace detail
{

// --- Color emission via fold expressions ---

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

using ColorBindings = std::tuple<
    ColorBinding<Bg,     &Palette::bg>,
    ColorBinding<Text,   &Palette::text>,
    ColorBinding<Muted,  &Palette::muted>,
    ColorBinding<Border, &Palette::border>,
    ColorBinding<Accent, &Palette::accent>
>;

template<typename... Bs>
void emit_palette(std::string& css, const Palette& p, std::tuple<Bs...>)
{
    (Bs::emit(css, p), ...);
}

// --- Structural emission: each enum -> CSS rules ---

inline void emit_corners(std::string& css, Corners c)
{
    switch (c)
    {
        case Corners::Soft: break; // base CSS defaults
        case Corners::Sharp:
            css += ":root{--border-radius:0;--card-radius:0;--tag-radius:0;}";
            break;
        case Corners::Round:
            css += ":root{--border-radius:12px;--card-radius:16px;--tag-radius:999px;}";
            break;
    }
}

inline void emit_tag_style(std::string& css, TagStyle s)
{
    switch (s)
    {
        case TagStyle::Pill: break; // base CSS defaults
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

inline void emit_link_style(std::string& css, LinkStyle s)
{
    switch (s)
    {
        case LinkStyle::Underline: break; // base CSS defaults
        case LinkStyle::Dotted:
            css += ".post-content a,.page-content a"
                   "{text-decoration-style:dotted;text-underline-offset:3px;}";
            css += ".post-content a:hover,.page-content a:hover"
                   "{text-decoration-style:solid;}";
            break;
        case LinkStyle::Dashed:
            css += ".post-content a,.page-content a"
                   "{text-decoration-style:dashed;text-underline-offset:3px;}";
            break;
        case LinkStyle::None:
            css += ":root{--link-decoration:none;}";
            break;
    }
}

inline void emit_code_style(std::string& css, CodeBlockStyle s)
{
    switch (s)
    {
        case CodeBlockStyle::Plain: break; // base CSS defaults
        case CodeBlockStyle::Bordered:
            css += ".post-content pre,.page-content pre"
                   "{border:1px solid var(--border);}";
            break;
        case CodeBlockStyle::LeftAccent:
            css += ".post-content pre,.page-content pre"
                   "{border:1px solid var(--border);border-left:3px solid var(--accent);}";
            break;
    }
}

inline void emit_blockquote_style(std::string& css, BlockquoteStyle s)
{
    switch (s)
    {
        case BlockquoteStyle::AccentBorder: break; // base CSS defaults
        case BlockquoteStyle::MutedBorder:
            css += ".post-content blockquote,.page-content blockquote"
                   "{border-left-color:var(--muted);}";
            break;
    }
}

inline void emit_heading_case(std::string& css, HeadingCase c)
{
    switch (c)
    {
        case HeadingCase::None: break;
        case HeadingCase::Upper:
            css += ".post-full h1,header h1{text-transform:uppercase;letter-spacing:2px;}";
            break;
        case HeadingCase::Lower:
            css += ".post-full h1,header h1{text-transform:lowercase;}";
            break;
    }
}

} // namespace detail

// Theme -> CSS compiler
// Produces: :root{colors+typo} [data-theme="dark"]{colors} structural-rules extra_css
inline std::string compile(const ThemeDef& t)
{
    std::string css;

    // Light mode: colors + typography
    css += ":root{";
    detail::emit_palette(css, t.light, detail::ColorBindings{});
    css += Font::var;     css += ':'; css += t.font.value;  css += ';';
    css += FontSize::var; css += ':'; css += t.font_size;   css += ';';
    css += MaxWidth::var; css += ':'; css += t.max_width;   css += ';';
    css += '}';

    // Dark mode: colors only
    css += "[data-theme=\"dark\"]{";
    detail::emit_palette(css, t.dark, detail::ColorBindings{});
    css += '}';

    // Structural rules
    detail::emit_corners(css, t.corners);
    detail::emit_tag_style(css, t.tag_style);
    detail::emit_link_style(css, t.link_style);
    detail::emit_code_style(css, t.code_style);
    detail::emit_blockquote_style(css, t.quote_style);
    detail::emit_heading_case(css, t.heading_case);

    // Escape hatch
    if (!t.extra_css.empty())
        css += t.extra_css;

    return css;
}

} // namespace loom::theme
