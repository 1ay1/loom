#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Kanagawa — inspired by Katsushika Hokusai's famous painting
inline const ThemeDef kanagawa = {
    .light = {{"#f2ecbc"}, {"#1F1F28"}, {"#696961"}, {"#d8d3ab"}, {"#957fb8"}},
    .dark  = {{"#1F1F28"}, {"#DCD7BA"}, {"#8a8983"}, {"#2A2A37"}, {"#957FB8"}},
    .font  = {"Charter,Georgia,Cambria,serif"},
    .font_size = "17px",
    .max_width = "700px",
    .extra_css = R"CSS(
::selection {
  background: #957fb8;
  color: #DCD7BA;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #FF5D62;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #E82424;
}

.post-content a, .page-content a {
  color: #6a9589;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #7FB4CA;
}

:root {
  --tag-bg: color-mix(in srgb, #957fb8 15%, var(--bg));
  --tag-text: #957fb8;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #957fb8 20%, #1F1F28);
  --tag-text: #938AA9;
}
)CSS",
};

} // namespace loom::theme::builtin
