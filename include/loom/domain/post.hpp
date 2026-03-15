#pragma once

#include "../core/types.hpp"

#include <vector>
#include <chrono>
#include <string>

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

        bool draft = false;
        std::string excerpt;
        int reading_time_minutes = 0;
        Series series{""};
        std::chrono::system_clock::time_point modified_at;
    };
}
