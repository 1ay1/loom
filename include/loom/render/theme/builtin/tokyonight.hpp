#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Tokyo Night — neon-tinged Tokyo evening palette
inline const ThemeDef tokyonight = {
    .light = {{"#d5d6db"}, {"#343b58"}, {"#575b70"}, {"#c0c1c8"}, {"#7aa2f7"}},
    .dark  = {{"#1a1b26"}, {"#a9b1d6"}, {"#7d84a4"}, {"#292e42"}, {"#7aa2f7"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "720px",
    .styles = sheet(
        sel("::selection") | bg(hex("#7aa2f7")) | color(hex("#1a1b26")),
        sel(".post-content blockquote", ".page-content blockquote") | border_left_color(hex("#ff9e64")),
        dark(sel(".post-content blockquote", ".page-content blockquote")) | border_left_color(hex("#ff9e64")),
        sel(".post-content a", ".page-content a") | color(hex("#7aa2f7")),
        dark(sel(".post-content a", ".page-content a")) | color(hex("#7dcfff")),
        sel(".post-card:hover") | border_color(hex("#bb9af7")),
        root() | set("tag-bg", mix(hex("#7aa2f7"), 12, v::bg)) | set("tag-text", hex("#7aa2f7")),
        dark_root() | set("tag-bg", mix(hex("#7aa2f7"), 15, hex("#1a1b26"))) | set("tag-text", hex("#7dcfff"))
    ),
};

} // namespace loom::theme::builtin
