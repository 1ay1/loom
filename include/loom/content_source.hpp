#pragma once

#include "post.hpp"
#include "post_summary.hpp"

#include <concepts>
#include <optional>
#include <vector>

namespace loom
{
    template<typename T>
    concept ContentSource =
    requires(T source, Slug slug)
    {
        { source.list_posts() } -> std::same_as<std::vector<PostSummary>>;
        { source.get_post(slug) } -> std::same_as<std::optional<Post>>;
    };
}
