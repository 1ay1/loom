#include <iostream>
#include <string>

#include "../include/loom/domain/domain.hpp"
#include "../include/loom/content/content.hpp"
#include "../include/loom/engine/engine.hpp"
#include "../include/loom/render/render.hpp"
#include "../include/loom/http/server.hpp"

int main()
{
    static_assert(loom::ContentSource<loom::MemorySource>);

    loom::MemorySource source;

    loom::Post post {
        loom::PostId("1"),
        loom::Title("Hello World"),
        loom::Slug("hello"),
        loom::Content("My first blog post"),
        {},
        std::chrono::system_clock::now(),
    };

    loom::Post post2 {
        loom::PostId("2"),
        loom::Title("Hello World2"),
        loom::Slug("hello2"),
        loom::Content("My first blog post2"),
        {},
        std::chrono::system_clock::now(),
    };

    loom::Post post3 {
        loom::PostId("3"),
        loom::Title("Hello World3"),
        loom::Slug("hello3"),
        loom::Content("My first blog post3"),
        {},
        std::chrono::system_clock::now(),
    };

    source.add(post);
    source.add(post2);
    source.add(post3);

    loom::BlogEngine engine(source);

    loom::HttpServer server(8080);

    server.router().add("/", [&](const loom::Params&) {
        auto posts = engine.list_posts();
        return loom::render_index(posts);
    });

    server.router().add("/post/:slug", [&](const loom::Params& params) {
        auto post = engine.get_post(loom::Slug(params[0]));
        if(post) return loom::render_post(*post);

        return std::string("<h1>Post <b>**" + params[0] + "**</b> not found.<h1>" );
    });

    server.run();
}
