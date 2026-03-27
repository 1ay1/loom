#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Mono — terminal/hacker aesthetic
inline const ThemeDef mono = {
    .light = {{"#f5f5f0"}, {"#1e1e1e"}, {"#666666"}, {"#d4d4d4"}, {"#16a34a"}},
    .dark  = {{"#0a0a0a"}, {"#d4d4d4"}, {"#7a7a7a"}, {"#262626"}, {"#4ade80"}},
    .font  = {"ui-monospace,SFMono-Regular,Menlo,Consolas,monospace"},
    .font_size = "15px",
    .max_width = "760px",
    .extra_css = {},
};

} // namespace loom::theme::builtin
