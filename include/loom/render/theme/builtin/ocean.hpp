#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Ocean — deep blue, calm and professional
inline const ThemeDef ocean = {
    .light = {{"#f4f7fb"}, {"#1a2744"}, {"#5a6e8a"}, {"#d4dce8"}, {"#1a6fb5"}},
    .dark  = {{"#0c1420"}, {"#c8d4e4"}, {"#6880a0"}, {"#182840"}, {"#4da8e8"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        root() | set("tag-bg", mix(hex("#1a6fb5"), 8, v::bg)) | set("tag-text", hex("#1a6fb5")) | set("tag-hover-bg", hex("#1a6fb5")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", px(4)),
        dark_root() | set("tag-bg", mix(hex("#4da8e8"), 10, v::bg)) | set("tag-text", hex("#4da8e8")) | set("tag-hover-bg", hex("#4da8e8")) | set("tag-hover-text", hex("#0c1420")),
        sel("::selection") | bg(hex("#1a6fb5")) | color(hex("#ffffff")),
        dark(sel("::selection")) | bg(hex("#4da8e8")) | color(hex("#0c1420")),
        sel("header") | prop("border-bottom-color", mix(hex("#1a6fb5"), 15, v::border)),
        sel(".post-card") | border_radius(px(8)) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        sel(".post-card:hover") | box_shadow(raw("0 3px 14px color-mix(in srgb, #1a6fb5 10%, transparent)")),
        sel(".post-listing") | transition(raw("background 0.15s")) | padding(px(14), px(10)) | margin(raw("0 -10px")) | border_radius(px(6)),
        sel(".post-listing:hover") | bg(mix(hex("#1a6fb5"), 3, v::bg)),
        sel(".post-content blockquote", ".page-content blockquote") | border_left(px(3), solid, hex("#1a6fb5")) | bg(mix(hex("#1a6fb5"), 3, v::bg)) | border_radius(raw("0 4px 4px 0")) | padding(px(10), px(14)),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#4da8e8")) | bg(mix(hex("#4da8e8"), 4, v::bg)),
        sel(".post-content a", ".page-content a") | color(hex("#1a6fb5")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#68b8f0"))
    ),
};

} // namespace loom::theme::builtin
