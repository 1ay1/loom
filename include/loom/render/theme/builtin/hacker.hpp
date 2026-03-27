#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Hacker — monospace, dark, no gimmicks
inline const ThemeDef hacker = {
    .light = {{"#f0f0e8"}, {"#1a1a1a"}, {"#5a5a5a"}, {"#c8c8b8"}, {"#2d8a2d"}},
    .dark  = {{"#0c0c0c"}, {"#b5b5b5"}, {"#7b7b7b"}, {"#1a1a1a"}, {"#88c070"}},
    .font  = {"ui-monospace,SFMono-Regular,Menlo,Consolas,monospace"},
    .font_size = "14px",
    .max_width = "800px",
    .corners = Corners::Sharp,
    .tag_style = TagStyle::Outline,
    .code_style = CodeBlockStyle::Bordered,
    .quote_style = BlockquoteStyle::MutedBorder,
    .extra_css = R"CSS(
::selection {
  background: #88c070;
  color: #0c0c0c;
}
)CSS",
};

} // namespace loom::theme::builtin
