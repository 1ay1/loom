#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{
using namespace css;

// Brutalist — raw, functional, anti-design design
inline const ThemeDef brutalist = {
    .light = {{"#ffffff"}, {"#000000"}, {"#555555"}, {"#000000"}, {"#ff0000"}},
    .dark  = {{"#000000"}, {"#ffffff"}, {"#aaaaaa"}, {"#ffffff"}, {"#ff4444"}},
    .font  = {"system-ui,-apple-system,Segoe UI,Roboto,sans-serif"},
    .font_size = "16px",
    .max_width = "760px",
    .heading_weight = "900",
    .corners = Corners::Sharp,
    .border_weight = BorderWeight::Thick,
    .code_style = CodeBlockStyle::Bordered,
    .heading_case = HeadingCase::Upper,
    .image_style = ImageStyle::Bordered,
    .table_style = TableStyle::Bordered,
    .focus_style = FocusStyle::Ring,
    .styles = sheet(
        "::selection"_s        | bg(hex("#ff0000")) | color(hex("#ffffff")),
        "::selection"_s.dark() | bg(hex("#ff4444")) | color(hex("#000000")),
        "header h1"_s | letter_spacing(6_px),
        ".post-card"_s | border_color(v::text),
        ".post-card:hover"_s | border_color(v::accent) | bg(v::accent) | color(v::bg),
        ".post-card:hover a"_s | color(v::bg),
        ".post-card:hover .date"_s | color(v::bg) | opacity(0.7),
        root() | set("tag-bg", v::text) | set("tag-text", v::bg) | set("tag-radius", raw("0")) | set("tag-hover-bg", v::accent) | set("tag-hover-text", v::bg),
        ".tag"_s | text_transform(uppercase) | font_weight(700) | font_size(11_px) | letter_spacing(1_px),
        ".post-content"_s.also(".page-content").nest(
            "pre"_s               | bg(mix(v::text, 12, v::bg)),
            ":not(pre) > code"_s  | border(1_px, solid, v::text) | bg(mix(v::text, 8, v::bg)),
            "blockquote"_s        | border_left(6_px, solid, v::accent) | font_weight(700) | color(v::text),
            "img"_s               | border_width(2_px) | border_color(v::text),
            "a"_s                 | text_decoration(none) | bg(raw("linear-gradient(var(--accent), var(--accent)) no-repeat 0 100%")) | background_size(raw("100% 2px")) | padding_bottom(1_px),
            "a:hover"_s           | background_size(raw("100% 100%")) | color(v::bg)
        )
    ),
};

} // namespace loom::theme::builtin
