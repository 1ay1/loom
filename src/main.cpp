#include <iostream>
#include <string>
#include <unordered_map>
#include <map>
#include <ctime>
#include <sstream>
#include "loom/util/gzip.hpp"
#include "loom/util/minify.hpp"

#include "loom/content/content_source.hpp"
#include "loom/content/filesystem_source.hpp"
#include "loom/content/git_source.hpp"
#include "loom/engine/site_builder.hpp"
#include "loom/engine/blog_engine.hpp"
#include "loom/render/layout.hpp"
#include "loom/render/navigation.hpp"
#include "loom/render/post_renderer.hpp"
#include "loom/render/page_renderer.hpp"
#include "loom/render/sidebar.hpp"
#include "loom/http/server.hpp"
#include "loom/http/response.hpp"

#include "loom/reload/hot_reloader.hpp"
#include "loom/reload/inotify_watcher.hpp"
#include "loom/reload/git_watcher.hpp"

struct CachedPage
{
    std::string raw;
    std::string gzipped;
    std::string etag;
};

// The entire pre-rendered site lives in this struct.
// Immutable once built — swapped atomically by the hot reloader.
struct SiteCache
{
    std::unordered_map<std::string, CachedPage> pages;
    CachedPage not_found;
    CachedPage sitemap;
    CachedPage robots;
    CachedPage rss;
};

