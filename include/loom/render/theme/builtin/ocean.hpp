#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Ocean — deep blue, calm and professional
inline const ThemeDef ocean = {
    .light = {{"#f4f7fb"}, {"#1a2744"}, {"#5a6e8a"}, {"#d4dce8"}, {"#1a6fb5"}},
    .dark  = {{"#0c1420"}, {"#c8d4e4"}, {"#6880a0"}, {"#182840"}, {"#4da8e8"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #1a6fb5 8%, var(--bg));
  --tag-text: #1a6fb5;
  --tag-hover-bg: #1a6fb5;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #4da8e8 10%, var(--bg));
  --tag-text: #4da8e8;
  --tag-hover-bg: #4da8e8;
  --tag-hover-text: #0c1420;
}

::selection {
  background: #1a6fb5;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #4da8e8;
  color: #0c1420;
}

header {
  border-bottom-color: color-mix(in srgb, #1a6fb5 15%, var(--border));
}

.post-card {
  border-radius: 8px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 3px 14px color-mix(in srgb, #1a6fb5 10%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #1a6fb5 3%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #1a6fb5;
  background: color-mix(in srgb, #1a6fb5 3%, var(--bg));
  border-radius: 0 4px 4px 0;
  padding: 10px 14px;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #4da8e8;
  background: color-mix(in srgb, #4da8e8 4%, var(--bg));
}

.post-content a, .page-content a {
  color: #1a6fb5;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #68b8f0;
}
)CSS",
};

} // namespace loom::theme::builtin
