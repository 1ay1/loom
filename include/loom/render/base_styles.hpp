#pragma once
// ═══════════════════════════════════════════════════════════════════════
//  base_styles.hpp — the entire default stylesheet as a compiled CSS DSL
//
//  Zero raw CSS. Every rule is a typed C++ expression.
//  This replaces the 1000-line DEFAULT_CSS string literal.
// ═══════════════════════════════════════════════════════════════════════

#include "theme/css.hpp"
#include <string>

namespace loom
{

namespace base_css_impl
{
using namespace css;

// ── CSS variable tokens (the design system) ──

inline css::Sheet base_variables()
{
    return sheet(
        root()
            | set("bg", hex("#ffffff"))
            | set("text", hex("#0f172a"))
            | set("muted", hex("#64748b"))
            | set("border", hex("#e5e7eb"))
            | set("accent", hex("#2563eb"))
            | set("font", raw("system-ui,-apple-system,Segoe UI,Roboto,sans-serif"))
            | set("max-width", raw("700px"))
            | set("font-size", raw("17px"))
            | set("heading-font", inherit)
            | set("code-font", raw("ui-monospace,SFMono-Regular,Menlo,Consolas,monospace"))
            | set("line-height", raw("1.7"))
            | set("heading-weight", raw("700"))
            | set("border-radius", raw("6px"))
            | set("container-padding", raw("40px 20px"))
            | set("sidebar-width", raw("260px"))
            | set("sidebar-gap", raw("32px"))
            | set("nav-gap", raw("18px"))
            | set("header-size", raw("42px"))
            | set("tag-radius", raw("12px"))
            | set("tag-bg", raw("color-mix(in srgb, var(--text) 7%, var(--bg))"))
            | set("tag-text", raw("var(--text)"))
            | set("tag-hover-bg", raw("color-mix(in srgb, var(--accent) 20%, var(--tag-bg))"))
            | set("tag-hover-text", raw("var(--accent)"))
            | set("link-decoration", underline)
            | set("card-bg", raw("var(--bg)"))
            | set("card-border", raw("var(--border)"))
            | set("card-radius", raw("8px"))
            | set("card-padding", raw("20px"))
            | set("accent-hover", raw("var(--accent)"))
            | set("header-border-width", raw("1px"))
            | set("footer-border-width", raw("1px"))
            | set("content-width", raw("var(--max-width)"))
            | set("card-hover-shadow", raw("0 2px 12px color-mix(in srgb, var(--text) 6%, transparent)"))
            | set("card-hover-lift", raw("translateY(-1px)")),

        dark_root()
            | set("bg", hex("#0b0f14"))
            | set("text", hex("#e5e7eb"))
            | set("muted", hex("#94a3b8"))
            | set("border", hex("#1f2937"))
            | set("accent", hex("#60a5fa"))
    );
}

// ── Reset & body ──

inline css::Sheet base_reset()
{
    return sheet(
        "*"_s | margin(px(0)) | padding(px(0)) | prop("box-sizing", raw("border-box")),

        "body"_s
            | bg(raw("var(--bg)")) | color(raw("var(--text)"))
            | prop("font-family", raw("var(--font)"))
            | line_height(raw("var(--line-height)"))
            | font_size(raw("var(--font-size)"))
            | prop("overflow-x", hidden)
            | prop("-webkit-text-size-adjust", raw("100%"))
            | prop("-webkit-font-smoothing", raw("antialiased"))
            | prop("-moz-osx-font-smoothing", raw("grayscale"))
            | prop("min-height", raw("100dvh"))
            | display(raw("flex"))
            | prop("flex-direction", raw("column")),

        "body,header,footer,.post-card,.tag,.theme-toggle,.sidebar"_s
            | transition(raw("background-color 0.25s, color 0.25s, border-color 0.25s")),

        "::selection"_s | bg(raw("var(--accent)")) | color(raw("var(--bg)")),

        ":focus-visible"_s
            | prop("outline", raw("2px solid var(--accent)"))
            | prop("outline-offset", raw("2px")),

        "a"_s | transition(raw("color 0.15s"))
    );
}

// ── Layout containers ──

inline css::Sheet base_layout()
{
    return sheet(
        ".container"_s
            | max_width(raw("var(--content-width)"))
            | prop("margin-inline", raw("auto"))
            | padding(raw("var(--container-padding)"))
            | width(raw("100%")),

        ".has-sidebar .container"_s | max_width(raw("960px")),

        "header"_s | prop("border-bottom", raw("var(--header-border-width) solid var(--border)")),

        ".header-bar"_s
            | display(raw("flex"))
            | prop("justify-content", raw("space-between"))
            | prop("align-items", center),

        ".header-centered .header-bar"_s
            | prop("flex-direction", raw("column"))
            | prop("text-align", center)
            | gap(px(8)),

        ".header-minimal header"_s | prop("border-bottom", none),

        ".header-left"_s | display(raw("flex")) | prop("flex-direction", raw("column")),
        ".header-centered .header-left"_s | prop("align-items", center),

        "header h1"_s
            | font_size(raw("var(--header-size)"))
            | font_weight(raw("var(--heading-weight)"))
            | prop("font-family", raw("var(--heading-font)")),

        "header h1 a"_s | text_decoration(none) | color(inherit),

        ".site-description"_s | color(raw("var(--muted)")) | font_size(px(15)) | margin_top(px(4)),

        "nav"_s | margin_top(px(10)),
        "nav ul"_s | prop("list-style", none) | display(raw("flex")) | gap(raw("var(--nav-gap)")),
        ".header-centered nav ul"_s | prop("justify-content", center),
        "nav a"_s | text_decoration(none) | color(raw("var(--muted)")) | font_weight(500),
        "nav a:hover"_s | color(raw("var(--accent-hover)")),

        "h2"_s
            | margin_top(px(24)) | margin_bottom(px(14)) | font_size(px(20))
            | prop("font-family", raw("var(--heading-font)"))
            | font_weight(raw("var(--heading-weight)")),

        "html"_s | prop("scroll-behavior", raw("smooth"))
    );
}

// ── Post listings ──

inline css::Sheet base_listings()
{
    return sheet(
        ".post-listing"_s
            | padding(px(14), px(10)) | prop("margin-inline", raw("-10px"))
            | prop("border-bottom", raw("1px solid var(--border)"))
            | display(raw("flex")) | prop("flex-wrap", raw("wrap"))
            | prop("align-items", raw("baseline")) | gap(px(6))
            | border_radius(px(4)) | transition(raw("background 0.15s")),
        ".post-listing:hover"_s | bg(raw("color-mix(in srgb, var(--text) 3%, var(--bg))")),
        ".post-listing a"_s | color(raw("var(--text)")) | text_decoration(none) | font_weight(500),
        ".post-listing a:hover"_s | color(raw("var(--accent-hover)")),
        ".post-listing .post-tags"_s | margin_top(px(0)),
        ".post-listing .tag"_s | color(raw("var(--tag-text)")) | text_decoration(none),
        ".post-listing .tag:hover"_s | color(raw("var(--tag-hover-text)"))
    );
}

// ── Post cards ──

inline css::Sheet base_cards()
{
    return sheet(
        ".post-cards"_s
            | display(raw("grid"))
            | prop("grid-template-columns", raw("repeat(auto-fill, minmax(280px, 1fr))"))
            | gap(px(20)),

        ".post-card"_s
            | bg(raw("var(--card-bg)"))
            | border(px(1), solid, raw("var(--card-border)"))
            | border_radius(raw("var(--card-radius)"))
            | padding(raw("var(--card-padding)"))
            | transition(raw("border-color 0.2s, box-shadow 0.2s, transform 0.2s")),

        ".post-card:hover"_s
            | border_color(raw("var(--accent)"))
            | box_shadow(raw("var(--card-hover-shadow)"))
            | prop("transform", raw("var(--card-hover-lift)")),

        ".post-card a"_s
            | color(raw("var(--text)")) | text_decoration(none)
            | font_weight(600) | font_size(raw("1.05em")),
        ".post-card a:hover"_s | color(raw("var(--accent-hover)")),
        ".post-card .date"_s | display(block) | margin_top(px(8)),
        ".post-card .post-tags"_s | margin_top(px(10)),
        ".post-card .tag"_s | color(raw("var(--tag-text)")) | text_decoration(none),
        ".post-card .tag:hover"_s | color(raw("var(--tag-hover-text)"))
    );
}

// ── Post content ──

inline css::Sheet base_post()
{
    return sheet(
        ".post-full"_s | padding(px(0)),
        ".post-full h1"_s
            | font_size(raw("1.8em")) | margin_bottom(px(8))
            | prop("font-family", raw("var(--heading-font)"))
            | font_weight(raw("var(--heading-weight)")),
        ".post-meta"_s | margin_bottom(px(12)) | font_size(px(14)) | color(raw("var(--muted)")),
        ".post-full .post-tags"_s | margin_bottom(px(20))
    );
}

inline css::Sheet base_content()
{
    return sheet(
        ".post-content,.page-content"_s | margin_top(px(20)) | line_height(raw("1.8")),

        // Headings
        ".post-content h1,.page-content h1"_s
            | font_size(raw("1.8em")) | margin_top(px(36)) | margin_bottom(px(12))
            | font_weight(raw("var(--heading-weight)"))
            | prop("font-family", raw("var(--heading-font)")),
        ".post-content h2,.page-content h2"_s
            | font_size(raw("1.5em")) | margin_top(px(28)) | margin_bottom(px(10))
            | prop("font-family", raw("var(--heading-font)")),
        ".post-content h3,.page-content h3"_s
            | font_size(raw("1.25em")) | margin_top(px(22)) | margin_bottom(px(8))
            | prop("font-family", raw("var(--heading-font)")),
        ".post-content h4,.page-content h4"_s
            | font_size(raw("1.1em")) | margin_top(px(18)) | margin_bottom(px(6)),
        ".post-content h5,.page-content h5"_s
            | font_size(raw("1em")) | margin_top(px(16)) | margin_bottom(px(6)),
        ".post-content h6,.page-content h6"_s
            | font_size(raw("0.9em")) | margin_top(px(14)) | margin_bottom(px(4))
            | color(raw("var(--muted)")),

        // Heading anchors
        ".post-content h1,.post-content h2,.post-content h3,.post-content h4,.post-content h5,.post-content h6,.page-content h1,.page-content h2,.page-content h3,.page-content h4,.page-content h5,.page-content h6"_s
            | prop("position", raw("relative")) | prop("scroll-margin-top", raw("24px")),
        ".heading-anchor"_s
            | opacity(0) | text_decoration(none) | padding_right(raw("0.4em"))
            | margin_left(raw("-1.2em")) | font_weight(400) | color(raw("var(--muted)"))
            | transition(raw("opacity 0.15s")),
        "h1:hover>.heading-anchor,h2:hover>.heading-anchor,h3:hover>.heading-anchor,h4:hover>.heading-anchor,h5:hover>.heading-anchor,h6:hover>.heading-anchor"_s
            | opacity(0.5),
        ".heading-anchor:hover"_s | prop("opacity", raw("1 !important")) | color(raw("var(--accent)")),

        // Paragraphs & lists
        ".post-content p,.page-content p"_s | margin_bottom(px(14)),
        ".post-content ul,.page-content ul"_s | margin_left(px(24)) | margin_bottom(px(14)),
        ".post-content ul ul,.post-content ul ol,.post-content ol ul,.post-content ol ol,.page-content ul ul,.page-content ul ol,.page-content ol ul,.page-content ol ol"_s
            | margin_bottom(px(0)),
        ".post-content li,.page-content li"_s | margin_bottom(px(2)),
        ".post-content ol,.page-content ol"_s | margin_left(px(24)) | margin_bottom(px(14)),

        // Code
        ".post-content pre,.page-content pre"_s
            | bg(raw("color-mix(in srgb, var(--text) 7%, var(--bg))"))
            | padding(px(14)) | border_radius(raw("var(--border-radius)"))
            | prop("overflow-x", raw("auto")) | margin_bottom(px(14))
            | font_size(px(13)) | line_height(raw("1.5")),
        ".post-content code,.page-content code"_s
            | prop("font-family", raw("var(--code-font)")),
        ".post-content :not(pre)>code,.page-content :not(pre)>code"_s
            | bg(raw("color-mix(in srgb, var(--text) 7%, var(--bg))"))
            | padding(raw("2px 6px"))
            | border_radius(raw("calc(var(--border-radius) * 0.67)"))
            | font_size(raw("0.9em")),

        // Horizontal rules
        ".post-content hr,.page-content hr"_s
            | border(none) | prop("border-top", raw("1px solid var(--border)"))
            | margin(raw("28px 0")),

        // Definition lists
        ".post-content dl,.page-content dl"_s | margin_bottom(px(14)),
        ".post-content dt,.page-content dt"_s | font_weight(600) | margin_top(px(10)),
        ".post-content dd,.page-content dd"_s | margin_left(px(24)) | margin_bottom(px(6)),

        // Links
        ".post-content a,.page-content a"_s
            | color(raw("var(--accent)")) | text_decoration(raw("var(--link-decoration)")),

        // Tables
        ".post-content table,.page-content table"_s
            | prop("border-collapse", raw("collapse")) | margin_bottom(px(14)) | width(raw("100%")),
        ".post-content th,.page-content th"_s
            | bg(raw("color-mix(in srgb, var(--text) 7%, var(--bg))")) | font_weight(600),
        ".post-content th,.post-content td,.page-content th,.page-content td"_s
            | border(px(1), solid, raw("var(--border)")) | padding(raw("8px 12px"))
            | prop("text-align", raw("left")),

        // Blockquotes
        ".post-content blockquote,.page-content blockquote"_s
            | border_left(px(4), solid, raw("var(--accent)"))
            | margin(raw("0 0 14px 0")) | padding(raw("8px 16px"))
            | color(raw("var(--muted)")),
        ".post-content blockquote p,.page-content blockquote p"_s | margin_bottom(px(4)),

        // Images
        ".post-content img,.page-content img"_s
            | max_width(raw("100%")) | prop("height", raw("auto"))
            | border_radius(raw("var(--border-radius)"))
            | display(block) | margin_bottom(px(14)),

        // Misc
        ".post-content del,.page-content del"_s | color(raw("var(--muted)")),
        ".post-content .footnotes"_s | font_size(px(14)) | color(raw("var(--muted)")),
        ".post-content li:has(input[type=\"checkbox\"]),.page-content li:has(input[type=\"checkbox\"])"_s
            | prop("list-style", none) | margin_left(raw("-20px")),
        ".post-content input[type=\"checkbox\"]"_s | margin_right(px(6))
    );
}

// ── Common elements ──

inline css::Sheet base_elements()
{
    return sheet(
        "time"_s | color(raw("var(--muted)")) | font_size(px(14)),
        ".date"_s | color(raw("var(--muted)")) | font_size(px(14))
            | prop("font-family", raw("var(--code-font)"))
            | prop("font-variant-numeric", raw("tabular-nums")),

        "footer"_s
            | prop("margin-top", raw("auto"))
            | padding_top(px(40)) | padding_bottom(px(24))
            | prop("border-top", raw("var(--footer-border-width) solid var(--border)"))
            | font_size(px(14)) | color(raw("var(--muted)")),
        ".footer-links"_s | margin_top(px(8)) | display(raw("flex")) | gap(px(16)),
        ".footer-links a"_s | color(raw("var(--muted)")) | text_decoration(none),
        ".footer-links a:hover"_s | color(raw("var(--accent-hover)")),

        ".post-tags"_s | display(raw("flex")) | prop("flex-wrap", raw("wrap")) | gap(px(6)) | margin_top(px(8)),
        ".tag"_s
            | display(raw("inline-block")) | bg(raw("var(--tag-bg)")) | color(raw("var(--tag-text)"))
            | padding(raw("2px 10px")) | border_radius(raw("var(--tag-radius)"))
            | font_size(px(13)) | text_decoration(none) | font_weight(500),
        ".tag:hover"_s | bg(raw("var(--tag-hover-bg)")) | color(raw("var(--tag-hover-text)"))
    );
}

// ── Sidebar ──

inline css::Sheet base_sidebar()
{
    return sheet(
        ".with-sidebar"_s
            | display(raw("grid"))
            | prop("grid-template-columns", raw("1fr var(--sidebar-width)"))
            | gap(raw("var(--sidebar-gap)")),
        ".sidebar-left .with-sidebar"_s
            | prop("grid-template-columns", raw("var(--sidebar-width) 1fr")),
        ".sidebar-left .with-sidebar>aside"_s | order(raw("-1")),
        ".with-sidebar>main"_s
            | prop("min-width", raw("0"))
            | prop("overflow-wrap", raw("break-word"))
            | prop("word-break", raw("break-word")),

        ".sidebar"_s
            | font_size(px(14)) | border_left(px(1), solid, raw("var(--border)"))
            | padding_left(px(32)) | padding_top(px(8)),
        ".sidebar-left .sidebar"_s
            | prop("border-left", none) | border_right(px(1), solid, raw("var(--border)"))
            | padding_left(px(0)) | padding_right(px(32)),

        ".widget"_s | margin_bottom(px(24)),
        ".widget h3"_s
            | font_size(px(13)) | font_weight(600) | margin_bottom(px(10))
            | text_transform(uppercase) | letter_spacing(raw("0.8px"))
            | color(raw("var(--muted)"))
            | prop("border-bottom", raw("1px solid var(--border)"))
            | padding_bottom(px(6)),
        ".widget ul"_s | prop("list-style", none),
        ".widget li"_s | margin_bottom(px(8)) | line_height(raw("1.4")),
        ".sidebar .post-tags"_s | gap(px(5)),
        ".sidebar .tag"_s | font_size(px(12)) | padding(raw("1px 7px")),
        ".widget li a"_s | color(raw("var(--text)")) | text_decoration(none) | font_size(px(14)),
        ".widget li a:hover"_s | color(raw("var(--accent-hover)")),
        ".widget .date"_s | display(block) | font_size(px(12)),
        ".widget p"_s | color(raw("var(--muted)")) | line_height(raw("1.6")) | font_size(px(14)),
        ".widget p a"_s | color(raw("var(--accent)")) | text_decoration(none),
        ".widget p a:hover"_s | color(raw("var(--accent-hover)")) | text_decoration(underline),
        ".widget .post-tags"_s | margin_top(px(0))
    );
}

// ── Interactive elements ──

inline css::Sheet base_interactive()
{
    return sheet(
        ".theme-toggle"_s
            | prop("cursor", raw("pointer"))
            | border(px(1), solid, raw("var(--border)"))
            | bg(none) | color(raw("var(--text)"))
            | padding(raw("6px 12px")) | border_radius(raw("var(--border-radius)"))
            | font_size(px(14)) | transition(raw("background 0.2s ease")),
        ".theme-toggle:hover"_s | bg(raw("var(--border)")),

        ".reading-time"_s | color(raw("var(--muted)")) | font_size(px(13)),

        ".excerpt"_s
            | color(raw("var(--muted)")) | font_size(px(14)) | line_height(raw("1.5"))
            | margin_top(px(6)) | margin_bottom(px(0)),
        ".post-card .excerpt"_s
            | display(raw("-webkit-box"))
            | prop("-webkit-line-clamp", raw("3"))
            | prop("-webkit-box-orient", raw("vertical"))
            | prop("overflow", hidden)
    );
}

// ── Post navigation ──

inline css::Sheet base_post_nav()
{
    return sheet(
        ".post-nav"_s
            | display(raw("flex")) | prop("justify-content", raw("space-between"))
            | gap(px(20)) | margin_top(px(48)) | padding_top(px(20))
            | prop("border-top", raw("1px solid var(--border)")),
        ".post-nav a"_s
            | color(raw("var(--muted)")) | text_decoration(none) | font_size(px(14))
            | max_width(raw("45%")) | line_height(raw("1.4")) | transition(raw("color 0.15s")),
        ".post-nav a:hover"_s | color(raw("var(--accent)")),
        ".post-nav-next"_s | prop("text-align", raw("right")) | prop("margin-left", raw("auto")),

        ".related-posts"_s
            | margin_top(px(32)) | padding_top(px(20))
            | prop("border-top", raw("1px solid var(--border)")),
        ".related-posts h2"_s
            | font_size(px(14)) | font_weight(600)
            | text_transform(uppercase) | letter_spacing(raw("0.5px"))
            | color(raw("var(--muted)")) | margin_top(px(0)) | margin_bottom(px(12)),
        ".related-posts ul"_s | prop("list-style", none),
        ".related-posts li"_s | margin_bottom(px(8)) | line_height(raw("1.4")),
        ".related-posts li a"_s | color(raw("var(--text)")) | text_decoration(none) | font_weight(500),
        ".related-posts li a:hover"_s | color(raw("var(--accent)")),
        ".related-posts .date"_s | font_size(px(12)) | margin_left(px(8))
    );
}

// ── Series ──

inline css::Sheet base_series()
{
    return sheet(
        ".series-nav"_s
            | margin(raw("24px 0")) | padding(raw("16px 20px"))
            | border(px(1), solid, raw("var(--border)"))
            | border_left(px(3), solid, raw("var(--accent)"))
            | border_radius(raw("var(--border-radius)"))
            | font_size(px(14))
            | bg(raw("color-mix(in srgb, var(--accent) 3%, var(--bg))")),
        ".series-label"_s
            | margin_bottom(px(10)) | font_size(px(13)) | color(raw("var(--muted)")),
        ".series-list"_s | margin_left(px(20)) | margin_bottom(px(0)),
        ".series-list li"_s | margin_bottom(px(4)),
        ".series-list a"_s | color(raw("var(--accent)")) | text_decoration(none),
        ".series-list a:hover"_s | text_decoration(underline),
        ".series-current"_s | font_weight(600) | color(raw("var(--text)")),
        ".archive-year"_s | margin_top(px(24))
    );
}

// ── Breadcrumbs ──

inline css::Sheet base_breadcrumbs()
{
    return sheet(
        "nav.breadcrumb"_s | font_size(raw("0.85rem")) | color(raw("var(--muted)")) | margin_bottom(px(20)),
        "nav.breadcrumb ol"_s
            | prop("list-style", none) | padding(px(0)) | margin(px(0))
            | display(raw("flex")) | prop("flex-wrap", raw("wrap"))
            | gap(px(4)) | prop("align-items", center),
        "nav.breadcrumb li+li::before"_s
            | content(raw("\"\\203A\"")) | margin_right(px(4)) | color(raw("var(--muted)")),
        "nav.breadcrumb a"_s | color(raw("var(--muted)")) | text_decoration(none),
        "nav.breadcrumb a:hover"_s | color(raw("var(--accent)")) | text_decoration(underline),
        "nav.breadcrumb li:last-child"_s | color(raw("var(--text)"))
    );
}

// ── Responsive ──

inline css::Sheet base_responsive()
{
    return sheet(
        media(max_w(px(768)),
            root()
                | set("container-padding", raw("24px 16px"))
                | set("font-size", raw("15px"))
                | set("header-size", raw("36px"))
                | set("line-height", raw("1.65")),
            "nav ul"_s | prop("flex-wrap", raw("wrap")) | gap(raw("10px 14px")),
            ".with-sidebar"_s | prop("grid-template-columns", raw("1fr")),
            ".sidebar-left .with-sidebar"_s | prop("grid-template-columns", raw("1fr")),
            ".sidebar-left .with-sidebar>aside"_s | order(raw("initial")),
            ".sidebar"_s
                | prop("border-left", none)
                | prop("border-top", raw("1px solid var(--border)"))
                | padding_left(px(0)) | padding_top(px(24)),
            ".sidebar-left .sidebar"_s
                | prop("border-right", none)
                | prop("border-top", raw("1px solid var(--border)"))
                | padding_right(px(0)) | padding_top(px(24)),
            ".has-sidebar .container"_s | max_width(raw("var(--content-width)")),
            ".post-full h1"_s | font_size(raw("1.5em")),
            ".post-content h1,.page-content h1"_s | font_size(raw("1.5em")) | margin_top(px(28)),
            ".post-content h2,.page-content h2"_s | font_size(raw("1.3em")) | margin_top(px(22)),
            ".post-content h3,.page-content h3"_s | font_size(raw("1.15em")) | margin_top(px(18)),
            ".post-content pre,.page-content pre"_s
                | padding(px(12)) | font_size(px(13)) | border_radius(px(0))
                | margin_left(raw("-16px")) | margin_right(raw("-16px")),
            ".post-content table,.page-content table"_s
                | display(block) | prop("overflow-x", raw("auto"))
                | prop("-webkit-overflow-scrolling", raw("touch")),
            ".post-content,.page-content"_s | font_size(raw("14.5px")) | line_height(raw("1.7")),
            ".post-content blockquote,.page-content blockquote"_s
                | margin_left(px(0)) | margin_right(px(0)) | padding(raw("6px 12px")),
            ".post-cards"_s | prop("grid-template-columns", raw("1fr")) | gap(px(14)),
            ".footer-links"_s | prop("flex-wrap", raw("wrap")) | gap(raw("10px 16px"))
        ),

        media(max_w(px(480)),
            root()
                | set("container-padding", raw("20px 12px"))
                | set("font-size", raw("14.5px"))
                | set("header-size", raw("30px")),
            ".post-content,.page-content"_s | font_size(raw("13.5px")),
            ".post-full h1"_s | font_size(raw("1.35em")),
            ".post-content pre,.page-content pre"_s
                | margin_left(raw("-12px")) | margin_right(raw("-12px"))
                | font_size(px(12)) | padding(px(10)),
            ".post-content th,.post-content td,.page-content th,.page-content td"_s
                | padding(raw("6px 8px")) | font_size(px(14)),
            ".tag"_s | font_size(px(12)) | padding(raw("2px 8px"))
        ),

        media("print",
            "body"_s | font_size(raw("12pt")) | color(hex("#000")) | bg(hex("#fff")) | display(block),
            "header,footer,.theme-toggle,.sidebar,.post-nav,.breadcrumb,.related-posts,.post-tags"_s
                | display(none),
            ".container"_s | max_width(raw("100%")) | padding(px(0)),
            "a"_s | color(hex("#000")) | text_decoration(underline),
            ".post-content pre,.page-content pre"_s
                | border(px(1), solid, hex("#ccc")) | prop("page-break-inside", raw("avoid")),
            ".post-content img,.page-content img"_s
                | max_width(raw("100%")) | prop("page-break-inside", raw("avoid")),
            ".post-full h1"_s | font_size(raw("24pt")) | margin_bottom(raw("8pt")),
            ".post-meta"_s | color(hex("#666")) | margin_bottom(raw("16pt"))
        )
    );
}

} // namespace base_css_impl

// ═══════════════════════════════════════════════════════════════════════
//  Compile all base styles into a single CSS string
// ═══════════════════════════════════════════════════════════════════════

inline std::string compile_base_styles()
{
    using namespace base_css_impl;
    auto all = base_variables()
             + base_reset()
             + base_layout()
             + base_listings()
             + base_cards()
             + base_post()
             + base_content()
             + base_elements()
             + base_sidebar()
             + base_interactive()
             + base_post_nav()
             + base_series()
             + base_breadcrumbs()
             + base_responsive();

    return all.compile();
}

} // namespace loom
