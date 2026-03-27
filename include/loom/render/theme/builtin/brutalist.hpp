#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Brutalist — raw, functional, anti-design design
inline const ThemeDef brutalist = {
    .light = {{"#ffffff"}, {"#000000"}, {"#555555"}, {"#000000"}, {"#ff0000"}},
    .dark  = {{"#000000"}, {"#ffffff"}, {"#aaaaaa"}, {"#ffffff"}, {"#ff4444"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "760px",
    .corners = Corners::Sharp,
    .code_style = CodeBlockStyle::Bordered,
    .heading_case = HeadingCase::Upper,
    .image_style = ImageStyle::Bordered,
    .table_style = TableStyle::Bordered,
    .extra_css = R"CSS(
::selection {
  background: #ff0000;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #ff4444;
  color: #000000;
}

header {
  border-bottom-width: 3px;
}

header h1 {
  letter-spacing: 6px;
  font-weight: 900;
}

footer {
  border-top-width: 3px;
}

.post-full h1 {
  font-weight: 900;
}

.post-card {
  border-width: 2px;
  border-color: var(--text);
}

.post-card:hover {
  border-color: var(--accent);
  background: var(--accent);
  color: var(--bg);
}

.post-card:hover a {
  color: var(--bg);
}

.post-card:hover .date {
  color: var(--bg);
  opacity: 0.7;
}

:root {
  --tag-bg: var(--text);
  --tag-text: var(--bg);
  --tag-radius: 0;
  --tag-hover-bg: var(--accent);
  --tag-hover-text: var(--bg);
}

.tag {
  text-transform: uppercase;
  font-weight: 700;
  font-size: 11px;
  letter-spacing: 1px;
}

.post-content pre, .page-content pre {
  border-width: 2px;
  background: color-mix(in srgb, var(--text) 12%, var(--bg));
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border: 1px solid var(--text);
  background: color-mix(in srgb, var(--text) 8%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left: 6px solid var(--accent);
  font-weight: 700;
  color: var(--text);
}

.theme-toggle {
  border: 2px solid var(--text);
}

.post-content img, .page-content img {
  border-width: 2px;
  border-color: var(--text);
}

.post-content a, .page-content a {
  text-decoration: none;
  background: linear-gradient(var(--accent), var(--accent)) no-repeat 0 100%;
  background-size: 100% 2px;
  padding-bottom: 1px;
}

.post-content a:hover, .page-content a:hover {
  background-size: 100% 100%;
  color: var(--bg);
}

.sidebar {
  border-left: 3px solid var(--text);
}

.sidebar-left .sidebar {
  border-right: 3px solid var(--text);
  border-left: none;
}
)CSS",
};

} // namespace loom::theme::builtin
