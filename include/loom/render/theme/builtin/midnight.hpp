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
        root() | set("tag-bg", mix(hex("#3b82f6"), 8, v::bg)) | set("tag-text", hex("#3b82f6")) | set("tag-hover-bg", hex("#3b82f6")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", px(4)),
        dark_root() | set("tag-bg", mix(hex("#60a5fa"), 10, v::bg)) | set("tag-text", hex("#60a5fa")) | set("tag-hover-bg", hex("#60a5fa")) | set("tag-hover-text", hex("#0e1018")),
        sel("::selection") | bg(hex("#3b82f6")) | color(hex("#ffffff")),
        dark(sel("::selection")) | bg(hex("#60a5fa")) | color(hex("#0e1018")),
        sel("header h1") | font_weight(800) | letter_spacing(raw("-0.5px")),
        sel(".post-card") | border_radius(px(8)) | transition(raw("border-color 0.2s, box-shadow 0.2s, transform 0.2s")),
        sel(".post-card:hover") | border_color(mix(hex("#3b82f6"), 30, v::border)) | box_shadow(raw("0 4px 16px color-mix(in srgb, #3b82f6 8%, transparent)")) | prop("transform", raw("translateY(-1px)")),
        sel(".post-listing") | transition(raw("background 0.15s")) | padding(px(14), px(10)) | margin(raw("0 -10px")) | border_radius(px(6)),
        sel(".post-listing:hover") | bg(mix(hex("#3b82f6"), 3, v::bg)),
        sel(".post-full h1") | font_weight(800) | letter_spacing(raw("-0.5px")),
        sel(".post-content blockquote", ".page-content blockquote") | border_left(px(3), solid, hex("#3b82f6")) | bg(mix(hex("#3b82f6"), 3, v::bg)) | border_radius(raw("0 6px 6px 0")) | padding(px(10), px(14)),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#60a5fa")) | bg(mix(hex("#60a5fa"), 4, v::bg)),
        sel(".post-content a", ".page-content a") | color(hex("#2563eb")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#78b4fc")),
        sel(".post-content pre", ".page-content pre") | border_radius(px(8)),
        sel(".post-content :not(pre) > code", ".page-content :not(pre) > code") | border_radius(px(4))
    ),
};

} // namespace loom::theme::builtin
