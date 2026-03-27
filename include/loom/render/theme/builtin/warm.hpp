#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Warm — golden amber palette, cozy reading feel
inline const ThemeDef warm = {
    .light = {{"#fdf8f0"}, {"#33261a"}, {"#816d5a"}, {"#e8dcc8"}, {"#c47a20"}},
    .dark  = {{"#1a1510"}, {"#ddd0b8"}, {"#8a7e68"}, {"#2e2618"}, {"#dda040"}},
    .font  = {"Georgia,Charter,Cambria,Times New Roman,serif"},
    .font_size = "18px",
    .max_width = "700px",
    .line_height = "1.8",
    .density = Density::Airy,
    .image_style = ImageStyle::Shadow,
    .hr_style = HrStyle::Fade,
    .styles = sheet(
        root() | set("tag-bg", mix(hex("#c47a20"), 10, v::bg)) | set("tag-text", hex("#a06418")) | set("tag-hover-bg", hex("#c47a20")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", 4_px) | set("heading-weight", raw("600")),
        dark_root() | set("tag-bg", mix(hex("#dda040"), 10, v::bg)) | set("tag-text", hex("#dda040")) | set("tag-hover-bg", hex("#dda040")) | set("tag-hover-text", hex("#1a1510")),
        "::selection"_s        | bg(hex("#c47a20")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#dda040")) | color(hex("#1a1510")),
        "header h1"_s | font_weight(600),
        ".post-card"_s | border_radius(6_px) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        ".post-card:hover"_s | box_shadow(raw("0 2px 10px color-mix(in srgb, #c47a20 8%, transparent)")),
        ".post-listing a"_s | font_weight(500),
        ".post-full h1"_s | font_weight(600) | line_height(num(1.25)),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left(3_px, solid, hex("#c47a20")) | font_style(italic),
            "blockquote"_s.dark() | border_left_color(hex("#dda040")),
            "a"_s                 | color(hex("#a06418")),
            "a"_s.dark()          | color(hex("#e8b060"))
        )
    ),
};

} // namespace loom::theme::builtin
