#include <iostream>
#include <string>

#include "loom/domain/post.hpp"
#include "loom/content/content_source.hpp"
#include "loom/content/memory_source.hpp"
#include "loom/engine/site_builder.hpp"
#include "loom/engine/blog_engine.hpp"
#include "loom/render/layout.hpp"
#include "loom/render/navigation.hpp"
#include "loom/render/post_renderer.hpp"
#include "loom/http/server.hpp"

int main()
{
    static_assert(loom::ContentSource<loom::MemorySource>);

    loom::MemorySource source;

    source.add_post({
        loom::PostId("1"),
        loom::Title("Hello World"),
        loom::Slug("hello"),
        loom::Content("My first blog post"),
        {},
        std::chrono::system_clock::now(),
    });

    source.add_post({
        loom::PostId("2"),
        loom::Title("Oksana Hello World2"),
        loom::Slug("hello2"),
        loom::Content("My second blog post"),
        {},
        std::chrono::system_clock::now(),
    });

    source.add_post({
        loom::PostId("3"),
        loom::Title("Hello World3"),
        loom::Slug("hello3"),
        loom::Content("My third blog post"),
        {},
        std::chrono::system_clock::now(),
    });

    auto site = loom::build_site(source, {
        .title = "My Blog",
        .description = "A simple blog engine",
        .author = "John Doe",
        .navigation = {{
            {"Home", "/"},
            {"Blog", "/"},
            {"About", "/about"},
            {"Projects", "/projects"},
        }},
        .theme = {"default"},
    });

    loom::BlogEngine engine(site);

    loom::HttpServer server(8080);

    server.router().get("/", [&](const loom::HttpRequest&)
    {
        auto posts = engine.list_posts();
        auto content = render_index(posts);
        auto nav_html = render_navigation(site.navigation);

        return loom::HttpResponse::ok(loom::render_layout(site.title, nav_html, content));
    });

    server.router().get("/post/:slug", [&](const loom::HttpRequest& req) {
        auto post = engine.get_post(loom::Slug(req.params[0]));

        if(!post)
            return loom::HttpResponse::not_found(
                "<h1>Post <b>" + req.params[0] + "</b> not found.</h1>");

        auto content = loom::render_post(*post);
        auto nav = loom::render_navigation(site.navigation);

        return loom::HttpResponse::ok(loom::render_layout(site.title, nav, content));
    });

    server.run();
}
