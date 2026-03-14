#pragma once

#include "../domain/post.hpp"
#include "../domain/site.hpp"
#include "../core/types.hpp"

#include <string>
#include <vector>

namespace loom
{
    std::string render_post(const Post& post, const LayoutConfig& layout);
    std::string render_index(const std::vector<PostSummary>& posts, const LayoutConfig& layout);
    std::string render_tag_page(const Tag& tag, const std::vector<PostSummary>& posts, const LayoutConfig& layout);
    std::string render_tag_index(const std::vector<Tag>& tags);
}
