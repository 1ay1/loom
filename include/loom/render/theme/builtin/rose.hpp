#pragma once
#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// ── Rose — refined editorial elegance ──
//
// Soft pinks, warm magenta accent, round corners, generous whitespace.
// Designed to feel like a beautifully typeset magazine, not a tech blog.

inline const ThemeDef rose = {
    .light = {{"#fffbfc"}, {"#2a1f25"}, {"#8e7a86"}, {"#f0dde4"}, {"#c2185b"}},
    .dark  = {{"#1a1118"}, {"#f5e6ec"}, {"#b09aa6"}, {"#2d2028"}, {"#f06292"}},
    .font  = {"'Georgia','Times New Roman',serif"},
    .font_size = "16px",
    .max_width = "680px",
    .heading_font = {"'Helvetica Neue',Helvetica,Arial,sans-serif"},
    .line_height = "1.8",
    .heading_weight = "700",
    .header_size = "28px",
    .corners = Corners::Round,
    .density = Density::Airy,
    .nav_style = NavStyle::Pills,
    .tag_style = TagStyle::Pill,
    .link_style = LinkStyle::Underline,
    .card_hover = CardHover::Lift,
    .scrollbar = Scrollbar::Thin,
    .date_format = "%B %d, %Y",
    .styles = sheet(
        // ── Selection ──
        "::selection"_s | bg(hex("#c2185b")) | color(hex("#fffbfc")),
        "::selection"_s.dark() | bg(hex("#f06292")) | color(hex("#1a1118")),

        // ── Header — big, clean, centered feel ──
        "header h1"_s | font_size(32_px) | letter_spacing(raw("-0.5px")),
        "header h1 a"_s | color(raw("var(--text)")) | text_decoration(none),
        "header h1 a:hover"_s | color(raw("var(--accent)")),
        ".site-description"_s | font_style(italic) | font_size(15_px)
                              | color(raw("var(--muted)")) | margin_top(4_px),

        // ── Nav pills — rosy tint on hover ──
        "nav a"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                  | font_size(13_px) | letter_spacing(raw("0.3px")),
        "nav a:hover"_s | bg(raw("color-mix(in srgb, var(--accent) 8%, var(--bg))")),

        // ── Post listings — editorial style ──
        ".post-listing"_s | padding(18_px, 0_px),
        ".post-listing > a"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                              | font_size(raw("1.15em")) | font_weight(600)
                              | letter_spacing(raw("-0.3px")),
        ".post-listing .excerpt"_s | font_size(14_px) | line_height(num(1.6))
                                   | margin_top(6_px),
        ".post-listing .post-tags"_s | margin_top(8_px),

        // ── Section headings — sans-serif, uppercase, delicate ──
        "h2"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
               | font_size(11_px) | font_weight(600) | text_transform(uppercase)
               | letter_spacing(2_px) | color(raw("var(--muted)"))
               | border_bottom(none) | margin_bottom(16_px) | padding_bottom(0_px),

        // ── Tags — soft pills ──
        vars({{"tag-radius", raw("999px")}}),
        ".tag"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                 | font_size(11_px) | letter_spacing(raw("0.3px")),

        // ── Cards — soft shadow, generous padding ──
        ".post-card"_s | padding(22_px)
                       | box_shadow(raw("0 1px 6px color-mix(in srgb, var(--text) 6%, transparent)")),
        ".post-card a"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                         | font_weight(600) | font_size(raw("1.1em")),
        ".post-card .excerpt"_s | font_size(13_px) | margin_top(8_px),

        // ── Post page ──
        ".post-full h1"_s | font_size(28_px) | letter_spacing(raw("-0.5px"))
                          | line_height(num(1.3)) | margin_bottom(10_px),
        ".post-meta"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                       | font_size(13_px) | letter_spacing(raw("0.2px")),
        ".heading-anchor"_s | opacity(0.15),

        // ── Content — serif body, clean hierarchy ──
        ".post-content,.page-content"_s | font_size(16_px) | line_height(num(1.85)),
        content_area().nest(
            // Links
            "a"_s | text_underline_offset(3_px)
                  | text_decoration_thickness(1_px),
            // Headings — sans-serif contrast against serif body
            "h2"_s | font_size(22_px) | margin_top(40_px) | margin_bottom(14_px)
                   | text_transform(none) | letter_spacing(raw("-0.3px"))
                   | border_bottom(none),
            "h3"_s | font_size(18_px) | margin_top(28_px),
            "h4"_s | font_size(15_px),
            // Code — subtle accent tint
            "pre"_s | border(1_px, solid, raw("var(--border)"))
                    | border_radius(12_px) | padding(16_px)
                    | font_size(13_px) | margin(raw("24px 0")),
            // Blockquotes — elegant left border, italic
            "blockquote"_s | border_left(3_px, solid, raw("var(--accent)"))
                           | color(raw("var(--muted)")) | font_style(italic)
                           | padding(8_px, 20_px) | margin(raw("20px 0")),
            // Bold
            "strong"_s | color(raw("var(--text)")),
            // HR — delicate
            "hr"_s | margin(raw("36px 0")) | opacity(0.5),
            // Paragraphs — generous spacing
            "p"_s | margin_bottom(16_px),
            // Lists
            "li"_s | margin_bottom(6_px)
        ),

        // ── Post nav ──
        ".post-nav"_s | margin_top(36_px) | padding_top(16_px),
        ".post-nav a"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                        | font_size(13_px),
        ".related-posts h2"_s | font_size(11_px) | text_transform(uppercase)
                              | letter_spacing(2_px) | color(raw("var(--muted)")),
        ".related-posts li a"_s | font_size(14_px),

        // ── Series nav — numbered, clean ──
        ".series-nav"_s | border_radius(12_px)
                        | bg(raw("color-mix(in srgb, var(--accent) 4%, var(--bg))"))
                        | border(1_px, solid, raw("var(--border)"))
                        | padding(16_px, 20_px),
        ".series-label"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                          | font_size(12_px) | letter_spacing(raw("0.3px")),
        ".series-list a"_s | font_size(14_px),

        // ── Sidebar — refined ──
        ".sidebar"_s | font_size(14_px),
        ".widget h3"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                       | font_size(11_px) | font_weight(600) | text_transform(uppercase)
                       | letter_spacing(2_px) | color(raw("var(--muted)"))
                       | border_bottom(1_px, solid, raw("var(--border)"))
                       | padding_bottom(8_px),
        ".widget li a"_s | font_size(14_px),
        ".widget .date"_s | font_size(12_px) | display(block),
        ".widget p"_s | font_size(14_px) | line_height(num(1.7)),

        // ── Footer ──
        "footer"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                   | font_size(12_px) | letter_spacing(raw("0.2px")),

        // ── Breadcrumbs ──
        "nav.breadcrumb"_s | font_family(raw("'Helvetica Neue',Helvetica,Arial,sans-serif"))
                           | font_size(12_px)
    ),
};

} // namespace loom::theme::builtin
