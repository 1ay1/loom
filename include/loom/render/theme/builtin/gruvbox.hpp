#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Gruvbox — retro groove with warm, earthy contrast
inline const ThemeDef gruvbox = {
    .light = {{"#fbf1c7"}, {"#3c3836"}, {"#766a60"}, {"#d5c4a1"}, {"#d65d0e"}},
    .dark  = {{"#282828"}, {"#ebdbb2"}, {"#a89984"}, {"#3c3836"}, {"#fe8019"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .extra_css = R"CSS(
::selection {
  background: #d65d0e;
  color: #fbf1c7;
}

[data-theme="dark"] ::selection {
  background: #fe8019;
  color: #282828;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #b8bb26;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #b8bb26;
}

.post-content a, .page-content a {
  color: #458588;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #83a598;
}
)CSS",
};

} // namespace loom::theme::builtin
