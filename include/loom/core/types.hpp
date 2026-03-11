#pragma once

#include "strong_type.hpp"
#include <string>

namespace loom
{
    struct SlugTag {};
    struct TitleTag {};
    struct PostIdTag {};
    struct TagTag {};
    struct ContentTag {};

    using Slug = StrongType<std::string, SlugTag>;
    using Title = StrongType<std::string, TitleTag>;
    using PostId = StrongType<std::string, PostIdTag>;
    using Tag = StrongType<std::string, TagTag>;
    using Content = StrongType<std::string, ContentTag>;

}
