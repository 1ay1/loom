#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Gruvbox — retro groove with warm, earthy contrast
inline const ThemeDef gruvbox = {
    .light = {{"#fbf1c7"}, {"#3c3836"}, {"#766a60"}, {"#d5c4a1"}, {"#d65d0e"}},
    .dark  = {{"#282828"}, {"#ebdbb2"}, {"#a89984"}, {"#3c3836"}, {"#fe8019"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        sel("::selection") | bg(hex("#d65d0e")) | color(hex("#fbf1c7")),
        dark(sel("::selection")) | bg(hex("#fe8019")) | color(hex("#282828")),
        sel(".post-content blockquote", ".page-content blockquote") | border_left_color(hex("#b8bb26")),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#b8bb26")),
        sel(".post-content a", ".page-content a") | color(hex("#458588")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#83a598"))
    ),
};

} // namespace loom::theme::builtin
