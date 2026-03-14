#pragma once

#include "../domain/site.hpp"
#include "../domain/post_summary.hpp"
#include "../core/types.hpp"

#include <string>
#include <vector>

namespace loom
{

struct SidebarData
{
    std::vector<PostSummary> recent_posts;
    std::vector<Tag> tags;
};

std::string render_sidebar(const Site& site, const SidebarData& data);

}
