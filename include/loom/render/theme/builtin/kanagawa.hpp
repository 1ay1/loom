#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Kanagawa — inspired by Katsushika Hokusai's famous painting
inline const ThemeDef kanagawa = {
    .light = {{"#f2ecbc"}, {"#1F1F28"}, {"#696961"}, {"#d8d3ab"}, {"#957fb8"}},
    .dark  = {{"#1F1F28"}, {"#DCD7BA"}, {"#8a8983"}, {"#2A2A37"}, {"#957FB8"}},
    .font  = {"Charter,Georgia,Cambria,serif"},
    .font_size = "17px",
    .max_width = "700px",
    .styles = sheet(
        sel("::selection") | bg(hex("#957fb8")) | color(hex("#DCD7BA")),
        sel(".post-content blockquote", ".page-content blockquote") | border_left_color(hex("#FF5D62")),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#E82424")),
        sel(".post-content a", ".page-content a") | color(hex("#6a9589")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#7FB4CA")),
        root() | set("tag-bg", mix(hex("#957fb8"), 15, v::bg)) | set("tag-text", hex("#957fb8")),
        dark_root() | set("tag-bg", mix(hex("#957fb8"), 20, hex("#1F1F28"))) | set("tag-text", hex("#938AA9"))
    ),
};

} // namespace loom::theme::builtin
