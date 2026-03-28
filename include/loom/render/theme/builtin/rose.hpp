#pragma once
#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Rose — Soft pinks, elegant, magenta accent
// Warm and inviting with a refined editorial feel
inline const ThemeDef rose = {
    .light = {{"#fffbfc"}, {"#1a1118"}, {"#8e7a86"}, {"#f0dde4"}, {"#c2185b"}},
    .dark  = {{"#1a1118"}, {"#f5e6ec"}, {"#b09aa6"}, {"#2d2028"}, {"#f06292"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "17px",
    .max_width = "720px",
    .line_height = "1.75",
    .corners = Corners::Round,
    .nav_style = NavStyle::Pills,
    .tag_style = TagStyle::Pill,
    .card_hover = CardHover::Lift,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // Rosy selection
        "::selection"_s | bg(hex("#c2185b")) | color(hex("#fffbfc")),
        "::selection"_s.dark() | bg(hex("#f06292")) | color(hex("#1a1118")),

        // Soft code blocks
        content_area().nest(
            "pre"_s | bg(hex("#fdf2f5")) | border(1_px, solid, hex("#f0dde4")),
            "pre"_s.dark() | bg(hex("#231a1f")) | border(1_px, solid, hex("#2d2028")),
            ":not(pre)>code"_s | bg(hex("#fdf2f5")) | color(hex("#c2185b")) | padding(2_px, 6_px),
            ":not(pre)>code"_s.dark() | bg(hex("#231a1f")) | color(hex("#f06292")) | padding(2_px, 6_px),
            "blockquote"_s | border_left(3_px, solid, hex("#c2185b")) | color(hex("#8e7a86"))
                           | bg(hex("#fdf2f5")) | padding(10_px, 16_px),
            "blockquote"_s.dark() | border_left(3_px, solid, hex("#f06292")) | color(hex("#b09aa6"))
                                  | bg(hex("#231a1f"))
        ),

        // Cards with blush tint
        ".post-card"_s | bg(hex("#fdf6f8")),
        ".post-card"_s.dark() | bg(hex("#231a1f")) | border_color(hex("#2d2028")),

        // Pill nav hover with rose tint
        "nav a:hover"_s | bg(hex("#fdf2f5")),
        "nav a:hover"_s.dark() | bg(hex("#2d2028"))
    ),
};

} // namespace loom::theme::builtin
