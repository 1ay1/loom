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
    .font  = {"Optima,Candara,Noto Sans,-apple-system,BlinkMacSystemFont,sans-serif"},
    .font_size = "15.5px",
    .max_width = "700px",
    .line_height = "1.7",
    .corners = Corners::Round,
    .nav_style = NavStyle::Pills,
    .tag_style = TagStyle::Pill,
    .card_hover = CardHover::Lift,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // Rosy selection
        "::selection"_s | bg(hex("#c2185b")) | color(hex("#fffbfc")),
        "::selection"_s.dark() | bg(hex("#f06292")) | color(hex("#1a1118")),

        // Code blocks & blockquotes — uses palette vars
        content_area().nest(
            "pre"_s | border(1_px, solid, raw("var(--border)")),
            "blockquote"_s | border_left(3_px, solid, raw("var(--accent)"))
                           | color(raw("var(--muted)")) | padding(10_px, 16_px)
        ),

        // Pill nav hover
        "nav a:hover"_s | bg(raw("color-mix(in srgb, var(--accent) 8%, var(--bg))"))
    ),
};

} // namespace loom::theme::builtin
