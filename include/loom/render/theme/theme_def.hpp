#pragma once

#include <string>
#include "palette.hpp"
#include "types.hpp"

namespace loom::theme
{

// Complete theme definition: two palettes + typography + escape hatch
struct ThemeDef
{
    Palette   light;
    Palette   dark;
    FontStack font;
    std::string font_size;
    std::string max_width;
    std::string extra_css;
};

// Derive a new theme by copying a base and applying overrides in-place
//
//   auto variant = derive(gruvbox, [](ThemeDef& t) {
//       t.light.accent = {"#ff0000"};
//   });
//
template<typename F>
ThemeDef derive(ThemeDef base, F&& overrides)
{
    overrides(base);
    return base;
}

} // namespace loom::theme
