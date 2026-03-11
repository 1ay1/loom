#pragma once

#include "post.hpp"
#include "post_summary.hpp"
#include "types.hpp"

#include <vector>
#include <optional>

namespace loom
{
    class MemorySource
    {
        public:
        void add(Post post)
        {
            posts_.push_back(std::move(post));
        }

        std::vector<PostSummary> list_posts()
        {
            std::vector<PostSummary> result;

            for(auto& p: posts_)
            {
                result.push_back(
                    PostSummary{p.id, p.title, p.slug}
                );
            }

            return result;
        }

        std::optional<Post> get_post(Slug slug)
        {
            for(auto& p: posts_)
            {
                if(p.slug.get() == slug.get()) return p;
            }

            return std::nullopt;
        }

        private:
        std::vector<Post> posts_;
    };
}
