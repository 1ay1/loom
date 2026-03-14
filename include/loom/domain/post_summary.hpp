#pragma once

#include "../core/types.hpp"

#include <chrono>
#include <string>
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
        std::string excerpt;
        int reading_time_minutes = 0;
        std::chrono::system_clock::time_point modified_at;
    };
}
