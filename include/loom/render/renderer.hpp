#pragma once

#include "../domain/post.hpp"
#include "../domain/post_summary.hpp"

#include <string>
#include <vector>

namespace loom
{
    std::string render_post(const Post& post);
    std::string render_index(const std::vector<PostSummary>& posts);
}
