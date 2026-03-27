#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Midnight — rich dark-first with electric blue, polished
inline const ThemeDef midnight = {
    .light = {{"#f0f2f8"}, {"#1e2030"}, {"#5a6080"}, {"#d0d4e0"}, {"#3b82f6"}},
    .dark  = {{"#0e1018"}, {"#c8cce0"}, {"#757c98"}, {"#1a1e30"}, {"#60a5fa"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "740px",
    .heading_weight = "800",
    .corners = Corners::Round,
    .nav_style = NavStyle::Pills,
    .image_style = ImageStyle::Shadow,
    .card_hover = CardHover::Glow,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        root() | set("tag-bg", mix(hex("#3b82f6"), 8, v::bg)) | set("tag-text", hex("#3b82f6")) | set("tag-hover-bg", hex("#3b82f6")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", 4_px),
        dark_root() | set("tag-bg", mix(hex("#60a5fa"), 10, v::bg)) | set("tag-text", hex("#60a5fa")) | set("tag-hover-bg", hex("#60a5fa")) | set("tag-hover-text", hex("#0e1018")),
        "::selection"_s        | bg(hex("#3b82f6")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#60a5fa")) | color(hex("#0e1018")),
        "header h1"_s | font_weight(800) | letter_spacing(raw("-0.5px")),
        ".post-card"_s | border_radius(8_px) | transition(raw("border-color 0.2s, box-shadow 0.2s, transform 0.2s")),
        ".post-card:hover"_s | border_color(mix(hex("#3b82f6"), 30, v::border)) | box_shadow(raw("0 4px 16px color-mix(in srgb, #3b82f6 8%, transparent)")) | prop("transform", raw("translateY(-1px)")),
        ".post-listing"_s | transition(raw("background 0.15s")) | padding(14_px, 10_px) | margin(raw("0 -10px")) | border_radius(6_px),
        ".post-listing:hover"_s | bg(mix(hex("#3b82f6"), 3, v::bg)),
        ".post-full h1"_s | font_weight(800) | letter_spacing(raw("-0.5px")),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left(3_px, solid, hex("#3b82f6")) | bg(mix(hex("#3b82f6"), 3, v::bg)) | border_radius(raw("0 6px 6px 0")) | padding(10_px, 14_px),
            "blockquote"_s.dark() | border_left_color(hex("#60a5fa")) | bg(mix(hex("#60a5fa"), 4, v::bg)),
            "a"_s                 | color(hex("#2563eb")),
            "a"_s.dark()          | color(hex("#78b4fc")),
            "pre"_s               | border_radius(8_px),
            ":not(pre) > code"_s  | border_radius(4_px)
        )
    ),
};

} // namespace loom::theme::builtin
