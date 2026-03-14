#include "../../include/loom/domain/post.hpp"
#include "../../include/loom/domain/post_summary.hpp"
#include "../../include/loom/domain/site.hpp"
#include <ctime>
#include <string>
#include <vector>

namespace loom
{
    static std::string format_date(std::chrono::system_clock::time_point tp, const std::string& fmt)
    {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
        gmtime_r(&time, &tm);

        char buf[64];
        std::strftime(buf, sizeof(buf), fmt.c_str(), &tm);
        return buf;
    }

    static std::string render_tags(const std::vector<Tag>& tags)
    {
        std::string html;
        html += "<div class='post-tags'>";
        for (const auto& tag : tags)
            html += "<a class='tag' href='/tag/" + tag.get() + "'>" + tag.get() + "</a>";
        html += "</div>";
        return html;
    }

    std::string render_post(const Post& post, const LayoutConfig& layout)
    {
        std::string html;

        html += "<article class='post-full'>";

        html += "<h1>";
        html += post.title.get();
        html += "</h1>";

        if (layout.show_post_dates)
        {
            html += "<div class='post-meta'>";
            html += "<time>";
            html += format_date(post.published, layout.date_format);
            html += "</time>";
            html += "</div>";
        }

        if (layout.show_post_tags && !post.tags.empty())
            html += render_tags(post.tags);

        html += "<div class='post-content'>";
        html += post.content.get();
        html += "</div>";

        html += "</article>";

        return html;
    }

    std::string render_index(const std::vector<PostSummary>& posts, const LayoutConfig& layout)
    {
        std::string html;

        html += "<section>";
        html += "<h2>Recent Posts</h2>";

        if (layout.post_list_style == "cards")
        {
            html += "<div class='post-cards'>";
            for (const auto& post : posts)
            {
                html += "<div class='post-card'>";
                html += "<a href=\"/post/" + post.slug.get() + "\">" + post.title.get() + "</a>";
                if (layout.show_post_dates)
                    html += "<span class='date'>" + format_date(post.published, layout.date_format) + "</span>";
                if (layout.show_post_tags && !post.tags.empty())
                    html += render_tags(post.tags);
                html += "</div>";
            }
            html += "</div>";
        }
        else
        {
            for (const auto& post : posts)
            {
                html += "<article class='post-listing'>";
                if (layout.show_post_dates)
                {
                    html += "<span class='date'>";
                    html += format_date(post.published, layout.date_format);
                    html += "</span> ";
                }
                html += "<a href=\"/post/";
                html += post.slug.get();
                html += "\">";
                html += post.title.get();
                html += "</a>";
                if (layout.show_post_tags && !post.tags.empty())
                    html += render_tags(post.tags);
                html += "</article>";
            }
        }

        html += "</section>";
        return html;
    }

    std::string render_tag_page(const Tag& tag, const std::vector<PostSummary>& posts, const LayoutConfig& layout)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Posts tagged &ldquo;" + tag.get() + "&rdquo;</h2>";

        for (const auto& post : posts)
        {
            html += "<article class='post-listing'>";
            if (layout.show_post_dates)
            {
                html += "<span class='date'>";
                html += format_date(post.published, layout.date_format);
                html += "</span> ";
            }
            html += "<a href=\"/post/" + post.slug.get() + "\">";
            html += post.title.get();
            html += "</a>";
            if (layout.show_post_tags && !post.tags.empty())
                html += render_tags(post.tags);
            html += "</article>";
        }

        html += "</section>";
        return html;
    }

    std::string render_tag_index(const std::vector<Tag>& tags)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Tags</h2>";
        html += "<div class='post-tags'>";
        for (const auto& tag : tags)
            html += "<a class='tag' href='/tag/" + tag.get() + "'>" + tag.get() + "</a>";
        html += "</div>";
        html += "</section>";
        return html;
    }
}
