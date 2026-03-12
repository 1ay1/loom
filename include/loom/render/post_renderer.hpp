#pragma once

#include "../domain/domain.hpp"

#include <string>
#include <vector>

namespace loom
{
    std::string render_post(const Post& post);
    std::string render_index(const std::vector<PostSummary>& posts);
}
