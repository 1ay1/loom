#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <map>
#include <ctime>
#include <sstream>
#include <fstream>
#include <set>
#include "loom/util/gzip.hpp"
#include "loom/util/minify.hpp"
#include "loom/util/git.hpp"

#include "loom/content/content_source.hpp"
#include "loom/content/filesystem_source.hpp"
#include "loom/content/git_source.hpp"
#include "loom/engine/site_builder.hpp"
#include "loom/engine/blog_engine.hpp"
#include "loom/render/component.hpp"
#include "loom/http/server.hpp"
#include "loom/http/response.hpp"
#include "loom/http/route.hpp"

#include "loom/reload/hot_reloader.hpp"
#include "loom/reload/inotify_watcher.hpp"
#include "loom/reload/git_watcher.hpp"

// ── Compile-time MIME type table ─────────────────────────────────
// constexpr lookup — the compiler folds this into an optimal branch
// chain at -O2. No hash map, no heap, no runtime table construction.

static constexpr std::string_view mime_type(std::string_view path) noexcept
{
    auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return "application/octet-stream";
    auto ext = path.substr(dot);

    // Ordered by frequency for early-out
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg")  return "image/jpeg";
    if (ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".woff") return "font/woff";
    if (ext == ".ttf")  return "font/ttf";
    if (ext == ".pdf")  return "application/pdf";
    if (ext == ".mp4")  return "video/mp4";
    if (ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

// ── Pre-serialized HTTP response builder ─────────────────────────

static std::string build_http_response(
    std::string_view status_line,
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers,
    std::string_view body,
    bool keep_alive)
{
    std::string out;
    out.reserve(256 + body.size());
    out.append(status_line.data(), status_line.size());

    for (const auto& [k, v] : headers)
    {
        out.append(k.data(), k.size());
        out += ": ";
        out.append(v.data(), v.size());
        out += "\r\n";
    }

    out += "Content-Length: ";
    out += std::to_string(body.size());
    out += "\r\nConnection: ";
    out += keep_alive ? "keep-alive" : "close";
    out += "\r\n\r\n";
    out.append(body.data(), body.size());
    return out;
}

// ── CachedPage ───────────────────────────────────────────────────
// 6 pre-serialized wire variants per page. On the hot path the server
// writes directly from these buffers — zero copies, zero allocations.

struct CachedPage
{
    std::string etag;
    std::string gzip_ka;
    std::string gzip_close;
    std::string plain_ka;
    std::string plain_close;
    std::string not_modified_ka;
    std::string not_modified_close;
};

// Transparent hash for string_view lookups in string-keyed maps
struct PageHash
{
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept
    { return std::hash<std::string_view>{}(sv); }
};

struct SiteCache
{
    std::unordered_map<std::string, CachedPage, PageHash, std::equal_to<>> pages;
    CachedPage not_found;
    CachedPage sitemap;
    CachedPage robots;
    CachedPage rss;
    CachedPage search_json;
};

static constexpr std::string_view status_line_for(int status) noexcept
{
    switch (status)
    {
        case 200: return "HTTP/1.1 200 OK\r\n";
        case 404: return "HTTP/1.1 404 Not Found\r\n";
        default:  return "HTTP/1.1 200 OK\r\n";
    }
}

static CachedPage make_cached(std::string html,
    std::string_view content_type = "text/html; charset=utf-8",
    bool do_minify = true,
    int status = 200)
{
    if (do_minify)
        html = loom::minify_html(html);

    auto hash = std::hash<std::string>{}(html);
    std::string etag = "\"" + std::to_string(hash) + "\"";
    auto gz = loom::gzip_compress(html);

    auto sl = status_line_for(status);
    CachedPage page;
    page.etag = etag;

    page.gzip_ka = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"},
         {"Content-Encoding", "gzip"}},
        gz, true);
    page.gzip_close = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"},
         {"Content-Encoding", "gzip"}},
        gz, false);
    page.plain_ka = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"}},
        html, true);
    page.plain_close = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"}},
        html, false);
    page.not_modified_ka = build_http_response("HTTP/1.1 304 Not Modified\r\n",
        {{"ETag", etag}, {"Cache-Control", "public, max-age=60, must-revalidate"}},
        "", true);
    page.not_modified_close = build_http_response("HTTP/1.1 304 Not Modified\r\n",
        {{"ETag", etag}, {"Cache-Control", "public, max-age=60, must-revalidate"}},
        "", false);

    return page;
}

