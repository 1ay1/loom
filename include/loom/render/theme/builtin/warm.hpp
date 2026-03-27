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
        root() | set("tag-bg", mix(hex("#c47a20"), 10, v::bg)) | set("tag-text", hex("#a06418")) | set("tag-hover-bg", hex("#c47a20")) | set("tag-hover-text", hex("#ffffff")) | set("tag-radius", px(4)) | set("heading-weight", raw("600")),
        dark_root() | set("tag-bg", mix(hex("#dda040"), 10, v::bg)) | set("tag-text", hex("#dda040")) | set("tag-hover-bg", hex("#dda040")) | set("tag-hover-text", hex("#1a1510")),
        sel("::selection") | bg(hex("#c47a20")) | color(hex("#ffffff")),
        dark(sel("::selection")) | bg(hex("#dda040")) | color(hex("#1a1510")),
        sel("header h1") | font_weight(600),
        sel(".post-card") | border_radius(px(6)) | transition(raw("border-color 0.2s, box-shadow 0.2s")),
        sel(".post-card:hover") | box_shadow(raw("0 2px 10px color-mix(in srgb, #c47a20 8%, transparent)")),
        sel(".post-listing a") | font_weight(500),
        sel(".post-full h1") | font_weight(600) | line_height(num(1.25)),
        sel(".post-content blockquote", ".page-content blockquote") | border_left(px(3), solid, hex("#c47a20")) | font_style(italic),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#dda040")),
        sel(".post-content a", ".page-content a") | color(hex("#a06418")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#e8b060"))
    ),
};

} // namespace loom::theme::builtin
