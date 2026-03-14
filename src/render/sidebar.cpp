#include "../../include/loom/render/sidebar.hpp"

#include <ctime>

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

static std::string render_recent_posts(const Site& site, const SidebarData& data)
{
    int count = site.sidebar.recent_posts_count;
    std::string html;
    html += "<div class='widget'>";
    html += "<h3>Recent Posts</h3>";
    html += "<ul>";
    int i = 0;
    for (const auto& post : data.recent_posts)
    {
        if (i++ >= count) break;
        html += "<li><a href='/post/" + post.slug.get() + "'>" + post.title.get() + "</a>";
        html += " <span class='date'>" + format_date(post.published, site.layout.date_format) + "</span></li>";
    }
    html += "</ul>";
    html += "</div>";
    return html;
}

static std::string render_tag_cloud(const SidebarData& data)
{
    std::string html;
    html += "<div class='widget'>";
    html += "<h3>Tags</h3>";
    html += "<div class='post-tags'>";
    for (const auto& tag : data.tags)
        html += "<a class='tag' href='/tag/" + tag.get() + "'>" + tag.get() + "</a>";
    html += "</div>";
    html += "</div>";
    return html;
}

static std::string render_about(const Site& site)
{
    std::string html;
    html += "<div class='widget'>";
    html += "<h3>About</h3>";
    html += "<p>" + site.sidebar.about_text + "</p>";
    html += "</div>";
    return html;
}

std::string render_sidebar(const Site& site, const SidebarData& data)
{
    if (site.sidebar.widgets.empty())
        return "";

    std::string html;
    html += "<aside class='sidebar'>";

    for (const auto& widget : site.sidebar.widgets)
    {
        if (widget == "recent_posts")
            html += render_recent_posts(site, data);
        else if (widget == "tag_cloud")
            html += render_tag_cloud(data);
        else if (widget == "about" && !site.sidebar.about_text.empty())
            html += render_about(site);
    }

    html += "</aside>";
    return html;
}

}
