#include <iostream>

#include "../include/loom/domain/post.hpp"
#include "../include/loom/content/memory_source.hpp"
#include "../include/loom/content/content_source.hpp"
#include "../include/loom/engine/blog_engine.hpp"

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

    std::cout << "Posts: " << posts.size() << "\n";
    for(auto& p: posts)
    {
        std::cout << engine.get_post(p.slug).value().content.get() << "\n";
    }


}
