#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Catppuccin — soothing pastel theme for the high-spirited
inline const ThemeDef catppuccin = {
    .light = {{"#eff1f5"}, {"#4c4f69"}, {"#696b7c"}, {"#ccd0da"}, {"#8839ef"}},
    .dark  = {{"#1e1e2e"}, {"#cdd6f4"}, {"#a6adc8"}, {"#313244"}, {"#cba6f7"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .styles = sheet(
        "::selection"_s        | bg(hex("#8839ef")) | color(hex("#eff1f5")),
        "::selection"_s.dark() | bg(hex("#cba6f7")) | color(hex("#1e1e2e")),
        root() | set("tag-bg", mix(hex("#8839ef"), 12, v::bg)) | set("tag-text", hex("#8839ef")),
        dark_root() | set("tag-bg", mix(hex("#cba6f7"), 15, hex("#1e1e2e"))) | set("tag-text", hex("#cba6f7")),
        ".post-content"_s.also(".page-content").nest(
            "blockquote"_s        | border_left_color(hex("#ea76cb")),
            "blockquote"_s.dark() | border_left_color(hex("#f5c2e7")),
            "a"_s                 | color(hex("#1e66f5")),
            "a"_s.dark()          | color(hex("#89b4fa"))
        )
    ),
};

} // namespace loom::theme::builtin
