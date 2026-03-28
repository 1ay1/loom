#pragma once
#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// ── Palette ──
namespace {
    const auto green  = hex("#5fba7d");
    const auto dark   = hex("#1a1a1a");
    const auto panel  = hex("#212121");
    const auto line   = hex("#2e2e2e");
    const auto dim    = hex("#666666");
    const auto bright = hex("#eeeeee");
    const auto light  = hex("#f0f0f0");
    const auto faint  = hex("#5a5a5a");
}

inline const ThemeDef terminal = {
    .light = {{dark.v}, {"#d8d8d8"}, {"#777777"}, {line.v}, {green.v}},
    .dark  = {{dark.v}, {"#d8d8d8"}, {"#777777"}, {line.v}, {green.v}},
    .font  = {"ui-monospace,'SF Mono',SFMono-Regular,Menlo,Consolas,monospace"},
    .font_size = "13.5px",
    .max_width = "720px",
    .line_height = "1.6",
    .heading_weight = "600",
    .header_size = "32px",
    .corners = Corners::Sharp,
    .border_weight = BorderWeight::Thin,
    .tag_style = TagStyle::Bordered,
    .link_style = LinkStyle::None,
    .card_hover = CardHover::Border,
    .table_style = TableStyle::Minimal,
    .sidebar_style = SidebarStyle::Clean,
    .scrollbar = Scrollbar::Thin,
    .date_format = "%b %d",
    .index_heading = "posts",
    .styles = sheet(
        // ── Hide dark toggle — both palettes are identical ──
        ".theme-toggle"_s | display(none),

        // ── Header ──
        "header"_s | border_bottom(1_px, solid, line),
        "header h1"_s | font_weight(700) | font_size(32_px) | letter_spacing(1_px) | margin_bottom(0_px),
        link_colors("header h1 a", light, green),
        ".site-description"_s | color(dim) | font_size(13_px) | margin_top(4_px),
        "nav"_s | margin_top(12_px),
        link_colors("nav a", dim, green),
        "nav a"_s | font_weight(400) | font_size(13_px),

        // ── Listings ──
        ".post-listing"_s | display(block) | padding(16_px, 0_px) | border_bottom(1_px, solid, hex("#262626")),
        ".post-listing:hover"_s | bg(transparent),
        link_colors(".post-listing > a", bright, green),
        ".post-listing > a"_s | font_weight(600) | font_size(15_px) | display(block) | line_height(num(1.35)),
        ".post-listing-meta"_s | display(flex) | gap(12_px) | margin_top(4_px) | font_size(13_px) | color(dim),
        ".post-listing .reading-time::before"_s | prop("content", none),
        ".post-listing .excerpt"_s | display(block) | margin_top(6_px) | font_size(13_px)
            | color(faint) | line_height(num(1.5)),
        ".post-listing .post-tags"_s | margin_top(8_px),

        // ── Section headings ──
        "h2"_s | border_bottom(none) | font_size(12_px) | color(green)
               | font_weight(600) | text_transform(uppercase) | letter_spacing(2_px)
               | margin_top(0_px) | margin_bottom(12_px) | padding_bottom(0_px),

        // ── Tags ──
        vars({{"tag-bg", transparent}, {"tag-text", dim}, {"tag-radius", raw("0")},
              {"tag-hover-bg", green}, {"tag-hover-text", dark}}),
        ".tag"_s | border(1_px, solid, hex("#3a3a3a")) | padding(1_px, 7_px)
                 | font_size(11_px) | transition(raw("all 0.15s")),
        ".tag:hover"_s | border_color(green),

        // ── Cards ──
        ".post-card"_s | bg(panel) | border(1_px, solid, line)
                       | padding(18_px) | transition(raw("border-color 0.15s")),
        link_colors(".post-card a", bright, green),
        ".post-card a"_s | font_weight(600) | font_size(15_px),
        ".post-card:hover"_s | border_color(green),
        ".post-card .date"_s | color(dim) | margin_top(6_px),
        ".post-card .reading-time"_s | display(none),
        ".post-card .excerpt"_s | color(faint) | font_size(13_px) | margin_top(6_px),

        // ── Post page ──
        ".post-full h1"_s | font_size(26_px) | font_weight(700)
                          | margin_bottom(8_px) | line_height(num(1.3)) | color(light),
        ".post-meta"_s | color(dim) | font_size(13_px),
        ".heading-anchor"_s | opacity(0.2),

        // ── Content area ──
        content_area().nest(
            "blockquote"_s | border_left(3_px, solid, green) | color(hex("#999999"))
                           | bg(panel) | padding(10_px, 16_px),
            "a"_s | color(green),
            "a"_s.hover() | text_decoration(underline),
            "pre"_s | border(1_px, solid, line),
            "h1,h2,h3,h4"_s | color(bright),
            "h2"_s | font_size(20_px) | margin_top(28_px) | border_bottom(none),
            "h3"_s | font_size(16_px),
            "hr"_s | border_top(1_px, solid, line),
            "table th"_s | bg(panel),
            "table th,table td"_s | border_color(line)
        ),

        // ── Post nav ──
        ".post-nav"_s | border_top(1_px, solid, line) | margin_top(32_px),
        link_colors(".post-nav a", dim, green),
        ".post-nav a"_s | font_size(13_px),
        ".related-posts"_s | border_top(1_px, solid, line),
        ".related-posts h2"_s | color(green),
        link_colors(".related-posts li a", hex("#d8d8d8"), green),

        // ── Sidebar ──
        ".sidebar"_s | font_size(13_px),
        ".widget h3"_s | font_size(11_px) | letter_spacing(2_px) | color(green)
                       | border_bottom(1_px, solid, line) | padding_bottom(6_px),
        link_colors(".widget li a", hex("#d8d8d8"), green),
        ".widget li a"_s | font_size(13_px),
        ".widget .date"_s | color(dim),
        ".widget p"_s | color(dim),

        // ── Footer & chrome ──
        "footer"_s | border_top(1_px, solid, line) | color(dim),
        link_colors(".footer-links a", dim, green),
        "nav.breadcrumb"_s | color(dim),
        link_colors("nav.breadcrumb a", dim, green),
        ".series-nav"_s | border_color(line) | border_left_color(green) | bg(panel),
        ".series-label"_s | color(dim),
        ".series-list a"_s | color(green),
        ".series-current"_s | color(bright),
        "::selection"_s | bg(green) | color(dark),

        // ── Blinking cursor after site title ──
        keyframes("blink",
            from() | opacity(1.0),
            to()   | opacity(0.0)
        ),
        "header h1 a::after"_s
            | prop("content", raw("'\\2588'")) | color(green)
            | margin_left(2_px)
            | prop("animation", raw("blink 1s step-end infinite"))
    ),
};

} // namespace loom::theme::builtin
