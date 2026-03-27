#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Cobalt — deep blue developer theme
inline const ThemeDef cobalt = {
    .light = {{"#f0f4f8"}, {"#1b2a4a"}, {"#546e8e"}, {"#d0dce8"}, {"#0066cc"}},
    .dark  = {{"#0d1b2a"}, {"#c8d6e5"}, {"#6b8db5"}, {"#1b2d45"}, {"#48a9fe"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = {},
};

} // namespace loom::theme::builtin
