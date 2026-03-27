#pragma once

#include <string>
#include "palette.hpp"
#include "types.hpp"
#include "components.hpp"

namespace loom::theme
{

// Complete theme definition.
//
// Only the first 5 fields (light, dark, font, font_size, max_width) are
// required. Everything else has a default that matches the base CSS behavior.
// A maximally opinionated theme can set every field; a minimal theme sets five.
struct ThemeDef
{
    // --- Colors ---
    Palette   light;
    Palette   dark;

    // --- Typography ---
    FontStack   font;
    std::string font_size;
    std::string max_width;
    FontStack   heading_font = {};   // empty = inherit from font
    FontStack   code_font    = {};   // empty = base CSS monospace stack
    std::string line_height  = {};   // empty = base CSS default ("1.7")

    // --- Shape & density ---
    Corners  corners = Corners::Soft;
    Density  density = Density::Normal;

    // --- Component styles ---
    TagStyle        tag_style  = TagStyle::Pill;
    LinkStyle       link_style = LinkStyle::Underline;
    CodeBlockStyle  code_style = CodeBlockStyle::Plain;
    BlockquoteStyle quote_style = BlockquoteStyle::AccentBorder;
    HeadingCase     heading_case = HeadingCase::None;
    ImageStyle      image_style = ImageStyle::Default;
    CardHover       card_hover  = CardHover::Lift;
    HrStyle         hr_style    = HrStyle::Line;
    TableStyle      table_style = TableStyle::Default;

    // --- Escape hatch ---
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
