#include <iostream>

#include "../include/loom/domain/domain.hpp"
#include "../include/loom/content/content.hpp"
#include "../include/loom/engine/engine.hpp"
#include "../include/loom/render/render.hpp"

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
    auto posts = engine.list_posts();
    auto html = loom::render_index(posts);

    std::cout << html;
}
