#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Tokyo Night — neon-tinged Tokyo evening palette
inline const ThemeDef tokyonight = {
    .light = {{"#d5d6db"}, {"#343b58"}, {"#575b70"}, {"#c0c1c8"}, {"#7aa2f7"}},
    .dark  = {{"#1a1b26"}, {"#a9b1d6"}, {"#7d84a4"}, {"#292e42"}, {"#7aa2f7"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "720px",
    .extra_css = R"CSS(
::selection {
  background: #7aa2f7;
  color: #1a1b26;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ff9e64;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #ff9e64;
}

.post-content a, .page-content a {
  color: #7aa2f7;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #7dcfff;
}

.post-card:hover {
  border-color: #bb9af7;
}

:root {
  --tag-bg: color-mix(in srgb, #7aa2f7 12%, var(--bg));
  --tag-text: #7aa2f7;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #7aa2f7 15%, #1a1b26);
  --tag-text: #7dcfff;
}
)CSS",
};

} // namespace loom::theme::builtin
