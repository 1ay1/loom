#include <iostream>
#include <string>

#include "loom/content/content_source.hpp"
#include "loom/content/filesystem_source.hpp"
#include "loom/engine/site_builder.hpp"
#include "loom/engine/blog_engine.hpp"
#include "loom/render/layout.hpp"
#include "loom/render/navigation.hpp"
#include "loom/render/post_renderer.hpp"
#include "loom/render/page_renderer.hpp"
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

    loom::HttpServer server(8080);

    // Index: list posts
    server.router().get("/", [&](const loom::HttpRequest&)
    {
        auto posts = engine.list_posts();
        auto content = loom::render_index(posts);
        auto nav = loom::render_navigation(site.navigation);

        return loom::HttpResponse::ok(loom::render_layout(site, nav, content));
    });

    // Single post
    server.router().get("/post/:slug", [&](const loom::HttpRequest& req)
    {
        auto post = engine.get_post(loom::Slug(req.params[0]));

        if (!post)
            return loom::HttpResponse::not_found(
                "<h1>Post <b>" + req.params[0] + "</b> not found.</h1>");

        auto content = loom::render_post(*post);
        auto nav = loom::render_navigation(site.navigation);

        return loom::HttpResponse::ok(loom::render_layout(site, nav, content));
    });

    // Pages (e.g. /about, /projects)
    server.router().get("/:slug", [&](const loom::HttpRequest& req)
    {
        auto page = engine.get_page(loom::Slug(req.params[0]));

        if (!page)
            return loom::HttpResponse::not_found(
                "<h1>Page <b>" + req.params[0] + "</b> not found.</h1>");

        auto content = loom::render_page(*page);
        auto nav = loom::render_navigation(site.navigation);

        return loom::HttpResponse::ok(loom::render_layout(site, nav, content));
    });

    std::cout << "Serving " << site.title << " at http://localhost:8080" << std::endl;
    server.run();
}