static CachedPage make_cached(std::string html, bool minify = true)
{
    if (minify)
        html = loom::minify_html(html);
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

// Build the entire SiteCache from a source. Pure function — no side effects.
template<loom::ContentSource Source>
static std::shared_ptr<const SiteCache> build_cache(Source& source)
{
    auto config = source.site_config();
    auto site = loom::build_site(source, std::move(config));
    loom::BlogEngine engine(site);

    auto nav = loom::render_navigation(site.navigation);
    auto all_tags = engine.all_tags();
    auto all_series = engine.all_series();
    loom::SidebarData sidebar_data{engine.list_posts(), all_tags, all_series};
    auto sidebar_html = loom::render_sidebar(site, sidebar_data);

    auto cache = std::make_shared<SiteCache>();

    // Index
    {
        loom::PageMeta meta;
        meta.canonical_path = "/";
        cache->pages["/"] = make_cached(loom::render_layout(site, nav, loom::render_index(engine.list_posts(), site.layout), sidebar_html, meta));
    }

    // Posts
    for (const auto& post : site.posts)
    {
        if (post.draft || post.published > std::chrono::system_clock::now())
            continue;

        loom::PageMeta meta;
        meta.title = post.title.get();
        meta.canonical_path = "/post/" + post.slug.get();
        meta.og_type = "article";
        meta.published_date = format_iso_date(post.published);
        meta.author = site.author;
        for (const auto& t : post.tags)
            meta.tags.push_back(t.get());

        if (!post.excerpt.empty())
        {
            meta.description = post.excerpt.substr(0, 160);
        }
        else
        {
            std::string plain;
            bool in_html_tag = false;
            for (char c : post.content.get())
            {
                if (c == '<') { in_html_tag = true; continue; }
                if (c == '>') { in_html_tag = false; continue; }
                if (!in_html_tag)
                {
                    if (c == '\n') c = ' ';
                    plain += c;
                }
                if (plain.size() >= 160) break;
            }
            meta.description = plain;
        }

        loom::PostContext ctx;
        auto [prev, next] = engine.prev_next(post);
        ctx.nav = {prev, next};
        ctx.related = engine.related_posts(post, 3);
        if (!post.series.empty())
        {
            ctx.series_name = post.series;
            ctx.series_posts = engine.posts_in_series(post.series);
        }

        cache->pages["/post/" + post.slug.get()] = make_cached(loom::render_layout(site, nav, loom::render_post(post, site.layout, ctx), sidebar_html, meta));
    }

    // Tag pages
    for (const auto& tag : all_tags)
    {
        auto tag_posts = engine.posts_by_tag(tag);
        loom::PageMeta meta;
        meta.title = "Posts tagged \"" + tag.get() + "\"";
        meta.canonical_path = "/tag/" + tag.get();
        meta.description = "All posts tagged with " + tag.get() + " on " + site.title;
        cache->pages["/tag/" + tag.get()] = make_cached(loom::render_layout(site, nav, loom::render_tag_page(tag, tag_posts, site.layout), sidebar_html, meta));
    }
    {
        loom::PageMeta meta;
        meta.title = "Tags";
        meta.canonical_path = "/tags";
        meta.description = "Browse all tags on " + site.title;
        cache->pages["/tags"] = make_cached(loom::render_layout(site, nav, loom::render_tag_index(all_tags), sidebar_html, meta));
    }

    // Archives
    {
        auto by_year = engine.posts_by_year();
        loom::PageMeta meta;
        meta.title = "Archives";
        meta.canonical_path = "/archives";
        meta.description = "All posts on " + site.title;
        cache->pages["/archives"] = make_cached(loom::render_layout(site, nav, loom::render_archives(by_year, site.layout), sidebar_html, meta));
    }

    // Series
    for (const auto& series : all_series)
    {
        auto series_posts = engine.posts_in_series(series);
        loom::PageMeta meta;
        meta.title = "Series: " + series.get();
        meta.canonical_path = "/series/" + series.get();
        meta.description = "Posts in the " + series.get() + " series on " + site.title;
        cache->pages["/series/" + series.get()] = make_cached(loom::render_layout(site, nav, loom::render_series_page(series, series_posts, site.layout), sidebar_html, meta));
    }
    if (!all_series.empty())
    {
        loom::PageMeta meta;
        meta.title = "Series";
        meta.canonical_path = "/series";
        meta.description = "Browse all series on " + site.title;
        cache->pages["/series"] = make_cached(loom::render_layout(site, nav, loom::render_series_index(all_series), sidebar_html, meta));
    }

    // Static pages
    for (const auto& page : site.pages)
    {
        loom::PageMeta meta;
        meta.title = page.title.get();
        meta.canonical_path = "/" + page.slug.get();
        cache->pages["/" + page.slug.get()] = make_cached(loom::render_layout(site, nav, loom::render_page(page), sidebar_html, meta));
    }

    // 404
    {
        loom::PageMeta meta;
        meta.title = "404 — Not Found";
        meta.noindex = true;
        cache->not_found = make_cached(loom::render_layout(site, nav, "<section><h2>404 — Not Found</h2><p>The page you're looking for doesn't exist.</p></section>", sidebar_html, meta));
    }

    // Sitemap
    {
        std::string sitemap;
        sitemap += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        sitemap += "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
        sitemap += "  <url><loc>" + site.base_url + "/</loc><priority>1.0</priority></url>\n";

        auto posts = engine.list_posts();
        for (const auto& post : posts)
        {
            sitemap += "  <url><loc>" + site.base_url + "/post/" + xml_escape(post.slug.get()) + "</loc>";
            sitemap += "<lastmod>" + format_iso_date(post.published) + "</lastmod>";
            sitemap += "<priority>0.8</priority></url>\n";
        }
        for (const auto& page : site.pages)
            sitemap += "  <url><loc>" + site.base_url + "/" + xml_escape(page.slug.get()) + "</loc><priority>0.6</priority></url>\n";
        for (const auto& tag : all_tags)
            sitemap += "  <url><loc>" + site.base_url + "/tag/" + xml_escape(tag.get()) + "</loc><priority>0.4</priority></url>\n";
        sitemap += "  <url><loc>" + site.base_url + "/tags</loc><priority>0.3</priority></url>\n";
        sitemap += "  <url><loc>" + site.base_url + "/archives</loc><priority>0.5</priority></url>\n";
        for (const auto& s : all_series)
            sitemap += "  <url><loc>" + site.base_url + "/series/" + xml_escape(s.get()) + "</loc><priority>0.5</priority></url>\n";
        if (!all_series.empty())
            sitemap += "  <url><loc>" + site.base_url + "/series</loc><priority>0.4</priority></url>\n";
        sitemap += "</urlset>\n";

        cache->sitemap = make_cached(sitemap, false);
    }

    // Robots
    {
        std::string robots;
        robots += "User-agent: *\n";
        robots += "Allow: /\n";
        robots += "Disallow:\n\n";
        if (!site.base_url.empty())
            robots += "Sitemap: " + site.base_url + "/sitemap.xml\n";
        cache->robots = make_cached(robots, false);
    }

    // RSS
    {
        std::string rss;
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
        cache->rss = make_cached(rss, false);
    }

    return cache;
}

static void setup_routes(loom::HttpServer& server, loom::AtomicCache<SiteCache>& cache)
{
    server.router().get("/", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->pages.at("/"), req);
    });

    server.router().get("/sitemap.xml", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->sitemap, req, "application/xml; charset=utf-8");
    });

    server.router().get("/robots.txt", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->robots, req, "text/plain; charset=utf-8");
    });

    server.router().get("/feed.xml", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->rss, req, "application/rss+xml; charset=utf-8");
    });

    server.router().get("/post/:slug", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        auto it = snap->pages.find("/post/" + req.params[0]);
        if (it == snap->pages.end())
            return serve_cached(snap->not_found, req);
        return serve_cached(it->second, req);
    });

    server.router().get("/tags", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->pages.at("/tags"), req);
    });

    server.router().get("/tag/:slug", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        auto it = snap->pages.find("/tag/" + req.params[0]);
        if (it == snap->pages.end())
            return serve_cached(snap->not_found, req);
        return serve_cached(it->second, req);
    });

    server.router().get("/archives", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        return serve_cached(snap->pages.at("/archives"), req);
    });

    server.router().get("/series", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        auto it = snap->pages.find("/series");
        if (it == snap->pages.end())
            return serve_cached(snap->not_found, req);
        return serve_cached(it->second, req);
    });

    server.router().get("/series/:slug", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        auto it = snap->pages.find("/series/" + req.params[0]);
        if (it == snap->pages.end())
            return serve_cached(snap->not_found, req);
        return serve_cached(it->second, req);
    });

    server.router().get("/:slug", [&](const loom::HttpRequest& req)
    {
        auto snap = cache.load();
        auto it = snap->pages.find("/" + req.params[0]);
        if (it == snap->pages.end())
            return serve_cached(snap->not_found, req);
        return serve_cached(it->second, req);
    });
}

