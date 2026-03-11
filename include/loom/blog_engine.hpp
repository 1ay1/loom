#pragma once

#include "content_source.hpp"
#include "types.hpp"

namespace loom
{
    template<ContentSource Source>
    class BlogEngine
    {
        public:
        explicit BlogEngine(Source source):
            source_(std::move(source)) {}

        auto list_posts()
        {
            return source_.list_posts();
        }

        auto get_post(Slug slug)
        {
            return source_.get_post(slug);
        }

        private:
        Source source_;
    };
}
