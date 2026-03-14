#pragma once

#include "../core/types.hpp"

#include <chrono>
#include <vector>

namespace loom
{
    struct PostSummary
    {
        PostId id;
        Title title;
        Slug slug;
        std::chrono::system_clock::time_point published;
        std::vector<Tag> tags;
    };
}
