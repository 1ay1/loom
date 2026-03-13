#pragma once

#include "../core/types.hpp"

#include <chrono>

namespace loom
{
    struct PostSummary
    {
        PostId id;
        Title title;
        Slug slug;
        std::chrono::system_clock::time_point published;
    };
}
