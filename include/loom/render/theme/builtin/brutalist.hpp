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
        sel("::selection") | bg(hex("#ff0000")) | color(hex("#ffffff")),
        dark(sel("::selection")) | bg(hex("#ff4444")) | color(hex("#000000")),
        sel("header h1") | letter_spacing(px(6)),
        sel(".post-card") | border_color(v::text),
        sel(".post-card:hover") | border_color(v::accent) | bg(v::accent) | color(v::bg),
        sel(".post-card:hover a") | color(v::bg),
        sel(".post-card:hover .date") | color(v::bg) | opacity(0.7),
        root() | set("tag-bg", v::text) | set("tag-text", v::bg) | set("tag-radius", raw("0")) | set("tag-hover-bg", v::accent) | set("tag-hover-text", v::bg),
        sel(".tag") | text_transform(uppercase) | font_weight(700) | font_size(px(11)) | letter_spacing(px(1)),
        sel(".post-content pre", ".page-content pre") | bg(mix(v::text, 12, v::bg)),
        sel(".post-content :not(pre) > code", ".page-content :not(pre) > code") | border(px(1), solid, v::text) | bg(mix(v::text, 8, v::bg)),
        sel(".post-content blockquote", ".page-content blockquote") | border_left(px(6), solid, v::accent) | font_weight(700) | color(v::text),
        sel(".post-content img", ".page-content img") | border_width(px(2)) | border_color(v::text),
        sel(".post-content a", ".page-content a") | text_decoration(none) | bg(raw("linear-gradient(var(--accent), var(--accent)) no-repeat 0 100%")) | background_size(raw("100% 2px")) | padding_bottom(px(1)),
        sel(".post-content a:hover", ".page-content a:hover") | background_size(raw("100% 100%")) | color(v::bg)
    ),
};

} // namespace loom::theme::builtin
