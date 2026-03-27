#pragma once

#include <string>
#include "palette.hpp"
#include "types.hpp"
#include "components.hpp"

namespace loom::theme
{

// Complete theme definition — 31 typed fields across 5 axes.
//
// Only the first 5 fields (light, dark, font, font_size, max_width) need
// to be specified. Everything else defaults to the base CSS behavior.
struct ThemeDef
{
    // ── Colors ──
    Palette   light;
    Palette   dark;

    // ── Typography ──
    FontStack   font;
    std::string font_size;
    std::string max_width;
    FontStack   heading_font   = {};   // empty = inherit
    FontStack   code_font      = {};   // empty = base CSS monospace
    std::string line_height    = {};   // empty = "1.7"
    std::string heading_weight = {};   // empty = "700"
    std::string header_size    = {};   // empty = "42px"

    // ── Shape & density ──
    Corners      corners      = Corners::Soft;
    Density      density      = Density::Normal;
    BorderWeight border_weight = BorderWeight::Normal;

    // ── Components ──
    NavStyle        nav_style    = NavStyle::Default;
    TagStyle        tag_style    = TagStyle::Pill;
    LinkStyle       link_style   = LinkStyle::Underline;
    CodeBlockStyle  code_style   = CodeBlockStyle::Plain;
    InlineCodeStyle inline_code  = InlineCodeStyle::Background;
    BlockquoteStyle quote_style  = BlockquoteStyle::AccentBorder;
    HeadingCase     heading_case = HeadingCase::None;
    ImageStyle      image_style  = ImageStyle::Default;
    CardHover       card_hover   = CardHover::Lift;
    HrStyle         hr_style     = HrStyle::Line;
    TableStyle      table_style  = TableStyle::Default;
    SidebarStyle    sidebar_style = SidebarStyle::Bordered;
    PostNavStyle    post_nav     = PostNavStyle::Default;
    Scrollbar       scrollbar    = Scrollbar::Default;
    FocusStyle      focus_style  = FocusStyle::Outline;

    // ── Escape hatch ──
    std::string extra_css = {};
};

// Derive a new theme by copying a base and applying overrides in-place
template<typename F>
ThemeDef derive(ThemeDef base, F&& overrides)
{
    overrides(base);
    return base;
}

} // namespace loom::theme
