#include "../../include/loom/domain/post.hpp"
#include "../../include/loom/domain/post_summary.hpp"
#include <ctime>
#include <string>
#include <vector>

namespace loom
{
    static std::string format_date(std::chrono::system_clock::time_point tp)
    {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
        gmtime_r(&time, &tm);

        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        return buf;
    }

    std::string render_post(const Post& post)
    {
        std::string html;

        html += "<article>";

        html += "<h1>";
        html += post.title.get();
        html += "</h1>";

        html += "<time>";
        html += format_date(post.published);
        html += "</time>";

        if (!post.tags.empty())
        {
            html += "<div class='post-tags'>";
            for (const auto& tag : post.tags)
                html += "<a class='tag' href='/tag/" + tag.get() + "'>" + tag.get() + "</a>";
            html += "</div>";
        }

        html += "<div class='post-content'>";
        html += post.content.get();
        html += "</div>";

        html += "</article>";

        return html;
    }

    std::string render_index(const std::vector<PostSummary>& posts)
    {
        std::string html;

        html += "<section>";
        html += "<h2>Recent Posts</h2>";

        for(const auto& post : posts)
        {
            html += "<article>";
            html += "<span class='date'>";
            html += format_date(post.published);
            html += "</span> ";
            html += "<a href=\"/post/";
            html += post.slug.get();
            html += "\">";
            html += post.title.get();
            html += "</a>";
            if (!post.tags.empty())
            {
                html += " ";
                for (const auto& tag : post.tags)
                    html += "<a class='tag' href='/tag/" + tag.get() + "'>" + tag.get() + "</a>";
            }
            html += "</article>";
        }

        return html;
    }

    std::string render_tag_page(const Tag& tag, const std::vector<PostSummary>& posts)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Posts tagged &ldquo;" + tag.get() + "&rdquo;</h2>";

        for (const auto& post : posts)
        {
            html += "<article>";
            html += "<span class='date'>";
            html += format_date(post.published);
            html += "</span> ";
            html += "<a href=\"/post/" + post.slug.get() + "\">";
            html += post.title.get();
            html += "</a>";
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
