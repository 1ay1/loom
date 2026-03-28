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
    .font  = {"Inter,system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "740px",
    .line_height = "1.7",
    .corners = Corners::Soft,
    .card_hover = CardHover::Glow,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // Subtle frost-tinted selection
        "::selection"_s | bg(hex("#5e81ac")) | color(hex("#eceff4")),
        "::selection"_s.dark() | bg(hex("#88c0d0")) | color(hex("#2e3440")),

        // Soften code blocks with nord palette
        content_area().nest(
            "pre"_s | bg(hex("#e5e9f0")) | border(1_px, solid, hex("#d8dee9")),
            "pre"_s.dark() | bg(hex("#3b4252")) | border(1_px, solid, hex("#434c5e")),
            ":not(pre)>code"_s | bg(hex("#e5e9f0")) | padding(2_px, 5_px),
            ":not(pre)>code"_s.dark() | bg(hex("#3b4252")) | padding(2_px, 5_px),
            "blockquote"_s | border_left(3_px, solid, hex("#5e81ac"))
                           | color(hex("#4c566a")) | bg(hex("#e5e9f0")) | padding(10_px, 16_px),
            "blockquote"_s.dark() | border_left(3_px, solid, hex("#88c0d0"))
                                  | color(hex("#d8dee9")) | bg(hex("#3b4252"))
        ),

        // Cards with frost background
        ".post-card"_s.dark() | bg(hex("#3b4252")) | border_color(hex("#434c5e"))
    ),
};

} // namespace loom::theme::builtin
