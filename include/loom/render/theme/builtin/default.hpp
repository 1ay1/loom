#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Clean default — neutral grays, blue accent
inline const ThemeDef default_theme = {
    .light = {{"#ffffff"}, {"#0f172a"}, {"#64748b"}, {"#e5e7eb"}, {"#2563eb"}},
    .dark  = {{"#0b0f14"}, {"#e5e7eb"}, {"#94a3b8"}, {"#1f2937"}, {"#60a5fa"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
};

} // namespace loom::theme::builtin
