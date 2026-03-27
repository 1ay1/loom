#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Sakura — cherry blossom pinks, delicate and refined
inline const ThemeDef sakura = {
    .light = {{"#fef9fa"}, {"#3a2030"}, {"#876978"}, {"#ecd8e0"}, {"#d4447a"}},
    .dark  = {{"#1c1218"}, {"#e0c8d4"}, {"#967887"}, {"#2e1e28"}, {"#f07098"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        root() | set("tag-bg", mix(hex("#d4447a"), 7, v::bg)) | set("tag-text", hex("#c03868")) | set("tag-hover-bg", hex("#d4447a")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", 14_px),
        dark_root() | set("tag-bg", mix(hex("#f07098"), 10, v::bg)) | set("tag-text", hex("#f07098")) | set("tag-hover-bg", hex("#f07098")) | set("tag-hover-text", hex("#1c1218")),
        "::selection"_s        | bg(hex("#d4447a")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#f07098")) | color(hex("#1c1218")),
        ".post-card"_s | border_radius(10_px) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        ".post-card:hover"_s | border_color(mix(hex("#d4447a"), 30, v::border)) | box_shadow(raw("0 2px 12px color-mix(in srgb, #d4447a 8%, transparent)")),
        ".post-listing"_s | transition(raw("background 0.15s")) | padding(14_px, 10_px) | margin(raw("0 -10px")) | border_radius(6_px),
        ".post-listing:hover"_s | bg(mix(hex("#d4447a"), 3, v::bg)),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left_color(hex("#d4447a")),
            "blockquote"_s.dark() | border_left_color(hex("#f07098")),
            "a"_s                 | color(hex("#c03868")),
            "a"_s.dark()          | color(hex("#f48aaa"))
        )
    ),
};

} // namespace loom::theme::builtin
