#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Solarized — Ethan Schoonover's precision-engineered palette
inline const ThemeDef solarized = {
    .light = {{"#fdf6e3"}, {"#657b83"}, {"#677171"}, {"#eee8d5"}, {"#268bd2"}},
    .dark  = {{"#002b36"}, {"#839496"}, {"#819297"}, {"#073642"}, {"#2aa198"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
::selection {
  background: #268bd2;
  color: #fdf6e3;
}

[data-theme="dark"] ::selection {
  background: #2aa198;
  color: #002b36;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #cb4b16;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #cb4b16;
}
)CSS",
};

} // namespace loom::theme::builtin
