#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Sakura — cherry blossom pinks, delicate and refined
inline const ThemeDef sakura = {
    .light = {{"#fef9fa"}, {"#3a2030"}, {"#876978"}, {"#ecd8e0"}, {"#d4447a"}},
    .dark  = {{"#1c1218"}, {"#e0c8d4"}, {"#967887"}, {"#2e1e28"}, {"#f07098"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #d4447a 7%, var(--bg));
  --tag-text: #c03868;
  --tag-hover-bg: #d4447a;
  --tag-hover-text: #ffffff;
  --tag-radius: 14px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #f07098 10%, var(--bg));
  --tag-text: #f07098;
  --tag-hover-bg: #f07098;
  --tag-hover-text: #1c1218;
}

::selection {
  background: #d4447a;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #f07098;
  color: #1c1218;
}

.post-card {
  border-radius: 10px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  border-color: color-mix(in srgb, #d4447a 30%, var(--border));
  box-shadow: 0 2px 12px color-mix(in srgb, #d4447a 8%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #d4447a 3%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #d4447a;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #f07098;
}

.post-content a, .page-content a {
  color: #c03868;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #f48aaa;
}
)CSS",
};

} // namespace loom::theme::builtin
