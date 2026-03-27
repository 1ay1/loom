#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Typewriter — old-school ink-on-paper, Courier, raw
inline const ThemeDef typewriter = {
    .light = {{"#f4f1ea"}, {"#222222"}, {"#6c6c6c"}, {"#d1cdc4"}, {"#222222"}},
    .dark  = {{"#1a1814"}, {"#c8c0b0"}, {"#878279"}, {"#2a2620"}, {"#c8c0b0"}},
    .font  = {"Courier New,Courier,monospace"},
    .font_size = "16px",
    .max_width = "640px",
    .extra_css = R"CSS(
::selection {
  background: #222222;
  color: #f4f1ea;
}

[data-theme="dark"] ::selection {
  background: #c8c0b0;
  color: #1a1814;
}

header h1 {
  text-transform: uppercase;
  letter-spacing: 4px;
  font-weight: 400;
}

.post-full h1 {
  text-transform: uppercase;
  letter-spacing: 2px;
}

:root {
  --tag-bg: transparent;
  --tag-text: var(--text);
  --tag-radius: 0;
}

.tag {
  border: 1px solid var(--text);
  text-transform: uppercase;
  font-size: 11px;
  letter-spacing: 1px;
}

.post-card {
  border-radius: 0;
  border-style: dashed;
}

.post-content pre, .page-content pre {
  border-radius: 0;
  border: 1px dashed var(--border);
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 0;
}

.post-content blockquote, .page-content blockquote {
  border-left: 2px solid var(--text);
  font-style: italic;
}

.theme-toggle {
  border-radius: 0;
}

.post-content img, .page-content img {
  border-radius: 0;
}

.post-content a, .page-content a {
  text-decoration: underline;
  text-decoration-style: dashed;
  text-underline-offset: 3px;
}
)CSS",
};

} // namespace loom::theme::builtin
