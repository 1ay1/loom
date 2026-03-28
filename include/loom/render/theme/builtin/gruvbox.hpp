#pragma once
#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Gruvbox — Retro groove, warm earthy contrast
// Based on morhetz/gruvbox color scheme
namespace {
    const auto gruvbox_orange  = hex("#d65d0e");
    const auto gruvbox_borange = hex("#fe8019");
    const auto gruvbox_cream   = hex("#fbf1c7");
    const auto gruvbox_dark    = hex("#282828");
}

inline const ThemeDef gruvbox = {
    .light = {{gruvbox_cream.v}, {"#3c3836"}, {"#7c6f64"}, {"#d5c4a1"}, {gruvbox_orange.v}},
    .dark  = {{gruvbox_dark.v},  {"#ebdbb2"}, {"#a89984"}, {"#3c3836"}, {gruvbox_borange.v}},
    .font  = {"-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica Neue,Arial,sans-serif"},
    .font_size = "15.5px",
    .max_width = "720px",
    .line_height = "1.7",
    .corners = Corners::Soft,
    .tag_style = TagStyle::Bordered,
    .card_hover = CardHover::Border,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // Warm selection
        "::selection"_s | bg(gruvbox_orange) | color(gruvbox_cream),
        "::selection"_s.dark() | bg(gruvbox_borange) | color(gruvbox_dark),

        // Code blocks & blockquotes — uses palette vars, works in both modes
        content_area().nest(
            "pre"_s | border(1_px, solid, raw("var(--border)")),
            "blockquote"_s | border_left(3_px, solid, raw("var(--accent)"))
                           | color(raw("var(--muted)")) | padding(10_px, 16_px)
        ),

        // Cards
        ".post-card:hover"_s | border_color(raw("var(--accent)")),

        // Tags with gruvbox warmth
        vars({{"tag-bg", transparent}, {"tag-text", hex("#7c6f64")}, {"tag-hover-bg", gruvbox_orange},
              {"tag-hover-text", gruvbox_cream}})
    ),
};

} // namespace loom::theme::builtin
