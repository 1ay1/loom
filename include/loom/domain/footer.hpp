#pragma once

#include <string>
#include <vector>

namespace loom
{
    struct FooterLink
    {
        std::string title;
        std::string url;
    };

    struct Footer
    {
        std::string copyright;
        std::vector<FooterLink> links;
    };
}