static bool accepts_gzip(const loom::HttpRequest& req)
{
    return req.header("accept-encoding").find("gzip") != std::string_view::npos;
}

// Zero-copy serve: returns a prebuilt HttpResponse pointing directly
// into the SiteCache wire buffers.
static loom::HttpResponse serve_cached(
    const CachedPage& page,
    const loom::HttpRequest& req,
    const std::shared_ptr<const SiteCache>& snap)
{
    bool ka = req.keep_alive();

    auto serve = [&](const std::string& wire) -> loom::HttpResponse {
        return loom::PrebuiltResponse{snap, wire.data(), wire.size()};
    };

    auto inm = req.header("if-none-match");
    if (!inm.empty() && inm == page.etag)
        return serve(ka ? page.not_modified_ka : page.not_modified_close);

    if (accepts_gzip(req))
        return serve(ka ? page.gzip_ka : page.gzip_close);

    return serve(ka ? page.plain_ka : page.plain_close);
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

static std::string json_escape_str(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
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

// ── build_cache ──────────────────────────────────────────────────

template<loom::ContentSource Source>
static std::shared_ptr<const SiteCache> build_cache(Source& source)
{
    namespace c = loom::component;

    auto config = source.site_config();
    auto site = loom::build_site(source, std::move(config));
    loom::BlogEngine engine(site);

    auto ctx = c::Ctx::from(site);

    auto all_tags = engine.all_tags();
    auto all_tag_infos = engine.tag_info();
    auto all_series = engine.all_series();
    auto all_series_infos = engine.series_info();
    c::SidebarData sidebar_data{engine.list_posts(), all_tags, all_tag_infos, all_series, all_series_infos};

    auto render_page = [&](loom::PageMeta meta, loom::dom::Node content) -> std::string {
        return ctx.page(meta, std::move(content), &sidebar_data).render();
    };

    auto cache = std::make_shared<SiteCache>();

    // Paginated index with featured posts
    int total_pages = 1;
    {
        auto all_posts = engine.list_posts();
        auto featured = engine.featured_posts();

        // Build set of featured slugs for dedup
        std::set<std::string> featured_slugs;
        for (const auto& fp : featured)
            featured_slugs.insert(fp.slug.get());

        // Remove featured from regular listing
        std::vector<loom::PostSummary> regular;
        regular.reserve(all_posts.size());
        for (auto& p : all_posts)
            if (!featured_slugs.count(p.slug.get()))
                regular.push_back(std::move(p));

        int per_page = site.layout.posts_per_page;
        int total = static_cast<int>(regular.size());
        total_pages = std::max(1, (total + per_page - 1) / per_page);

        for (int page = 1; page <= total_pages; ++page)
        {
            int start = (page - 1) * per_page;
            int end = std::min(start + per_page, total);
            std::vector<loom::PostSummary> page_posts(regular.begin() + start, regular.begin() + end);

            c::PaginationInfo pinfo{page, total_pages};

            loom::PageMeta meta;
            meta.canonical_path = (page == 1) ? "/" : "/page/" + std::to_string(page);
            if (page > 1)
                meta.title = "Page " + std::to_string(page);

            auto* feat_ptr = (page == 1 && !featured.empty()) ? &featured : nullptr;

            cache->pages[meta.canonical_path] = make_cached(
                render_page(meta, ctx(c::Index{
                    .posts = &page_posts,
                    .featured = feat_ptr,
                    .pagination = &pinfo
                })));
        }
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

        meta.og_image = post.image;

        if (!post.excerpt.empty())
        {
            meta.description = post.excerpt.substr(0, 160);
        }
        else
        {
            std::string plain;
            bool in_html_tag = false;
            for (char ch : post.content.get())
            {
                if (ch == '<') { in_html_tag = true; continue; }
                if (ch == '>') { in_html_tag = false; continue; }
                if (!in_html_tag)
                {
                    if (ch == '\n') ch = ' ';
                    plain += ch;
                }
                if (plain.size() >= 160) break;
            }
            meta.description = plain;
        }

        c::PostContext pctx;
        auto [prev, next] = engine.prev_next(post);
        pctx.nav = {prev, next};
        pctx.related = engine.related_posts(post, 3);
        if (!post.series.empty())
        {
            pctx.series_name = post.series;
            pctx.series_posts = engine.posts_in_series(post.series);
        }

        cache->pages["/post/" + post.slug.get()] = make_cached(
            render_page(meta, ctx(c::PostFull{.post = &post, .context = &pctx})));
    }

    // Tag pages
    for (const auto& tag : all_tags)
    {
        auto tag_posts = engine.posts_by_tag(tag);
        loom::PageMeta meta;
        meta.title = "Posts tagged \"" + tag.get() + "\"";
        meta.canonical_path = "/tag/" + tag.get();
        meta.description = "All posts tagged with " + tag.get() + " on " + site.title;
        cache->pages["/tag/" + tag.get()] = make_cached(
            render_page(meta, ctx(c::TagPage{.tag = tag, .posts = &tag_posts,
                                              .post_count = static_cast<int>(tag_posts.size())})));
    }
    {
        loom::PageMeta meta;
        meta.title = "Tags";
        meta.canonical_path = "/tags";
        meta.description = "Browse all tags on " + site.title;
        cache->pages["/tags"] = make_cached(
            render_page(meta, ctx(c::TagIndex{.tag_infos = &all_tag_infos})));
    }

    // Archives
    {
        auto by_year = engine.posts_by_year();
        loom::PageMeta meta;
        meta.title = "Archives";
        meta.canonical_path = "/archives";
        meta.description = "All posts on " + site.title;
        cache->pages["/archives"] = make_cached(
            render_page(meta, ctx(c::Archives{.by_year = &by_year})));
    }

    // Series
    for (const auto& series : all_series)
    {
        auto series_posts = engine.posts_in_series(series);
        loom::PageMeta meta;
        meta.title = "Series: " + series.get();
        meta.canonical_path = "/series/" + series.get();
        meta.description = "Posts in the " + series.get() + " series on " + site.title;
        cache->pages["/series/" + series.get()] = make_cached(
            render_page(meta, ctx(c::SeriesPage{.series = series, .posts = &series_posts})));
    }
    if (!all_series.empty())
    {
        loom::PageMeta meta;
        meta.title = "Series";
        meta.canonical_path = "/series";
        meta.description = "Browse all series on " + site.title;
        cache->pages["/series"] = make_cached(
            render_page(meta, ctx(c::SeriesIndex{.all_series_info = &all_series_infos})));
    }

    // Static pages
    for (const auto& page : site.pages)
    {
        loom::PageMeta meta;
        meta.title = page.title.get();
        meta.canonical_path = "/" + page.slug.get();
        meta.og_image = page.image;
        cache->pages["/" + page.slug.get()] = make_cached(
            render_page(meta, ctx(c::PageView{.page = &page})));
    }

    // 404
    {
        loom::PageMeta meta;
        meta.title = "404 \xe2\x80\x94 Not Found";
        meta.noindex = true;
        cache->not_found = make_cached(
            render_page(meta, ctx(c::NotFound{})),
            "text/html; charset=utf-8", true, 404);
    }

    // Search page
    {
        loom::PageMeta meta;
        meta.title = "Search";
        meta.canonical_path = "/search";
        cache->pages["/search"] = make_cached(
            render_page(meta, ctx(c::SearchPage{})));
    }

    // Search JSON index
    {
        std::string json = "[";
        auto posts = engine.list_posts();
        for (size_t i = 0; i < posts.size(); ++i)
        {
            const auto& p = posts[i];
            if (i > 0) json += ',';
            json += "{\"s\":\"" + json_escape_str(p.slug.get())
                 + "\",\"t\":\"" + json_escape_str(p.title.get())
                 + "\",\"e\":\"" + json_escape_str(p.excerpt.substr(0, 200)) + "\",\"g\":[";
            for (size_t j = 0; j < p.tags.size(); ++j)
            {
                if (j > 0) json += ',';
                json += "\"" + json_escape_str(p.tags[j].get()) + "\"";
            }
            json += "]}";
        }
        json += "]";
        cache->search_json = make_cached(json, "application/json; charset=utf-8", false);
    }

    // Sitemap
    {
        std::string sitemap;
        sitemap += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        sitemap += "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
        sitemap += "  <url><loc>" + site.base_url + "/</loc><priority>1.0</priority></url>\n";
        for (int page = 2; page <= total_pages; ++page)
            sitemap += "  <url><loc>" + site.base_url + "/page/" + std::to_string(page) + "</loc><priority>0.5</priority></url>\n";
        sitemap += "  <url><loc>" + site.base_url + "/search</loc><priority>0.3</priority></url>\n";

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

        cache->sitemap = make_cached(sitemap, "application/xml; charset=utf-8", false);
    }

    // Robots
    {
        std::string robots;
        robots += "User-agent: *\n";
        robots += "Allow: /\n";
        robots += "Disallow:\n\n";
        if (!site.base_url.empty())
            robots += "Sitemap: " + site.base_url + "/sitemap.xml\n";
        cache->robots = make_cached(robots, "text/plain; charset=utf-8", false);
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
        cache->rss = make_cached(rss, "application/rss+xml; charset=utf-8", false);
    }

    return cache;
}

// ── Helpers ──────────────────────────────────────────────────────

static constexpr bool safe_path(std::string_view path) noexcept
{
    if (path.find("..") != std::string_view::npos) return false;
    if (path.find('\0') != std::string_view::npos) return false;
    if (path.empty() || path[0] != '/') return false;
    return true;
}

static std::string raw_content_base(const std::string& remote_url, const std::string& branch)
{
    std::string owner_repo;
    auto gh = remote_url.find("github.com/");
    if (gh != std::string::npos)
        owner_repo = remote_url.substr(gh + 11);
    else
    {
        gh = remote_url.find("github.com:");
        if (gh != std::string::npos)
            owner_repo = remote_url.substr(gh + 11);
    }

    if (!owner_repo.empty())
    {
        if (owner_repo.ends_with(".git"))
            owner_repo = owner_repo.substr(0, owner_repo.size() - 4);
        return "https://raw.githubusercontent.com/" + owner_repo + "/refs/heads/" + branch;
    }

    return "";
}

// ── Compile-time route table ─────────────────────────────────────
//
// Every route pattern is a non-type template parameter. The compiler
// sees the full table as a single constexpr expression and generates
// an optimal dispatch chain — no trie, no hash map, no vtable.

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

    // ── Handlers ──

    // Cached HTML pages — all use the same logic: look up req.path in the map
    auto cached = [&cache](const loom::HttpRequest& req) {
        auto snap = cache.load();
        auto it = snap->pages.find(req.path);
        if (it != snap->pages.end())
            return serve_cached(it->second, req, snap);
        return serve_cached(snap->not_found, req, snap);
    };

    // Fixed cache fields
    auto sitemap = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->sitemap, req, s);
    };
    auto robots = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->robots, req, s);
    };
    auto rss = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->rss, req, s);
    };
    auto search_json = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->search_json, req, s);
    };

    // Static file fallback
    auto fb = [&cache, &content_dir](loom::HttpRequest& req) -> loom::HttpResponse {
        if (!safe_path(req.path))
            return loom::DynamicResponse::not_found();

        auto snap = cache.load();

        auto it = snap->pages.find(req.path);
        if (it != snap->pages.end())
            return serve_cached(it->second, req, snap);

        std::string file_path = content_dir;
        file_path += req.path;
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open())
            return serve_cached(snap->not_found, req, snap);

        std::string body((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

        loom::DynamicResponse resp;
        resp.status = 200;
        resp.headers = {{"Content-Type", std::string(mime_type(req.path))},
                        {"Cache-Control", "public, max-age=3600"}};
        resp.body = std::move(body);
        return resp;
    };

    // ── Compile the route table ──
    using namespace loom::route;

    auto dispatch = compile(
        fallback(fb),
        // Static routes (exact match, checked first)
        get<"/">(cached),
        get<"/tags">(cached),
        get<"/archives">(cached),
        get<"/series">(cached),
        get<"/search">(cached),
        get<"/sitemap.xml">(sitemap),
        get<"/robots.txt">(robots),
        get<"/feed.xml">(rss),
        get<"/search.json">(search_json),
        // Parameterized routes (prefix match)
        get<"/post/:slug">(cached),
        get<"/tag/:slug">(cached),
        get<"/series/:slug">(cached),
        get<"/page/:num">(cached)
    );

    // Server socket typestate: Unbound → Bound → Listening
    // Each transition consumes the source and produces the next state.
    auto socket = loom::create_server_socket()
        .bind(8080)
        .listen();

    loom::HttpServer server(std::move(socket));
    server.set_dispatch(dispatch);

    std::cout << "Serving at http://localhost:8080" << std::endl;
    server.run();
}

