#pragma once

#include "post.hpp"
#include "page.hpp"
#include "navigation.hpp"
#include "theme.hpp"
#include "footer.hpp"

#include <string>
#include <vector>

namespace loom
{

struct Site
{
    std::string title;
    std::string description;
    std::string author;

    std::vector<Post> posts;
    std::vector<Page> pages;
    Navigation navigation;
    Theme theme;
    Footer footer;
};

}
