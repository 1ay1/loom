#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Rose — soft pinks, elegant
inline const ThemeDef rose = {
    .light = {{"#fdf2f4"}, {"#1c1017"}, {"#7a6872"}, {"#e8d5db"}, {"#be185d"}},
    .dark  = {{"#1c1017"}, {"#e8d5db"}, {"#9a8590"}, {"#2d1f27"}, {"#f472b6"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
};

} // namespace loom::theme::builtin
