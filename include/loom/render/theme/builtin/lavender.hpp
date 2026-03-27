#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Lavender — soft purple tones, warm and inviting
inline const ThemeDef lavender = {
    .light = {{"#faf8ff"}, {"#2d2640"}, {"#766d8a"}, {"#e6e0f0"}, {"#7c5cbf"}},
    .dark  = {{"#1a1724"}, {"#e0daea"}, {"#9890a8"}, {"#2a2538"}, {"#a78bfa"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #7c5cbf 8%, var(--bg));
  --tag-text: #7c5cbf;
  --tag-hover-bg: #7c5cbf;
  --tag-hover-text: #ffffff;
  --tag-radius: 14px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #a78bfa 10%, var(--bg));
  --tag-text: #a78bfa;
  --tag-hover-bg: #a78bfa;
  --tag-hover-text: #1a1724;
}

::selection {
  background: #7c5cbf;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #a78bfa;
  color: #1a1724;
}

.post-card {
  border-radius: 10px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 2px 12px color-mix(in srgb, #7c5cbf 10%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #7c5cbf 4%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #7c5cbf;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #a78bfa;
}

.post-content a, .page-content a {
  color: #6d4dab;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #b49afa;
}
)CSS",
};

} // namespace loom::theme::builtin
