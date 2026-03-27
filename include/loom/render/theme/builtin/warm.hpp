#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Warm — golden amber palette, cozy reading feel
inline const ThemeDef warm = {
    .light = {{"#fdf8f0"}, {"#33261a"}, {"#816d5a"}, {"#e8dcc8"}, {"#c47a20"}},
    .dark  = {{"#1a1510"}, {"#ddd0b8"}, {"#8a7e68"}, {"#2e2618"}, {"#dda040"}},
    .font  = {"Georgia,Charter,Cambria,Times New Roman,serif"},
    .font_size = "18px",
    .max_width = "700px",
    .extra_css = R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #c47a20 10%, var(--bg));
  --tag-text: #a06418;
  --tag-hover-bg: #c47a20;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
  --heading-weight: 600;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #dda040 10%, var(--bg));
  --tag-text: #dda040;
  --tag-hover-bg: #dda040;
  --tag-hover-text: #1a1510;
}

::selection {
  background: #c47a20;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #dda040;
  color: #1a1510;
}

header h1 {
  font-weight: 600;
}

.post-card {
  border-radius: 6px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 2px 10px color-mix(in srgb, #c47a20 8%, transparent);
}

.post-listing a {
  font-weight: 500;
}

.post-full h1 {
  font-weight: 600;
  line-height: 1.25;
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #c47a20;
  font-style: italic;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #dda040;
}

.post-content a, .page-content a {
  color: #a06418;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #e8b060;
}
)CSS",
};

} // namespace loom::theme::builtin
