#include <iostream>

#include "../include/loom/post.hpp"

int main()
{
    loom::Post post {
        loom::PostId("1"),
        loom::Title("Hello World"),
        loom::Slug("hello-world"),
        loom::Content("My first blog post"),
        {},
        std::chrono::system_clock::now(),
    };

    std::cout << "Post created\n";
}
