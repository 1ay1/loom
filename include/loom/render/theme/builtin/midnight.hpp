#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Midnight — rich dark-first with electric blue, polished
inline const ThemeDef midnight = {
    .light = {{"#f0f2f8"}, {"#1e2030"}, {"#5a6080"}, {"#d0d4e0"}, {"#3b82f6"}},
    .dark  = {{"#0e1018"}, {"#c8cce0"}, {"#757c98"}, {"#1a1e30"}, {"#60a5fa"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "740px",
    .extra_css = R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #3b82f6 8%, var(--bg));
  --tag-text: #3b82f6;
  --tag-hover-bg: #3b82f6;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #60a5fa 10%, var(--bg));
  --tag-text: #60a5fa;
  --tag-hover-bg: #60a5fa;
  --tag-hover-text: #0e1018;
}

::selection {
  background: #3b82f6;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #60a5fa;
  color: #0e1018;
}

header h1 {
  font-weight: 800;
  letter-spacing: -0.5px;
}

.post-card {
  border-radius: 8px;
  transition: border-color 0.2s, box-shadow 0.2s, transform 0.2s;
}

.post-card:hover {
  border-color: color-mix(in srgb, #3b82f6 30%, var(--border));
  box-shadow: 0 4px 16px color-mix(in srgb, #3b82f6 8%, transparent);
  transform: translateY(-1px);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #3b82f6 3%, var(--bg));
}

.post-full h1 {
  font-weight: 800;
  letter-spacing: -0.5px;
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #3b82f6;
  background: color-mix(in srgb, #3b82f6 3%, var(--bg));
  border-radius: 0 6px 6px 0;
  padding: 10px 14px;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #60a5fa;
  background: color-mix(in srgb, #60a5fa 4%, var(--bg));
}

.post-content a, .page-content a {
  color: #2563eb;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #78b4fc;
}

.post-content pre, .page-content pre {
  border-radius: 8px;
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 4px;
}
)CSS",
};

} // namespace loom::theme::builtin
