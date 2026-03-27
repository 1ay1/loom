#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

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
    .border_weight = BorderWeight::Thin,
    .nav_style = NavStyle::Minimal,
    .tag_style = TagStyle::Outline,
    .link_style = LinkStyle::Dashed,
    .quote_style = BlockquoteStyle::MutedBorder,
    .heading_case = HeadingCase::Upper,
    .card_hover = CardHover::Border,
    .hr_style = HrStyle::Dashed,
    .table_style = TableStyle::Minimal,
    .post_nav = PostNavStyle::Minimal,
    .styles = sheet(
        sel("::selection") | bg(hex("#222222")) | color(hex("#f4f1ea")),
        dark(sel("::selection")) | bg(hex("#c8c0b0")) | color(hex("#1a1814")),
        sel(".tag") | text_transform(uppercase) | font_size(px(11)) | letter_spacing(px(1)),
        sel("header h1") | letter_spacing(px(4)) | font_weight(400),
        sel(".post-card") | border_style(dashed),
        sel(".post-content blockquote", ".page-content blockquote") | font_style(italic)
    ),
};

} // namespace loom::theme::builtin
