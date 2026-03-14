#pragma once

#include <map>
#include <string>

namespace loom
{

struct ThemeColors
{
    // Light mode
    std::string bg, text, muted, border, accent;
    std::string font;
    std::string font_size;
    std::string max_width;

    // Dark mode
    std::string dark_bg, dark_text, dark_muted, dark_border, dark_accent;

    // Optional extra CSS appended after variable overrides
    std::string extra_css;
};

const std::map<std::string, ThemeColors>& builtin_themes();

std::string theme_to_css(const ThemeColors& colors);

}
