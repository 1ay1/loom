#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Dracula — the mass-beloved dark-first theme
inline const ThemeDef dracula = {
    .light = {{"#f8f8f2"}, {"#282a36"}, {"#606fa0"}, {"#d6d6e6"}, {"#7c3aed"}},
    .dark  = {{"#282a36"}, {"#f8f8f2"}, {"#8592b8"}, {"#44475a"}, {"#bd93f9"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        "::selection"_s | bg(hex("#44475a")) | color(hex("#f8f8f2")),
        root() | set("tag-bg", mix(hex("#7c3aed"), 12, v::bg)) | set("tag-text", hex("#7c3aed")),
        dark_root() | set("tag-bg", mix(hex("#bd93f9"), 20, hex("#282a36"))) | set("tag-text", hex("#bd93f9")),
        ".post-card:hover"_s | border_color(hex("#ff79c6")),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left_color(hex("#ff79c6")),
            "a"_s                 | color(hex("#6838b2")),
            "a"_s.dark()          | color(hex("#8be9fd"))
        )
    ),
};

} // namespace loom::theme::builtin
