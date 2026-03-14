#pragma once

#include <map>
#include <string>

namespace loom
{
    struct Theme
    {
        std::string name = "default";
        std::string css;
        std::map<std::string, std::string> variables;
    };
}
