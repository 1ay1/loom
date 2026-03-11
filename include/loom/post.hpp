#pragma once

#include "types.hpp"

#include <vector>
#include <chrono>

namespace loom
{
    struct Post
    {
        PostId id;
        Title title;
        Slug slug;
        Content content;

        std::vector<Tag> tags;
        std::chrono::system_clock::time_point published;
    };
}