static void run_with_filesystem(const std::string& content_dir)
{
    std::cout << "Loading content from: " << content_dir << std::endl;

    loom::FileSystemSource source(content_dir);

    auto initial = build_cache(source);
    std::cout << "Pre-rendered " << initial->pages.size() << " pages" << std::endl;

    loom::AtomicCache<SiteCache> cache(std::move(initial));

    loom::InotifyWatcher watcher(content_dir);

    loom::HotReloader<loom::FileSystemSource, loom::InotifyWatcher, SiteCache> reloader(
        source, watcher, cache,
        [](loom::FileSystemSource& src, const loom::ChangeSet&) {
            return build_cache(src);
        }
    );
    reloader.start();
    std::cout << "[hot-reload] Watching " << content_dir << " for changes" << std::endl;

    loom::HttpServer server(8080);
    setup_routes(server, cache);

    std::cout << "Serving at http://localhost:8080" << std::endl;
    server.run();
}

static void run_with_git(const std::string& repo_path, const std::string& branch,
                         const std::string& content_prefix, bool fetch_remote)
{
    std::cout << "Loading content from git: " << repo_path
              << " (" << branch << ")" << std::endl;
    if (!content_prefix.empty())
        std::cout << "Content prefix: " << content_prefix << std::endl;

    loom::GitSource source({repo_path, branch, content_prefix});

    auto initial = build_cache(source);
    std::cout << "Pre-rendered " << initial->pages.size() << " pages" << std::endl;

    loom::AtomicCache<SiteCache> cache(std::move(initial));

    loom::GitWatcher watcher(repo_path, branch, fetch_remote);

    loom::HotReloader<loom::GitSource, loom::GitWatcher, SiteCache> reloader(
        source, watcher, cache,
        [](loom::GitSource& src, const loom::ChangeSet&) {
            return build_cache(src);
        }
    );
    reloader.start();

    if (fetch_remote)
        std::cout << "[hot-reload] Fetching " << branch << " from remote" << std::endl;
    else
        std::cout << "[hot-reload] Polling " << branch << " for new commits" << std::endl;

    loom::HttpServer server(8080);
    setup_routes(server, cache);

    std::cout << "Serving at http://localhost:8080" << std::endl;
    server.run();
}

int main(int argc, char* argv[])
{
    // Usage:
    //   ./loom [content_dir]                                 — filesystem source (default)
    //   ./loom --git <repo_or_url> [branch] [content_prefix] — git source (local path or remote URL)
    if (argc > 1 && std::string(argv[1]) == "--git")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: loom --git <repo_path_or_url> [branch] [content_prefix]" << std::endl;
            return 1;
        }

        std::string repo_arg = argv[2];
        std::string branch = (argc > 3) ? argv[3] : "main";
        std::string prefix = (argc > 4) ? argv[4] : "";

        bool remote = loom::is_remote_url(repo_arg);
        std::string repo_path = repo_arg;

        if (remote)
        {
            std::cout << "Cloning " << repo_arg << "..." << std::endl;
            try
            {
                repo_path = loom::git_clone_bare(repo_arg, "/tmp/loom-repos");
                std::cout << "Cloned to " << repo_path << std::endl;
            }
            catch (const loom::GitError& e)
            {
                std::cerr << "Failed to clone: " << e.what() << std::endl;
                return 1;
            }
        }

        run_with_git(repo_path, branch, prefix, remote);
    }
    else
    {
        std::string content_dir = "content";
        if (argc > 1)
            content_dir = argv[1];

        run_with_filesystem(content_dir);
    }
}