static void run_with_git(const std::string& repo_path, const std::string& branch,
                         const std::string& content_prefix, bool fetch_remote,
                         const std::string& remote_url = "")
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

    std::string raw_base = raw_content_base(remote_url, branch);
    if (!raw_base.empty())
        std::cout << "[static] Redirecting assets to " << raw_base << std::endl;

    // ── Handlers ──

    auto cached = [&cache](const loom::HttpRequest& req) {
        auto snap = cache.load();
        auto it = snap->pages.find(req.path);
        if (it != snap->pages.end())
            return serve_cached(it->second, req, snap);
        return serve_cached(snap->not_found, req, snap);
    };

    auto sitemap_h = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->sitemap, req, s);
    };
    auto robots_h = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->robots, req, s);
    };
    auto rss_h = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->rss, req, s);
    };
    auto search_json_h = [&cache](const loom::HttpRequest& req) {
        auto s = cache.load(); return serve_cached(s->search_json, req, s);
    };

    auto fb = [&cache, repo_path, branch, content_prefix, raw_base](loom::HttpRequest& req) -> loom::HttpResponse {
        if (!safe_path(req.path))
            return loom::DynamicResponse::not_found();

        auto snap = cache.load();

        auto it = snap->pages.find(req.path);
        if (it != snap->pages.end())
            return serve_cached(it->second, req, snap);

        std::string asset_path;
        if (content_prefix.empty())
            asset_path = std::string(req.path.substr(1));
        else
        {
            asset_path = content_prefix;
            asset_path += req.path;
        }

        if (!raw_base.empty())
        {
            return loom::DynamicResponse::redirect(302, raw_base + "/" + asset_path);
        }

        try
        {
            auto body = loom::git_read_blob(repo_path, branch, asset_path);

            loom::DynamicResponse resp;
            resp.status = 200;
            resp.headers = {{"Content-Type", std::string(mime_type(req.path))},
                            {"Cache-Control", "public, max-age=3600"}};
            resp.body = std::move(body);
            return resp;
        }
        catch (const loom::GitError&)
        {
            return serve_cached(snap->not_found, req, snap);
        }
    };

    using namespace loom::route;

    auto dispatch = compile(
        fallback(fb),
        get<"/">(cached),
        get<"/tags">(cached),
        get<"/archives">(cached),
        get<"/series">(cached),
        get<"/search">(cached),
        get<"/sitemap.xml">(sitemap_h),
        get<"/robots.txt">(robots_h),
        get<"/feed.xml">(rss_h),
        get<"/search.json">(search_json_h),
        get<"/post/:slug">(cached),
        get<"/tag/:slug">(cached),
        get<"/series/:slug">(cached),
        get<"/page/:num">(cached)
    );

    // Server socket typestate: Unbound → Bound → Listening
    auto socket = loom::create_server_socket()
        .bind(8080)
        .listen();

    loom::HttpServer server(std::move(socket));
    server.set_dispatch(dispatch);

    std::cout << "Serving at http://localhost:8080" << std::endl;
    server.run();
}

int main(int argc, char* argv[])
{
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

        try { run_with_git(repo_path, branch, prefix, remote, remote ? repo_arg : ""); }
        catch (const std::exception& e) { std::cerr << "Fatal: " << e.what() << std::endl; return 1; }
    }
    else
    {
        std::string content_dir = "content";
        if (argc > 1)
            content_dir = argv[1];

        try { run_with_filesystem(content_dir); }
        catch (const std::exception& e) { std::cerr << "Fatal: " << e.what() << std::endl; return 1; }
    }
}
