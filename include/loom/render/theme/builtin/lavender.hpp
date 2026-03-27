#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Lavender — soft purple tones, warm and inviting
inline const ThemeDef lavender = {
    .light = {{"#faf8ff"}, {"#2d2640"}, {"#766d8a"}, {"#e6e0f0"}, {"#7c5cbf"}},
    .dark  = {{"#1a1724"}, {"#e0daea"}, {"#9890a8"}, {"#2a2538"}, {"#a78bfa"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        root() | set("tag-bg", mix(hex("#7c5cbf"), 8, v::bg)) | set("tag-text", hex("#7c5cbf")) | set("tag-hover-bg", hex("#7c5cbf")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", 14_px),
        dark_root() | set("tag-bg", mix(hex("#a78bfa"), 10, v::bg)) | set("tag-text", hex("#a78bfa")) | set("tag-hover-bg", hex("#a78bfa")) | set("tag-hover-text", hex("#1a1724")),
        "::selection"_s        | bg(hex("#7c5cbf")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#a78bfa")) | color(hex("#1a1724")),
        ".post-card"_s | border_radius(10_px) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        ".post-card:hover"_s | box_shadow(raw("0 2px 12px color-mix(in srgb, #7c5cbf 10%, transparent)")),
        ".post-listing"_s | transition(raw("background 0.15s")) | padding(14_px, 10_px) | margin(raw("0 -10px")) | border_radius(6_px),
        ".post-listing:hover"_s | bg(mix(hex("#7c5cbf"), 4, v::bg)),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left_color(hex("#7c5cbf")),
            "blockquote"_s.dark() | border_left_color(hex("#a78bfa")),
            "a"_s                 | color(hex("#6d4dab")),
            "a"_s.dark()          | color(hex("#b49afa"))
        )
    ),
};

} // namespace loom::theme::builtin
