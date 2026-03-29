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
        bool featured = false;
        std::string excerpt;
        std::string image;
        int reading_time_minutes = 0;
        Series series{""};
        std::chrono::system_clock::time_point modified_at;
    };
}
