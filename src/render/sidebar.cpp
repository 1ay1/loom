#include "../../include/loom/render/sidebar.hpp"
#include "../../include/loom/render/dom.hpp"
#include <ctime>

namespace loom
{

using namespace dom;

static std::string format_date(std::chrono::system_clock::time_point tp, const std::string& fmt)
{
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);

    char buf[64];
    std::strftime(buf, sizeof(buf), fmt.c_str(), &tm);
    return buf;
}

static Node widget(const char* heading, Node content)
{
    return div(class_("widget"),
        h3(heading),
        std::move(content)
    );
}

static Node render_recent_posts(const Site& site, const SidebarData& data)
{
    int count = site.sidebar.recent_posts_count;
    int i = 0;
    return widget("Recent Posts",
        ul(each(data.recent_posts, [&](const PostSummary& post) -> Node {
            if (i++ >= count) return Node{Node::Fragment};
            return li(
                a(href("/post/" + post.slug.get()), post.title.get()),
                span(class_("date"),
                    raw(" " + format_date(post.published, site.layout.date_format))));
        }))
    );
}

static Node render_tag_cloud(const SidebarData& data)
{
    return widget("Tags",
        div(class_("post-tags"),
            each(data.tags, [](const Tag& t) {
                return a(class_("tag"), href("/tag/" + t.get()), t.get());
            }))
    );
}

static Node render_archives_widget()
{
    return widget("Archives",
        p_(a(href("/archives"), "Browse all posts by year"))
    );
}

static Node render_series_widget(const SidebarData& data)
{
    if (data.series.empty()) return Node{Node::Fragment};
    return widget("Series",
        ul(each(data.series, [](const Series& s) {
            return li(a(href("/series/" + s.get()), s.get()));
        }))
    );
}

static Node render_about(const Site& site)
{
    return widget("About",
        p_(site.sidebar.about_text)
    );
}

std::string render_sidebar(const Site& site, const SidebarData& data)
{
    if (site.sidebar.widgets.empty())
        return "";

    auto sidebar_node = aside(class_("sidebar"),
        each(site.sidebar.widgets, [&](const std::string& w) -> Node {
            if (w == "recent_posts") return render_recent_posts(site, data);
            if (w == "tag_cloud")    return render_tag_cloud(data);
            if (w == "archives")     return render_archives_widget();
            if (w == "series")       return render_series_widget(data);
            if (w == "about" && !site.sidebar.about_text.empty())
                return render_about(site);
            return Node{Node::Fragment};
        })
    );

    return sidebar_node.render();
}

}
