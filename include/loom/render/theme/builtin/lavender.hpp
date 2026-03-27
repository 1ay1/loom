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
        root() | set("tag-bg", mix(hex("#7c5cbf"), 8, v::bg)) | set("tag-text", hex("#7c5cbf")) | set("tag-hover-bg", hex("#7c5cbf")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", px(14)),
        dark_root() | set("tag-bg", mix(hex("#a78bfa"), 10, v::bg)) | set("tag-text", hex("#a78bfa")) | set("tag-hover-bg", hex("#a78bfa")) | set("tag-hover-text", hex("#1a1724")),
        sel("::selection") | bg(hex("#7c5cbf")) | color(hex("#ffffff")),
        dark(sel("::selection")) | bg(hex("#a78bfa")) | color(hex("#1a1724")),
        sel(".post-card") | border_radius(px(10)) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        sel(".post-card:hover") | box_shadow(raw("0 2px 12px color-mix(in srgb, #7c5cbf 10%, transparent)")),
        sel(".post-listing") | transition(raw("background 0.15s")) | padding(px(14), px(10)) | margin(raw("0 -10px")) | border_radius(px(6)),
        sel(".post-listing:hover") | bg(mix(hex("#7c5cbf"), 4, v::bg)),
        sel(".post-content blockquote", ".page-content blockquote") | border_left_color(hex("#7c5cbf")),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#a78bfa")),
        sel(".post-content a", ".page-content a") | color(hex("#6d4dab")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#b49afa"))
    ),
};

} // namespace loom::theme::builtin
