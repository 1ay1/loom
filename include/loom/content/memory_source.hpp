#pragma once

#include "../domain/post.hpp"
#include "../domain/page.hpp"

#include <vector>

namespace loom
{
    class MemorySource
    {
        public:
        void add_post(Post post)
        {
            posts_.push_back(std::move(post));
        }

        void add_page(Page page)
        {
            pages_.push_back(std::move(page));
        }

        std::vector<Post> all_posts()
        {
            return posts_;
        }

        std::vector<Page> all_pages()
        {
            return pages_;
        }

        private:
        std::vector<Post> posts_;
        std::vector<Page> pages_;
    };
}
