#pragma once

#include "../domain/site.hpp"
#include "../domain/post_summary.hpp"
#include "../core/types.hpp"

#include <algorithm>
#include <map>
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
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;
                result.push_back(make_summary(p));
            }

            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.published > b.published;
            });

            return result;
        }

        std::vector<PostSummary> posts_by_tag(const Tag& tag) const
        {
            std::vector<PostSummary> result;
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;
                for (const auto& t : p.tags)
                {
                    if (t.get() == tag.get())
                    {
                        result.push_back(make_summary(p));
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
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;
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

        // Published posts in the same series, sorted by series_order
        std::vector<PostSummary> posts_in_series(const std::string& series) const
        {
            std::vector<PostSummary> result;
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;
                if (p.series == series)
                    result.push_back(make_summary(p));
            }
            std::sort(result.begin(), result.end(), [&](const auto& a, const auto& b) {
                // Find original posts to compare series_order
                int oa = 0, ob = 0;
                for (const auto& p : site_.posts)
                {
                    if (p.slug.get() == a.slug.get()) oa = p.series_order;
                    if (p.slug.get() == b.slug.get()) ob = p.series_order;
                }
                return oa < ob;
            });
            return result;
        }

        // All unique series names
        std::vector<std::string> all_series() const
        {
            std::set<std::string> seen;
            std::vector<std::string> result;
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now || p.series.empty())
                    continue;
                if (seen.insert(p.series).second)
                    result.push_back(p.series);
            }
            std::sort(result.begin(), result.end());
            return result;
        }

        // Related posts by tag overlap, excluding the given post
        std::vector<PostSummary> related_posts(const Post& post, int count = 3) const
        {
            auto now = std::chrono::system_clock::now();
            std::vector<std::pair<int, PostSummary>> scored;

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;
                if (p.slug.get() == post.slug.get())
                    continue;

                int overlap = 0;
                for (const auto& t1 : post.tags)
                    for (const auto& t2 : p.tags)
                        if (t1.get() == t2.get()) ++overlap;

                if (overlap > 0)
                    scored.push_back({overlap, make_summary(p)});
            }

            std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
                if (a.first != b.first) return a.first > b.first;
                return a.second.published > b.second.published;
            });

            std::vector<PostSummary> result;
            for (int i = 0; i < count && i < static_cast<int>(scored.size()); ++i)
                result.push_back(std::move(scored[i].second));
            return result;
        }

        // Posts grouped by year (descending)
        std::map<int, std::vector<PostSummary>, std::greater<int>> posts_by_year() const
        {
            std::map<int, std::vector<PostSummary>, std::greater<int>> result;
            auto now = std::chrono::system_clock::now();

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now)
                    continue;

                auto time = std::chrono::system_clock::to_time_t(p.published);
                std::tm tm{};
                gmtime_r(&time, &tm);
                result[tm.tm_year + 1900].push_back(make_summary(p));
            }

            for (auto& [year, posts] : result)
            {
                std::sort(posts.begin(), posts.end(), [](const auto& a, const auto& b) {
                    return a.published > b.published;
                });
            }

            return result;
        }

        // Get prev/next posts relative to a given post (by publish date)
        std::pair<std::optional<PostSummary>, std::optional<PostSummary>>
        prev_next(const Post& post) const
        {
            auto sorted = list_posts();
            std::optional<PostSummary> prev, next;

            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (sorted[i].slug.get() == post.slug.get())
                {
                    if (i + 1 < sorted.size())
                        prev = sorted[i + 1]; // older
                    if (i > 0)
                        next = sorted[i - 1]; // newer
                    break;
                }
            }
            return {prev, next};
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

        static PostSummary make_summary(const Post& p)
        {
            return {p.id, p.title, p.slug, p.published, p.tags, p.excerpt, p.reading_time_minutes};
        }
    };
}
