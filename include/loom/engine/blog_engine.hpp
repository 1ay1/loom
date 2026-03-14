#pragma once

#include "../domain/site.hpp"
#include "../domain/post_summary.hpp"
#include "../core/types.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

namespace loom
{
    class BlogEngine
    {
        public:
        explicit BlogEngine(const Site& site):
            site_(site) {}

        std::vector<PostSummary> list_posts() const
        {
            std::vector<PostSummary> result;
            for (const auto& p : site_.posts)
                result.push_back(PostSummary{p.id, p.title, p.slug, p.published, p.tags});

            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.published > b.published;
            });

            return result;
        }

        std::vector<PostSummary> posts_by_tag(const Tag& tag) const
        {
            std::vector<PostSummary> result;
            for (const auto& p : site_.posts)
            {
                for (const auto& t : p.tags)
                {
                    if (t.get() == tag.get())
                    {
                        result.push_back(PostSummary{p.id, p.title, p.slug, p.published, p.tags});
                        break;
                    }
                }
            }
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.published > b.published;
            });
            return result;
        }

        std::vector<Tag> all_tags() const
        {
            std::set<std::string> seen;
            std::vector<Tag> result;
            for (const auto& p : site_.posts)
            {
                for (const auto& t : p.tags)
                {
                    if (seen.insert(t.get()).second)
                        result.push_back(t);
                }
            }
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.get() < b.get();
            });
            return result;
        }

        std::optional<Post> get_post(Slug slug) const
        {
            for (const auto& p : site_.posts)
            {
                if (p.slug.get() == slug.get()) return p;
            }
            return std::nullopt;
        }

        std::optional<Page> get_page(Slug slug) const
        {
            for (const auto& p : site_.pages)
            {
                if (p.slug.get() == slug.get()) return p;
            }
            return std::nullopt;
        }

        const Site& site() const { return site_; }

        private:
        const Site& site_;
    };
}
