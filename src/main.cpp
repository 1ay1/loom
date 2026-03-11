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

    source.add(post);
    source.add(post2);

    loom::BlogEngine engine(source);

    loom::HttpServer server(8080);

    server.set_handler([&](const std::string& path) -> std::string {
        std::cout << path << "\n";
        if(path == "/") {
            auto posts = engine.list_posts();
            return loom::render_index(posts);
        }

        if(path.starts_with("/post/")) {
            std::string slug = path.substr(6);
            auto post = engine.get_post(loom::Slug(slug));
            if(post) {
                return loom::render_post(*post);
            }
        }

        return std::string("<h1>404: Not Found<h1>");
    });


    server.run();
}
