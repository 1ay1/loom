#include <iostream>
#include <string>
#include <unordered_map>

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

    std::unordered_map<std::string, std::string> cache;

    // Pre-render index
    cache["/"] = loom::render_layout(site, nav, loom::render_index(engine.list_posts(), site.layout), sidebar_html);

    // Pre-render all posts
    for (const auto& post : site.posts)
        cache["/post/" + post.slug.get()] = loom::render_layout(site, nav, loom::render_post(post, site.layout), sidebar_html);

    // Pre-render tag pages
    for (const auto& tag : all_tags)
    {
        auto tag_posts = engine.posts_by_tag(tag);
        cache["/tag/" + tag.get()] = loom::render_layout(site, nav, loom::render_tag_page(tag, tag_posts, site.layout), sidebar_html);
    }
    cache["/tags"] = loom::render_layout(site, nav, loom::render_tag_index(all_tags), sidebar_html);

    // Pre-render all pages
    for (const auto& page : site.pages)
        cache["/" + page.slug.get()] = loom::render_layout(site, nav, loom::render_page(page), sidebar_html);

    // Pre-render 404 page
    auto not_found_html = loom::render_layout(site, nav, "<section><h2>404 — Not Found</h2><p>The page you're looking for doesn't exist.</p></section>", sidebar_html);

    std::cout << "Pre-rendered " << cache.size() << " pages" << std::endl;

    loom::HttpServer server(8080);

    // Index
    server.router().get("/", [&](const loom::HttpRequest&)
    {
        return loom::HttpResponse::ok(cache.at("/"));
    });

    // Single post
    server.router().get("/post/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/post/" + req.params[0]);
        if (it == cache.end())
            return loom::HttpResponse::not_found(not_found_html);

        return loom::HttpResponse::ok(it->second);
    });

    // Tag index
    server.router().get("/tags", [&](const loom::HttpRequest&)
    {
        return loom::HttpResponse::ok(cache.at("/tags"));
    });

    // Tag page
    server.router().get("/tag/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/tag/" + req.params[0]);
        if (it == cache.end())
            return loom::HttpResponse::not_found(not_found_html);

        return loom::HttpResponse::ok(it->second);
    });

    // Pages
    server.router().get("/:slug", [&](const loom::HttpRequest& req)
    {
        auto it = cache.find("/" + req.params[0]);
        if (it == cache.end())
            return loom::HttpResponse::not_found(not_found_html);

        return loom::HttpResponse::ok(it->second);
    });

    std::cout << "Serving " << site.title << " at http://localhost:8080" << std::endl;
    server.run();
}
