#include "../../include/loom/render/post_renderer.hpp"
#include "../../include/loom/domain/post_summary.hpp"
#include <ctime>
#include <map>
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

    std::string render_post(const Post& post, const LayoutConfig& layout, const PostContext& ctx)
    {
        std::string html;

        html += "<article class='post-full'>";

        html += "<h1>";
        html += post.title.get();
        html += "</h1>";

        if (layout.show_post_dates || layout.show_reading_time)
        {
            html += "<div class='post-meta'>";
            if (layout.show_post_dates)
            {
                html += "<time>";
                html += format_date(post.published, layout.date_format);
                html += "</time>";
            }
            if (layout.show_reading_time && post.reading_time_minutes > 0)
            {
                if (layout.show_post_dates)
                    html += " &middot; ";
                html += std::to_string(post.reading_time_minutes) + " min read";
            }
            html += "</div>";
        }

        if (layout.show_post_tags && !post.tags.empty())
            html += render_tags(post.tags);

        // Series navigation
        if (!ctx.series_name.empty() && ctx.series_posts.size() > 1)
        {
            html += "<nav class='series-nav'>";
            html += "<p class='series-label'>Part of series: <strong>" + ctx.series_name + "</strong></p>";
            html += "<ol class='series-list'>";
            for (const auto& sp : ctx.series_posts)
            {
                if (sp.slug.get() == post.slug.get())
                    html += "<li class='series-current'>" + sp.title.get() + "</li>";
                else
                    html += "<li><a href='/post/" + sp.slug.get() + "'>" + sp.title.get() + "</a></li>";
            }
            html += "</ol>";
            html += "</nav>";
        }

        html += "<div class='post-content'>";
        html += post.content.get();
        html += "</div>";

        // Prev/Next navigation
        if (ctx.nav.prev || ctx.nav.next)
        {
            html += "<nav class='post-nav'>";
            if (ctx.nav.prev)
            {
                html += "<a class='post-nav-prev' href='/post/" + ctx.nav.prev->slug.get() + "'>";
                html += "&larr; " + ctx.nav.prev->title.get();
                html += "</a>";
            }
            else
            {
                html += "<span></span>";
            }
            if (ctx.nav.next)
            {
                html += "<a class='post-nav-next' href='/post/" + ctx.nav.next->slug.get() + "'>";
                html += ctx.nav.next->title.get() + " &rarr;";
                html += "</a>";
            }
            html += "</nav>";
        }

        // Related posts
        if (!ctx.related.empty())
        {
            html += "<section class='related-posts'>";
            html += "<h2>Related Posts</h2>";
            html += "<ul>";
            for (const auto& rp : ctx.related)
            {
                html += "<li><a href='/post/" + rp.slug.get() + "'>" + rp.title.get() + "</a>";
                if (layout.show_post_dates)
                    html += " <span class='date'>" + format_date(rp.published, layout.date_format) + "</span>";
                html += "</li>";
            }
            html += "</ul>";
            html += "</section>";
        }

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
                if (layout.show_post_dates || layout.show_reading_time)
                {
                    html += "<span class='date'>";
                    if (layout.show_post_dates)
                        html += format_date(post.published, layout.date_format);
                    if (layout.show_reading_time && post.reading_time_minutes > 0)
                    {
                        if (layout.show_post_dates)
                            html += " &middot; ";
                        html += std::to_string(post.reading_time_minutes) + " min";
                    }
                    html += "</span>";
                }
                if (layout.show_excerpts && !post.excerpt.empty())
                    html += "<p class='excerpt'>" + post.excerpt + "</p>";
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
                if (layout.show_reading_time && post.reading_time_minutes > 0)
                    html += " <span class='reading-time'>" + std::to_string(post.reading_time_minutes) + " min</span>";
                if (layout.show_post_tags && !post.tags.empty())
                    html += render_tags(post.tags);
                if (layout.show_excerpts && !post.excerpt.empty())
                    html += "<p class='excerpt'>" + post.excerpt + "</p>";
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
            if (layout.show_reading_time && post.reading_time_minutes > 0)
                html += " <span class='reading-time'>" + std::to_string(post.reading_time_minutes) + " min</span>";
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

    std::string render_archives(const std::map<int, std::vector<PostSummary>, std::greater<int>>& by_year, const LayoutConfig& layout)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Archives</h2>";

        for (const auto& [year, posts] : by_year)
        {
            html += "<h3>" + std::to_string(year) + "</h3>";
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
                html += "</article>";
            }
        }

        html += "</section>";
        return html;
    }

    std::string render_series_page(const std::string& series, const std::vector<PostSummary>& posts, const LayoutConfig& layout)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Series: " + series + "</h2>";

        html += "<ol class='series-list'>";
        for (const auto& post : posts)
        {
            html += "<li>";
            html += "<a href=\"/post/" + post.slug.get() + "\">" + post.title.get() + "</a>";
            if (layout.show_post_dates)
                html += " <span class='date'>" + format_date(post.published, layout.date_format) + "</span>";
            html += "</li>";
        }
        html += "</ol>";

        html += "</section>";
        return html;
    }

    std::string render_series_index(const std::vector<std::string>& series)
    {
        std::string html;
        html += "<section>";
        html += "<h2>Series</h2>";
        html += "<ul>";
        for (const auto& s : series)
            html += "<li><a href='/series/" + s + "'>" + s + "</a></li>";
        html += "</ul>";
        html += "</section>";
        return html;
    }
}
