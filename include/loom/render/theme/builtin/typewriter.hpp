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
    .line_height = "1.8",
    .corners = Corners::Sharp,
    .density = Density::Airy,
    .tag_style = TagStyle::Outline,
    .link_style = LinkStyle::Dashed,
    .quote_style = BlockquoteStyle::MutedBorder,
    .heading_case = HeadingCase::Upper,
    .card_hover = CardHover::Border,
    .hr_style = HrStyle::Dashed,
    .extra_css = R"CSS(
::selection {
  background: #222222;
  color: #f4f1ea;
}

[data-theme="dark"] ::selection {
  background: #c8c0b0;
  color: #1a1814;
}

.tag {
  text-transform: uppercase;
  font-size: 11px;
  letter-spacing: 1px;
}

header h1 {
  letter-spacing: 4px;
  font-weight: 400;
}

.post-card {
  border-style: dashed;
}

.post-content blockquote, .page-content blockquote {
  font-style: italic;
}
)CSS",
};

} // namespace loom::theme::builtin
