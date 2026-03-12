#include "../../include/loom/render/render.hpp"
#include <string>
#include <vector>

namespace  loom
{
    std::string render_post(const Post& post)
    {
        std::string html;

        html += "<article>";

        html += "<h1>";
        html += post.title.get();
        html += "</h1>";

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
            html += "<h3><a href=\"/post/";
            html += post.slug.get();
            html += "\">";
            html += post.title.get();
            html += "</a></h3>";
            html += "</article>";
        }

        return html;
    }
}
