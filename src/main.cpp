#include <iostream>
#include <string>
#include <unordered_map>
#include <ctime>
#include <sstream>
#include "util/gzip.hpp"

#include "loom/content/content_source.hpp"
#include "loom/content/filesystem_source.hpp"
#include "loom/engine/site_builder.hpp"
#include "loom/engine/blog_engine.hpp"
#include "loom/render/layout.hpp"
#include "loom/render/navigation.hpp"
#include "loom/render/post_renderer.hpp"
#include "loom/render/page_renderer.hpp"
#include "loom/render/sidebar.hpp"
#include "loom/http/server.hpp"
#include "loom/http/response.hpp"

struct CachedPage
{
    std::string raw;
    std::string gzipped;
    std::string etag;
};

static CachedPage make_cached(std::string html)
{
    // Simple hash-based ETag
    auto hash = std::hash<std::string>{}(html);
    std::string etag = "\"" + std::to_string(hash) + "\"";
    auto gz = loom::gzip_compress(html);
    return {std::move(html), std::move(gz), std::move(etag)};
}

static bool accepts_gzip(const loom::HttpRequest& req)
{
    auto ae = req.header("Accept-Encoding");
    return ae.find("gzip") != std::string::npos;
}

static loom::HttpResponse serve_cached(const CachedPage& page, const loom::HttpRequest& req,
    const std::string& content_type = "text/html; charset=utf-8")
{
    // Check If-None-Match for 304
    auto inm = req.header("If-None-Match");
    if (!inm.empty() && inm == page.etag)
    {
        loom::HttpResponse resp;
        resp.status = 304;
        resp.headers["ETag"] = page.etag;
        resp.headers["Cache-Control"] = "public, max-age=60, must-revalidate";
        return resp;
    }

    loom::HttpResponse resp;
    resp.status = 200;
    resp.headers["Content-Type"] = content_type;
    resp.headers["ETag"] = page.etag;
    resp.headers["Cache-Control"] = "public, max-age=60, must-revalidate";

    if (accepts_gzip(req) && !page.gzipped.empty())
    {
        resp.body = page.gzipped;
        resp.headers["Content-Encoding"] = "gzip";
    }
    else
    {
        resp.body = page.raw;
    }
    return resp;
}

