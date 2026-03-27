#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Dracula — the mass-beloved dark-first theme
inline const ThemeDef dracula = {
    .light = {{"#f8f8f2"}, {"#282a36"}, {"#606fa0"}, {"#d6d6e6"}, {"#7c3aed"}},
    .dark  = {{"#282a36"}, {"#f8f8f2"}, {"#8592b8"}, {"#44475a"}, {"#bd93f9"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
::selection {
  background: #44475a;
  color: #f8f8f2;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ff79c6;
}

:root {
  --tag-bg: color-mix(in srgb, #7c3aed 12%, var(--bg));
  --tag-text: #7c3aed;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #bd93f9 20%, #282a36);
  --tag-text: #bd93f9;
}

.post-content a, .page-content a {
  color: #6838b2;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #8be9fd;
}

.post-card:hover {
  border-color: #ff79c6;
}
)CSS",
};

} // namespace loom::theme::builtin
