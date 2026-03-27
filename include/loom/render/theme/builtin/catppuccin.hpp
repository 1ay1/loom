#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Catppuccin — soothing pastel theme for the high-spirited
inline const ThemeDef catppuccin = {
    .light = {{"#eff1f5"}, {"#4c4f69"}, {"#696b7c"}, {"#ccd0da"}, {"#8839ef"}},
    .dark  = {{"#1e1e2e"}, {"#cdd6f4"}, {"#a6adc8"}, {"#313244"}, {"#cba6f7"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
::selection {
  background: #8839ef;
  color: #eff1f5;
}

[data-theme="dark"] ::selection {
  background: #cba6f7;
  color: #1e1e2e;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ea76cb;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #f5c2e7;
}

.post-content a, .page-content a {
  color: #1e66f5;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #89b4fa;
}

:root {
  --tag-bg: color-mix(in srgb, #8839ef 12%, var(--bg));
  --tag-text: #8839ef;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #cba6f7 15%, #1e1e2e);
  --tag-text: #cba6f7;
}
)CSS",
};

} // namespace loom::theme::builtin
