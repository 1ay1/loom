#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Solarized — Ethan Schoonover's precision-engineered palette
inline const ThemeDef solarized = {
    .light = {{"#fdf6e3"}, {"#657b83"}, {"#677171"}, {"#eee8d5"}, {"#268bd2"}},
    .dark  = {{"#002b36"}, {"#839496"}, {"#819297"}, {"#073642"}, {"#2aa198"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        sel("::selection") | bg(hex("#268bd2")) | color(hex("#fdf6e3")),
        dark(sel("::selection")) | bg(hex("#2aa198")) | color(hex("#002b36")),
        sel(".post-content blockquote", ".page-content blockquote") | border_left_color(hex("#cb4b16")),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#cb4b16"))
    ),
};

} // namespace loom::theme::builtin
