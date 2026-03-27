#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Earth — warm, organic tones
inline const ThemeDef earth = {
    .light = {{"#faf6f0"}, {"#2d2418"}, {"#7a6e5e"}, {"#ddd4c4"}, {"#946b2d"}},
    .dark  = {{"#1a1610"}, {"#d4c8b0"}, {"#8a7e6e"}, {"#2d2618"}, {"#d4a04a"}},
    .font  = {"Charter,Georgia,Cambria,serif"},
    .font_size = "17px",
    .max_width = "700px",
    .extra_css = {},
};

} // namespace loom::theme::builtin
