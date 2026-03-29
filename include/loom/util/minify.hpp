#pragma once

#include <string>

namespace loom
{
    std::string minify_html(const std::string& html);
    std::string minify_css_standalone(const std::string& css);
}
