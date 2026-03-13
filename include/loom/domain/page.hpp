#pragma once

#include "../core/types.hpp"

#include <string>

namespace loom
{
    struct Page
    {
        Slug slug;
        Title title;
        Content content;
    };
}