static std::string format_iso_date(std::chrono::system_clock::time_point tp)
{
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

static std::string format_rfc822_date(std::chrono::system_clock::time_point tp)
{
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    return buf;
}

static std::string xml_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

int main(int argc, char* argv[])
{
    std::string content_dir = "content";
    if (argc > 1)
        content_dir = argv[1];

    std::cout << "Loading content from: " << content_dir << std::endl;

    loom::FileSystemSource source(content_dir);

    auto config = source.site_config();
    auto site = loom::build_site(source, std::move(config));

    loom::BlogEngine engine(site);

    // Pre-render all pages at startup
    auto nav = loom::render_navigation(site.navigation);

    // Build sidebar
    auto all_tags = engine.all_tags();
    loom::SidebarData sidebar_data{engine.list_posts(), all_tags};
    auto sidebar_html = loom::render_sidebar(site, sidebar_data);

    std::unordered_map<std::string, CachedPage> cache;

    // Pre-render index
    {
        loom::PageMeta meta;
        meta.canonical_path = "/";
        cache["/"] = make_cached(loom::render_layout(site, nav, loom::render_index(engine.list_posts(), site.layout), sidebar_html, meta));
    }

    // Pre-render all posts
    for (const auto& post : site.posts)
    {
        loom::PageMeta meta;
        meta.title = post.title.get();
        meta.canonical_path = "/post/" + post.slug.get();
        meta.og_type = "article";
        meta.published_date = format_iso_date(post.published);
        meta.author = site.author;
        for (const auto& t : post.tags)
            meta.tags.push_back(t.get());

        // Use first 160 chars of content as description (strip HTML)
        std::string plain;
        bool in_tag = false;
        for (char c : post.content.get())
        {
            if (c == '<') { in_tag = true; continue; }
            if (c == '>') { in_tag = false; continue; }
            if (!in_tag)
            {
                if (c == '\n') c = ' ';
                plain += c;
            }
            if (plain.size() >= 160) break;
        }
        meta.description = plain;

        cache["/post/" + post.slug.get()] = make_cached(loom::render_layout(site, nav, loom::render_post(post, site.layout), sidebar_html, meta));
    }

    // Pre-render tag pages
    for (const auto& tag : all_tags)
    {
        auto tag_posts = engine.posts_by_tag(tag);
        loom::PageMeta meta;
        meta.title = "Posts tagged \"" + tag.get() + "\"";
        meta.canonical_path = "/tag/" + tag.get();
        meta.description = "All posts tagged with " + tag.get() + " on " + site.title;
        cache["/tag/" + tag.get()] = make_cached(loom::render_layout(site, nav, loom::render_tag_page(tag, tag_posts, site.layout), sidebar_html, meta));
    }
    {
        loom::PageMeta meta;
        meta.title = "Tags";
        meta.canonical_path = "/tags";
        meta.description = "Browse all tags on " + site.title;
        cache["/tags"] = make_cached(loom::render_layout(site, nav, loom::render_tag_index(all_tags), sidebar_html, meta));
    }

    // Pre-render all pages
    for (const auto& page : site.pages)
    {
        loom::PageMeta meta;
        meta.title = page.title.get();
        meta.canonical_path = "/" + page.slug.get();
        cache["/" + page.slug.get()] = make_cached(loom::render_layout(site, nav, loom::render_page(page), sidebar_html, meta));
    }

    // Pre-render 404 page (noindex)
    loom::PageMeta not_found_meta;
    not_found_meta.title = "404 — Not Found";
    not_found_meta.noindex = true;
    auto not_found_page = make_cached(loom::render_layout(site, nav, "<section><h2>404 — Not Found</h2><p>The page you're looking for doesn't exist.</p></section>", sidebar_html, not_found_meta));

    // Generate sitemap.xml
    std::string sitemap;
    {
        sitemap += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        sitemap += "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

        // Homepage
        sitemap += "  <url><loc>" + site.base_url + "/</loc><priority>1.0</priority></url>\n";

        // Posts
        auto posts = engine.list_posts();
        for (const auto& post : posts)
        {
            sitemap += "  <url><loc>" + site.base_url + "/post/" + xml_escape(post.slug.get()) + "</loc>";
            sitemap += "<lastmod>" + format_iso_date(post.published) + "</lastmod>";
            sitemap += "<priority>0.8</priority></url>\n";
        }

        // Pages
        for (const auto& page : site.pages)
            sitemap += "  <url><loc>" + site.base_url + "/" + xml_escape(page.slug.get()) + "</loc><priority>0.6</priority></url>\n";

        // Tag pages
        for (const auto& tag : all_tags)
            sitemap += "  <url><loc>" + site.base_url + "/tag/" + xml_escape(tag.get()) + "</loc><priority>0.4</priority></url>\n";

        sitemap += "  <url><loc>" + site.base_url + "/tags</loc><priority>0.3</priority></url>\n";
        sitemap += "</urlset>\n";
    }

    // Generate robots.txt
    std::string robots;
    {
        robots += "User-agent: *\n";
        robots += "Allow: /\n";
        robots += "Disallow:\n\n";
        if (!site.base_url.empty())
            robots += "Sitemap: " + site.base_url + "/sitemap.xml\n";
    }

    // Generate RSS feed
    std::string rss;
    {
        rss += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        rss += "<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n";
        rss += "<channel>\n";
        rss += "  <title>" + xml_escape(site.title) + "</title>\n";
        rss += "  <description>" + xml_escape(site.description) + "</description>\n";
        rss += "  <link>" + site.base_url + "/</link>\n";
        if (!site.base_url.empty())
            rss += "  <atom:link href=\"" + site.base_url + "/feed.xml\" rel=\"self\" type=\"application/rss+xml\"/>\n";
        rss += "  <language>en</language>\n";

        auto posts = engine.list_posts();
        int count = 0;
        for (const auto& post : posts)
        {
            if (++count > 20) break;
            rss += "  <item>\n";
            rss += "    <title>" + xml_escape(post.title.get()) + "</title>\n";
            rss += "    <link>" + site.base_url + "/post/" + xml_escape(post.slug.get()) + "</link>\n";
            rss += "    <guid>" + site.base_url + "/post/" + xml_escape(post.slug.get()) + "</guid>\n";
            rss += "    <pubDate>" + format_rfc822_date(post.published) + "</pubDate>\n";
            for (const auto& tag : post.tags)
                rss += "    <category>" + xml_escape(tag.get()) + "</category>\n";
            rss += "  </item>\n";
        }

        rss += "</channel>\n";
        rss += "</rss>\n";
    }

    auto sitemap_page = make_cached(sitemap);
    auto robots_page = make_cached(robots);
    auto rss_page = make_cached(rss);

    std::cout << "Pre-rendered " << cache.size() << " pages" << std::endl;

    loom::HttpServer server(8080);

    // Index
    server.router().get("/", [&](const loom::HttpRequest& req)
    {
        return serve_cached(cache.at("/"), req);
    });

    // Sitemap
    server.router().get("/sitemap.xml", [&](const loom::HttpRequest& req)
    {
        return serve_cached(sitemap_page, req, "application/xml; charset=utf-8");
    });

    // Robots.txt
    server.router().get("/robots.txt", [&](const loom::HttpRequest& req)
    {
        return serve_cached(robots_page, req, "text/plain; charset=utf-8");
    });

    // RSS feed
    server.router().get("/feed.xml", [&](const loom::HttpRequest& req)
    {
        return serve_cached(rss_page, req, "application/rss+xml; charset=utf-8");
    });

    // Single post
    server.router().get("/post/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/post/" + req.params[0]);
        if (it == cache.end())
            return serve_cached(not_found_page, req);

        return serve_cached(it->second, req);
    });

    // Tag index
    server.router().get("/tags", [&](const loom::HttpRequest& req)
    {
        return serve_cached(cache.at("/tags"), req);
    });

    // Tag page
    server.router().get("/tag/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/tag/" + req.params[0]);
        if (it == cache.end())
            return serve_cached(not_found_page, req);

        return serve_cached(it->second, req);
    });

    // Pages
    server.router().get("/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/" + req.params[0]);
        if (it == cache.end())
            return serve_cached(not_found_page, req);

        return serve_cached(it->second, req);
    });

    std::cout << "Serving " << site.title << " at http://localhost:8080" << std::endl;
    server.run();
}
