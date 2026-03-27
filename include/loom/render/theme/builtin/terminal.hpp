#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Terminal — monospace, sharp, restrained. Let typography do the work.
inline const ThemeDef terminal = {
    .light = {{"#fafaf8"}, {"#1c1c1c"}, {"#737373"}, {"#e5e5e0"}, {"#0c4a6e"}},
    .dark  = {{"#0c0c0c"}, {"#d4d4d4"}, {"#737373"}, {"#252525"}, {"#7dd3fc"}},
    .font  = {"ui-monospace,SFMono-Regular,Menlo,Consolas,Liberation Mono,monospace"},
    .font_size = "14px",
    .max_width = "820px",
    .corners = Corners::Sharp,
    .tag_style = TagStyle::Bordered,
    .link_style = LinkStyle::Dotted,
    .code_style = CodeBlockStyle::LeftAccent,
    .quote_style = BlockquoteStyle::MutedBorder,
    .card_hover = CardHover::Border,
    .hr_style = HrStyle::Fade,
    .table_style = TableStyle::Minimal,
};

} // namespace loom::theme::builtin
