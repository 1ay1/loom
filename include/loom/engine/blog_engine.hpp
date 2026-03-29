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
    struct TagInfo
    {
        Tag tag;
        int count = 0;
        float weight = 0.0f;
    };

    struct SeriesInfo
    {
        Series name{""};
        int post_count = 0;
        std::chrono::system_clock::time_point first_date;
        std::chrono::system_clock::time_point last_date;
        Title latest_post_title{""};
    };

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
                if (a.published != b.published) return a.published > b.published;
                return a.modified_at > b.modified_at;
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
                if (a.published != b.published) return a.published > b.published;
                return a.modified_at > b.modified_at;
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

        // Published posts in the same series, sorted by publish date (oldest first)
        std::vector<PostSummary> posts_in_series(const Series& series) const
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
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.published < b.published;
            });
            return result;
        }

        // All unique series names
        std::vector<Series> all_series() const
        {
            std::set<Series> seen;
            std::vector<Series> result;
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

        std::vector<PostSummary> featured_posts() const
        {
            std::vector<PostSummary> result;
            auto now = std::chrono::system_clock::now();
            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now || !p.featured)
                    continue;
                result.push_back(make_summary(p));
            }
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                if (a.published != b.published) return a.published > b.published;
                return a.modified_at > b.modified_at;
            });
            return result;
        }

        std::vector<TagInfo> tag_info() const
        {
            std::map<std::string, int> counts;
            auto now = std::chrono::system_clock::now();
            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now) continue;
                for (const auto& t : p.tags)
                    ++counts[t.get()];
            }
            int max_count = 0;
            for (const auto& [_, c] : counts)
                if (c > max_count) max_count = c;

            std::vector<TagInfo> result;
            result.reserve(counts.size());
            for (auto& [name, c] : counts)
                result.push_back({Tag(name), c, max_count > 0 ? static_cast<float>(c) / max_count : 0.0f});

            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.tag.get() < b.tag.get();
            });
            return result;
        }

        std::vector<SeriesInfo> series_info() const
        {
            auto now = std::chrono::system_clock::now();
            std::map<std::string, SeriesInfo> info_map;

            for (const auto& p : site_.posts)
            {
                if (p.draft || p.published > now || p.series.empty())
                    continue;

                auto& si = info_map[p.series.get()];
                if (si.name.empty())
                {
                    si.name = p.series;
                    si.first_date = p.published;
                    si.last_date = p.published;
                    si.latest_post_title = p.title;
                }
                ++si.post_count;
                if (p.published < si.first_date) si.first_date = p.published;
                if (p.published > si.last_date)
                {
                    si.last_date = p.published;
                    si.latest_post_title = p.title;
                }
            }

            std::vector<SeriesInfo> result;
            result.reserve(info_map.size());
            for (auto& [_, si] : info_map)
                result.push_back(std::move(si));
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
                return a.last_date > b.last_date;
            });
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
                if (a.second.published != b.second.published) return a.second.published > b.second.published;
                return a.second.modified_at > b.second.modified_at;
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
                    if (a.published != b.published) return a.published > b.published;
                    return a.modified_at > b.modified_at;
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
            return {p.id, p.title, p.slug, p.published, p.tags, p.excerpt, p.reading_time_minutes, p.modified_at};
        }
    };
}
