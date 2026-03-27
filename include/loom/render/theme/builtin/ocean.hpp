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
        root() | set("tag-bg", mix(hex("#1a6fb5"), 8, v::bg)) | set("tag-text", hex("#1a6fb5")) | set("tag-hover-bg", hex("#1a6fb5")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", 4_px),
        dark_root() | set("tag-bg", mix(hex("#4da8e8"), 10, v::bg)) | set("tag-text", hex("#4da8e8")) | set("tag-hover-bg", hex("#4da8e8")) | set("tag-hover-text", hex("#0c1420")),
        "::selection"_s        | bg(hex("#1a6fb5")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#4da8e8")) | color(hex("#0c1420")),
        "header"_s | prop("border-bottom-color", mix(hex("#1a6fb5"), 15, v::border)),
        ".post-card"_s | border_radius(8_px) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        ".post-card:hover"_s | box_shadow(raw("0 3px 14px color-mix(in srgb, #1a6fb5 10%, transparent)")),
        ".post-listing"_s | transition(raw("background 0.15s")) | padding(14_px, 10_px) | margin(raw("0 -10px")) | border_radius(6_px),
        ".post-listing:hover"_s | bg(mix(hex("#1a6fb5"), 3, v::bg)),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left(3_px, solid, hex("#1a6fb5")) | bg(mix(hex("#1a6fb5"), 3, v::bg)) | border_radius(raw("0 4px 4px 0")) | padding(10_px, 14_px),
            "blockquote"_s.dark() | border_left_color(hex("#4da8e8")) | bg(mix(hex("#4da8e8"), 4, v::bg)),
            "a"_s                 | color(hex("#1a6fb5")),
            "a"_s.dark()          | color(hex("#68b8f0"))
        )
    ),
};

} // namespace loom::theme::builtin
