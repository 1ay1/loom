#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Editorial — serif font, warm tones, literary feel
inline const ThemeDef serif = {
    .light = {{"#faf8f5"}, {"#1a1a1a"}, {"#6b6b6b"}, {"#e0dcd4"}, {"#8b4513"}},
    .dark  = {{"#1a1712"}, {"#e8e0d4"}, {"#9a9080"}, {"#2e2920"}, {"#d4956a"}},
    .font  = {"Georgia,Garamond,Times New Roman,serif"},
    .font_size = "18px",
    .max_width = "680px",
    .extra_css = {},
};

} // namespace loom::theme::builtin
