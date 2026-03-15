#pragma once

#include "../domain/post.hpp"
#include "../domain/post_summary.hpp"
#include "../domain/site.hpp"
#include "../core/types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace loom
{
    struct PostNavigation
    {
        std::optional<PostSummary> prev;    // older
        std::optional<PostSummary> next;    // newer
    };

    struct PostContext
    {
        PostNavigation nav;
        std::vector<PostSummary> related;
        std::vector<PostSummary> series_posts;
        Series series_name{""};
    };

    std::string render_post(const Post& post, const LayoutConfig& layout, const PostContext& ctx = {});
    std::string render_index(const std::vector<PostSummary>& posts, const LayoutConfig& layout);
    std::string render_tag_page(const Tag& tag, const std::vector<PostSummary>& posts, const LayoutConfig& layout);
    std::string render_tag_index(const std::vector<Tag>& tags);
    std::string render_archives(const std::map<int, std::vector<PostSummary>, std::greater<int>>& by_year, const LayoutConfig& layout);
    std::string render_series_page(const Series& series, const std::vector<PostSummary>& posts, const LayoutConfig& layout);
    std::string render_series_index(const std::vector<Series>& series);
}
