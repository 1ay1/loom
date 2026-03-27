#pragma once

#include "types.hpp"

namespace loom::theme
{

// A complete set of color tokens for one mode (light or dark)
struct Palette
{
    Color bg;
    Color text;
    Color muted;
    Color border;
    Color accent;
};

} // namespace loom::theme
