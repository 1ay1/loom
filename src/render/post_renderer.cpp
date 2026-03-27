#include "../../include/loom/render/post_renderer.hpp"
#include "../../include/loom/render/dom.hpp"
#include "../../include/loom/domain/post_summary.hpp"
#include <ctime>
#include <map>
#include <string>
#include <vector>

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

// ── Shared components ──

static Node tag_list(const std::vector<Tag>& tags)
{
    return div(class_("post-tags"),
        each(tags, [](const Tag& t) {
            return a(class_("tag"), href("/tag/" + t.get()), t.get());
        })
    );
}

static Node post_listing_node(const PostSummary& post, const LayoutConfig& layout)
{
    return article(class_("post-listing"),
        when(layout.show_post_dates,
            span(class_("date"), format_date(post.published, layout.date_format))),
        raw(" "),
        a(href("/post/" + post.slug.get()), post.title.get()),
        when(layout.show_reading_time && post.reading_time_minutes > 0,
            span(class_("reading-time"),
                raw(" " + std::to_string(post.reading_time_minutes) + " min"))),
        when(layout.show_post_tags && !post.tags.empty(),
            tag_list(post.tags)),
        when(layout.show_excerpts && !post.excerpt.empty(),
            p_(class_("excerpt"), post.excerpt))
    );
}

static Node post_card_node(const PostSummary& post, const LayoutConfig& layout)
{
    return div(class_("post-card"),
        a(href("/post/" + post.slug.get()), post.title.get()),
        when(layout.show_post_dates || layout.show_reading_time,
            span(class_("date"),
                when(layout.show_post_dates,
                    text(format_date(post.published, layout.date_format))),
                when(layout.show_reading_time && post.reading_time_minutes > 0,
                    raw((layout.show_post_dates ? " &middot; " : "") +
                        std::to_string(post.reading_time_minutes) + " min")))),
        when(layout.show_excerpts && !post.excerpt.empty(),
            p_(class_("excerpt"), post.excerpt)),
        when(layout.show_post_tags && !post.tags.empty(),
            tag_list(post.tags))
    );
}

// ── Public render functions ──

std::string render_post(const Post& post, const LayoutConfig& layout, const PostContext& ctx)
{
    return article(class_("post-full"),
        h1(post.title.get()),

        when(layout.show_post_dates || layout.show_reading_time,
            div(class_("post-meta"),
                when(layout.show_post_dates,
                    time_(format_date(post.published, layout.date_format))),
                when(layout.show_reading_time && post.reading_time_minutes > 0,
                    raw((layout.show_post_dates ? " &middot; " : "") +
                        std::to_string(post.reading_time_minutes) + " min read")))),

        when(layout.show_post_tags && !post.tags.empty(),
            div(class_("post-full post-tags"), tag_list(post.tags))),

        // Series navigation
        when(!ctx.series_name.empty() && ctx.series_posts.size() > 1,
            nav(class_("series-nav"),
                p_(class_("series-label"),
                    raw("Part of series: <strong>" + ctx.series_name.get() + "</strong>")),
                ol(class_("series-list"),
                    each(ctx.series_posts, [&](const PostSummary& sp) -> Node {
                        if (sp.slug.get() == post.slug.get())
                            return li(class_("series-current"), sp.title.get());
                        return li(a(href("/post/" + sp.slug.get()), sp.title.get()));
                    })))),

        div(class_("post-content"), raw(post.content.get())),

        // Prev/Next navigation
        when(ctx.nav.prev || ctx.nav.next,
            nav(class_("post-nav"),
                ctx.nav.prev
                    ? a(class_("post-nav-prev"), href("/post/" + ctx.nav.prev->slug.get()),
                        raw("&larr; " + ctx.nav.prev->title.get()))
                    : span(),
                ctx.nav.next
                    ? a(class_("post-nav-next"), href("/post/" + ctx.nav.next->slug.get()),
                        raw(ctx.nav.next->title.get() + " &rarr;"))
                    : Node{Node::Fragment, {}, {}, {}, {}})),

        // Related posts
        when(!ctx.related.empty(),
            section(class_("related-posts"),
                h2("Related Posts"),
                ul(each(ctx.related, [&](const PostSummary& rp) {
                    return li(
                        a(href("/post/" + rp.slug.get()), rp.title.get()),
                        when(layout.show_post_dates,
                            span(class_("date"),
                                raw(" " + format_date(rp.published, layout.date_format)))));
                }))))
    ).render();
}

std::string render_index(const std::vector<PostSummary>& posts, const LayoutConfig& layout)
{
    if (layout.post_list_style == "cards")
    {
        return section(
            h2("Recent Posts"),
            div(class_("post-cards"),
                each(posts, [&](const PostSummary& p) {
                    return post_card_node(p, layout);
                }))
        ).render();
    }

    return section(
        h2("Recent Posts"),
        each(posts, [&](const PostSummary& p) {
            return post_listing_node(p, layout);
        })
    ).render();
}

std::string render_tag_page(const Tag& tag, const std::vector<PostSummary>& posts, const LayoutConfig& layout)
{
    return section(
        h2(raw("Posts tagged &ldquo;" + tag.get() + "&rdquo;")),
        each(posts, [&](const PostSummary& p) {
            return post_listing_node(p, layout);
        })
    ).render();
}

std::string render_tag_index(const std::vector<Tag>& tags)
{
    return section(
        h2("Tags"),
        div(class_("post-tags"),
            each(tags, [](const Tag& t) {
                return a(class_("tag"), href("/tag/" + t.get()), t.get());
            }))
    ).render();
}

std::string render_archives(const std::map<int, std::vector<PostSummary>, std::greater<int>>& by_year, const LayoutConfig& layout)
{
    return section(
        h2("Archives"),
        each(by_year, [&](const auto& pair) {
            return fragment(
                h3(std::to_string(pair.first)),
                each(pair.second, [&](const PostSummary& p) {
                    return article(class_("post-listing"),
                        when(layout.show_post_dates,
                            span(class_("date"), format_date(p.published, layout.date_format))),
                        raw(" "),
                        a(href("/post/" + p.slug.get()), p.title.get()));
                }));
        })
    ).render();
}

std::string render_series_page(const Series& series, const std::vector<PostSummary>& posts, const LayoutConfig& layout)
{
    return section(
        h2(raw("Series: " + series.get())),
        ol(class_("series-list"),
            each(posts, [&](const PostSummary& p) {
                return li(
                    a(href("/post/" + p.slug.get()), p.title.get()),
                    when(layout.show_post_dates,
                        span(class_("date"),
                            raw(" " + format_date(p.published, layout.date_format)))));
            }))
    ).render();
}

std::string render_series_index(const std::vector<Series>& series)
{
    return section(
        h2("Series"),
        ul(each(series, [](const Series& s) {
            return li(a(href("/series/" + s.get()), s.get()));
        }))
    ).render();
}

}
