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

// --- Typography emission ---

inline void emit_typography(std::string& css, const ThemeDef& t)
{
    if (!t.heading_font.value.empty())
    {
        css += "--heading-font:";
        css += t.heading_font.value;
        css += ';';
    }
    if (!t.code_font.value.empty())
    {
        css += "--code-font:";
        css += t.code_font.value;
        css += ';';
    }
    if (!t.line_height.empty())
    {
        css += "--line-height:";
        css += t.line_height;
        css += ';';
    }
}

// --- Structural emission: each enum -> CSS rules ---

inline void emit_corners(std::string& css, Corners c)
{
    switch (c)
    {
        case Corners::Soft: break;
        case Corners::Sharp:
            css += ":root{--border-radius:0;--card-radius:0;--tag-radius:0;}";
            break;
        case Corners::Round:
            css += ":root{--border-radius:12px;--card-radius:16px;--tag-radius:999px;}";
            break;
    }
}

inline void emit_density(std::string& css, Density d)
{
    switch (d)
    {
        case Density::Normal: break;
        case Density::Compact:
            css += ":root{--line-height:1.5;--container-padding:28px 16px;}";
            css += ".post-content p,.page-content p{margin-bottom:10px;}";
            css += ".post-content h2,.page-content h2{margin-top:22px;margin-bottom:8px;}";
            css += ".post-content h3,.page-content h3{margin-top:18px;margin-bottom:6px;}";
            css += ".post-listing{padding:10px 10px;}";
            css += ".widget{margin-bottom:18px;}";
            break;
        case Density::Airy:
            css += ":root{--line-height:1.85;--container-padding:48px 24px;}";
            css += ".post-content p,.page-content p{margin-bottom:1.2em;}";
            css += ".post-content h2,.page-content h2{margin-top:36px;margin-bottom:14px;}";
            css += ".post-content h3,.page-content h3{margin-top:28px;margin-bottom:10px;}";
            css += ".post-listing{padding:18px 10px;}";
            css += ".widget{margin-bottom:30px;}";
            break;
    }
}

inline void emit_tag_style(std::string& css, TagStyle s)
{
    switch (s)
    {
        case TagStyle::Pill: break;
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
        case LinkStyle::Underline: break;
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
        case CodeBlockStyle::Plain: break;
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
        case BlockquoteStyle::AccentBorder: break;
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

inline void emit_image_style(std::string& css, ImageStyle s)
{
    switch (s)
    {
        case ImageStyle::Default: break;
        case ImageStyle::Bordered:
            css += ".post-content img,.page-content img"
                   "{border:1px solid var(--border);}";
            break;
        case ImageStyle::Shadow:
            css += ".post-content img,.page-content img"
                   "{box-shadow:0 2px 8px color-mix(in srgb, var(--text) 10%, transparent);}";
            break;
    }
}

inline void emit_card_hover(std::string& css, CardHover h)
{
    switch (h)
    {
        case CardHover::Lift: break; // base CSS default
        case CardHover::Border:
            css += ":root{--card-hover-shadow:none;--card-hover-lift:none;}";
            break;
        case CardHover::Glow:
            css += ":root{--card-hover-lift:none;"
                   "--card-hover-shadow:0 0 20px color-mix(in srgb, var(--accent) 12%, transparent);}";
            break;
        case CardHover::None:
            css += ":root{--card-hover-shadow:none;--card-hover-lift:none;}";
            css += ".post-card:hover{border-color:var(--card-border);}";
            break;
    }
}

inline void emit_hr_style(std::string& css, HrStyle s)
{
    switch (s)
    {
        case HrStyle::Line: break; // base CSS default
        case HrStyle::Dashed:
            css += ".post-content hr,.page-content hr{border-top-style:dashed;}";
            break;
        case HrStyle::Fade:
            css += ".post-content hr,.page-content hr"
                   "{border:none;height:1px;"
                   "background:linear-gradient(to right,transparent,var(--border),transparent);}";
            break;
    }
}

inline void emit_table_style(std::string& css, TableStyle s)
{
    switch (s)
    {
        case TableStyle::Default: break;
        case TableStyle::Striped:
            css += ".post-content tr:nth-child(even),.page-content tr:nth-child(even)"
                   "{background:color-mix(in srgb, var(--text) 4%, var(--bg));}";
            break;
        case TableStyle::Bordered:
            css += ".post-content th,.post-content td,"
                   ".page-content th,.page-content td"
                   "{border:2px solid var(--border);}";
            css += ".post-content th,.page-content th"
                   "{font-weight:700;text-transform:uppercase;font-size:12px;letter-spacing:0.5px;}";
            break;
        case TableStyle::Minimal:
            css += ".post-content th,.page-content th"
                   "{background:none;border:none;border-bottom:2px solid var(--border);"
                   "font-weight:600;padding-left:0;}";
            css += ".post-content td,.page-content td"
                   "{border:none;border-bottom:1px solid var(--border);padding-left:0;}";
            break;
    }
}

} // namespace detail

// Theme -> CSS compiler
inline std::string compile(const ThemeDef& t)
{
    std::string css;

    // Light mode: colors + typography
    css += ":root{";
    detail::emit_palette(css, t.light, detail::ColorBindings{});
    css += Font::var;     css += ':'; css += t.font.value;  css += ';';
    css += FontSize::var; css += ':'; css += t.font_size;   css += ';';
    css += MaxWidth::var; css += ':'; css += t.max_width;   css += ';';
    detail::emit_typography(css, t);
    css += '}';

    // Dark mode: colors only
    css += "[data-theme=\"dark\"]{";
    detail::emit_palette(css, t.dark, detail::ColorBindings{});
    css += '}';

    // Structural rules
    detail::emit_corners(css, t.corners);
    detail::emit_density(css, t.density);
    detail::emit_tag_style(css, t.tag_style);
    detail::emit_link_style(css, t.link_style);
    detail::emit_code_style(css, t.code_style);
    detail::emit_blockquote_style(css, t.quote_style);
    detail::emit_heading_case(css, t.heading_case);
    detail::emit_image_style(css, t.image_style);
    detail::emit_card_hover(css, t.card_hover);
    detail::emit_hr_style(css, t.hr_style);
    detail::emit_table_style(css, t.table_style);

    // Escape hatch
    if (!t.extra_css.empty())
        css += t.extra_css;

    return css;
}

} // namespace loom::theme
