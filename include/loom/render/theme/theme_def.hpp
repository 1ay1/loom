#pragma once

#include <string>
#include "palette.hpp"
#include "types.hpp"
#include "components.hpp"

namespace loom::theme
{

// Complete theme definition: palettes + typography + structure + escape hatch
//
// Fields with defaults can be omitted in designated initializers.
// A minimal theme only needs: light, dark, font, font_size, max_width.
// Structural fields default to the base CSS behavior.
struct ThemeDef
{
    // Colors
    Palette   light;
    Palette   dark;

    // Typography
    FontStack font;
    std::string font_size;
    std::string max_width;

    // Structure (all have defaults matching base CSS)
    Corners        corners    = Corners::Soft;
    TagStyle       tag_style  = TagStyle::Pill;
    LinkStyle      link_style = LinkStyle::Underline;
    CodeBlockStyle code_style = CodeBlockStyle::Plain;
    BlockquoteStyle quote_style = BlockquoteStyle::AccentBorder;
    HeadingCase    heading_case = HeadingCase::None;

    // Escape hatch for truly custom CSS
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
