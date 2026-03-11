#include "../../include/loom/render/render.hpp"
#include <string>
#include <vector>

namespace  loom
{
    std::string render_post(const Post& post)
    {
        std::string html;

        html += "<html>";
        html += "<body>";

        html += "<h1>";
        html += post.title.get();
        html += "</h1>";

        html += "<div>";
        html += post.content.get();
        html += "</div>";

        html += "</body>";
        html += "</html>";

        return html;
    }

    std::string render_index(const std::vector<PostSummary>& posts)
    {
        std::string html;

        html += "<html><body>";
        html += "<h1>Blog</h1>";

        for(const auto& post : posts)
        {
            html += "<a href=\"/post/";
            html += post.slug.get();
            html += "\">";

            html += post.title.get();
            html += "</a><br>";
        }

        html += "</body></html>";

        return html;
    }
}
