#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Nord — arctic color palette
inline const ThemeDef nord = {
    .light = {{"#eceff4"}, {"#2e3440"}, {"#4c566a"}, {"#d8dee9"}, {"#5e81ac"}},
    .dark  = {{"#2e3440"}, {"#d8dee9"}, {"#81a1c1"}, {"#3b4252"}, {"#88c0d0"}},
    .font  = {"Inter,system-ui,-apple-system,sans-serif"},
    .font_size = "16px",
    .max_width = "720px",
    .extra_css = {},
};

} // namespace loom::theme::builtin
