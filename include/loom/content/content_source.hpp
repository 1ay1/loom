#pragma once

#include "../domain/post.hpp"
#include "../domain/page.hpp"

#include <concepts>
#include <vector>

namespace loom
{
    template<typename T>
    concept ContentSource =
    requires(T source)
    {
        { source.all_posts() } -> std::same_as<std::vector<Post>>;
        { source.all_pages() } -> std::same_as<std::vector<Page>>;
    };
}
