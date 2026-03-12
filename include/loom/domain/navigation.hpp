#pragma once

#include <string>
#include <vector>

namespace loom
{

    struct NavItem
    {
        std::string title;
        std::string url;
    };

    struct Navigation
    {
        std::vector<NavItem> items;
    };

}
