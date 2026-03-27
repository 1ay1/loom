#pragma once

#include <string>

namespace loom::theme
{

// Strong type for CSS color values
struct Color
{
    std::string value;
};

// Strong type for font-family stacks
struct FontStack
{
    std::string value;
};

} // namespace loom::theme
