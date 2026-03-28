#pragma once
#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Nord — Arctic color palette by Arctic Ice Studio
// Muted blues and frost tones, calm and focused
inline const ThemeDef nord = {
    .light = {{"#eceff4"}, {"#2e3440"}, {"#4c566a"}, {"#d8dee9"}, {"#5e81ac"}},
    .dark  = {{"#2e3440"}, {"#eceff4"}, {"#d8dee9"}, {"#3b4252"}, {"#88c0d0"}},
    .font  = {"Inter,-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica Neue,sans-serif"},
    .font_size = "15px",
    .max_width = "740px",
    .line_height = "1.7",
    .corners = Corners::Soft,
    .card_hover = CardHover::Glow,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // Subtle frost-tinted selection
        "::selection"_s | bg(hex("#5e81ac")) | color(hex("#eceff4")),
        "::selection"_s.dark() | bg(hex("#88c0d0")) | color(hex("#2e3440")),

        // Code blocks & blockquotes — uses palette vars
        content_area().nest(
            "pre"_s | border(1_px, solid, raw("var(--border)")),
            "blockquote"_s | border_left(3_px, solid, raw("var(--accent)"))
                           | color(raw("var(--muted)")) | padding(10_px, 16_px)
        )
    ),
};

} // namespace loom::theme::builtin
