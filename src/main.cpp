#include <iostream>

#include "../include/loom/post.hpp"
#include "../include/loom/memory_source.hpp"
#include "../include/loom/content_source.hpp"

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

    source.add(post);

    auto posts = source.list_posts();
    for(auto& p: posts)
    {
        std::cout << source.get_post(p.slug).value().content.get() << "\n";
    }


}
