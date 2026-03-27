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

// ── Color emission via fold expressions ──

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

// ── Typography ──

inline void emit_typography(std::string& css, const ThemeDef& t)
{
    if (!t.heading_font.value.empty())
        { css += "--heading-font:"; css += t.heading_font.value; css += ';'; }
    if (!t.code_font.value.empty())
        { css += "--code-font:"; css += t.code_font.value; css += ';'; }
    if (!t.line_height.empty())
        { css += "--line-height:"; css += t.line_height; css += ';'; }
    if (!t.heading_weight.empty())
        { css += "--heading-weight:"; css += t.heading_weight; css += ';'; }
    if (!t.header_size.empty())
        { css += "--header-size:"; css += t.header_size; css += ';'; }
}

// ── Structural emitters ──

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

inline void emit_border_weight(std::string& css, BorderWeight w)
{
    switch (w)
    {
        case BorderWeight::Normal: break;
        case BorderWeight::Thin:
            css += ":root{--header-border-width:0;--footer-border-width:0;}";
            css += ".sidebar{border-left-width:0;}.sidebar-left .sidebar{border-right-width:0;}";
            css += ".post-card{border-width:0;}";
            break;
        case BorderWeight::Thick:
            css += ":root{--header-border-width:3px;--footer-border-width:3px;}";
            css += ".sidebar{border-left-width:3px;}.sidebar-left .sidebar{border-right-width:3px;}";
            css += ".post-card{border-width:2px;}";
            css += ".series-nav{border-width:2px;}";
            break;
    }
}

inline void emit_nav_style(std::string& css, NavStyle s)
{
    switch (s)
    {
        case NavStyle::Default: break;
        case NavStyle::Pills:
            css += "nav a{padding:4px 12px;border-radius:var(--border-radius);"
                   "transition:background 0.15s,color 0.15s;}";
            css += "nav a:hover{background:color-mix(in srgb, var(--accent) 12%, var(--bg));"
                   "color:var(--accent);}";
            break;
        case NavStyle::Underline:
            css += "nav a{padding-bottom:4px;border-bottom:2px solid transparent;"
                   "transition:border-color 0.15s,color 0.15s;}";
            css += "nav a:hover{border-bottom-color:var(--accent);color:var(--accent);}";
            break;
        case NavStyle::Minimal:
            css += "nav a{font-size:13px;letter-spacing:0.5px;text-transform:uppercase;}";
            css += ":root{--nav-gap:14px;}";
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

inline void emit_inline_code(std::string& css, InlineCodeStyle s)
{
    switch (s)
    {
        case InlineCodeStyle::Background: break; // base CSS default
        case InlineCodeStyle::Bordered:
            css += ".post-content :not(pre)>code,.page-content :not(pre)>code"
                   "{border:1px solid var(--border);}";
            break;
        case InlineCodeStyle::Plain:
            css += ".post-content :not(pre)>code,.page-content :not(pre)>code"
                   "{background:none;padding:0;}";
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
        case CardHover::Lift: break;
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
        case HrStyle::Line: break;
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

inline void emit_sidebar_style(std::string& css, SidebarStyle s)
{
    switch (s)
    {
        case SidebarStyle::Bordered: break; // base CSS default
        case SidebarStyle::Clean:
            css += ".sidebar{border-left:none;padding-left:0;}";
            css += ".sidebar-left .sidebar{border-right:none;padding-right:0;}";
            break;
        case SidebarStyle::Card:
            css += ".sidebar{border-left:none;padding-left:0;}";
            css += ".sidebar-left .sidebar{border-right:none;padding-right:0;}";
            css += ".widget{border:1px solid var(--border);border-radius:var(--border-radius);"
                   "padding:16px;background:var(--card-bg);}";
            break;
    }
}

inline void emit_post_nav(std::string& css, PostNavStyle s)
{
    switch (s)
    {
        case PostNavStyle::Default: break;
        case PostNavStyle::Arrows:
            css += ".post-nav a{font-size:15px;font-weight:600;color:var(--text);}";
            css += ".post-nav a:hover{color:var(--accent);}";
            css += ".post-nav-prev::before{content:'\\2190  ';opacity:0.5;}";
            css += ".post-nav-next::after{content:'  \\2192';opacity:0.5;}";
            break;
        case PostNavStyle::Minimal:
            css += ".post-nav a{font-size:12px;text-transform:uppercase;"
                   "letter-spacing:0.5px;color:var(--muted);}";
            css += ".post-nav a:hover{color:var(--accent);}";
            break;
    }
}

inline void emit_scrollbar(std::string& css, Scrollbar s)
{
    switch (s)
    {
        case Scrollbar::Default: break;
        case Scrollbar::Thin:
            // Firefox
            css += "*{scrollbar-width:thin;scrollbar-color:var(--border) var(--bg);}";
            // WebKit
            css += "::-webkit-scrollbar{width:6px;height:6px;}";
            css += "::-webkit-scrollbar-track{background:var(--bg);}";
            css += "::-webkit-scrollbar-thumb{background:var(--border);border-radius:3px;}";
            css += "::-webkit-scrollbar-thumb:hover{background:var(--muted);}";
            break;
        case Scrollbar::Hidden:
            css += "*{scrollbar-width:none;}";
            css += "::-webkit-scrollbar{display:none;}";
            break;
    }
}

inline void emit_focus_style(std::string& css, FocusStyle f)
{
    switch (f)
    {
        case FocusStyle::Outline: break; // base CSS default
        case FocusStyle::Ring:
            css += ":focus-visible{outline:3px solid var(--accent);"
                   "outline-offset:3px;border-radius:2px;}";
            break;
        case FocusStyle::None:
            css += ":focus-visible{outline:none;}";
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

    // Shape & density
    detail::emit_corners(css, t.corners);
    detail::emit_density(css, t.density);
    detail::emit_border_weight(css, t.border_weight);

    // Components
    detail::emit_nav_style(css, t.nav_style);
    detail::emit_tag_style(css, t.tag_style);
    detail::emit_link_style(css, t.link_style);
    detail::emit_code_style(css, t.code_style);
    detail::emit_inline_code(css, t.inline_code);
    detail::emit_blockquote_style(css, t.quote_style);
    detail::emit_heading_case(css, t.heading_case);
    detail::emit_image_style(css, t.image_style);
    detail::emit_card_hover(css, t.card_hover);
    detail::emit_hr_style(css, t.hr_style);
    detail::emit_table_style(css, t.table_style);
    detail::emit_sidebar_style(css, t.sidebar_style);
    detail::emit_post_nav(css, t.post_nav);
    detail::emit_scrollbar(css, t.scrollbar);
    detail::emit_focus_style(css, t.focus_style);

    // Escape hatch
    if (!t.extra_css.empty())
        css += t.extra_css;

    return css;
}

} // namespace loom::theme
