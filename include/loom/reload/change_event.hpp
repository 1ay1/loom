#pragma once

#include <variant>
#include <vector>
#include <string>

namespace loom
{

// Sum type: each variant arm represents a category of content change
struct PostsChanged   { std::vector<std::string> paths; };
struct PagesChanged   { std::vector<std::string> paths; };
struct ConfigChanged  {};
struct ThemeChanged   {};

using ChangeEvent = std::variant<PostsChanged, PagesChanged, ConfigChanged, ThemeChanged>;

// Accumulates change events into a normalized set of flags
struct ChangeSet
{
    bool posts  = false;
    bool pages  = false;
    bool config = false;
    bool theme  = false;

    bool empty() const { return !posts && !pages && !config && !theme; }
    bool all()   const { return posts && pages && config && theme; }

    // Fold a single event into the set
    ChangeSet& operator<<(const ChangeEvent& ev)
    {
        std::visit(Absorb{*this}, ev);
        return *this;
    }

    // Fold multiple events
    ChangeSet& operator<<(const std::vector<ChangeEvent>& evs)
    {
        for (const auto& ev : evs) *this << ev;
        return *this;
    }

    // Merge two change sets
    ChangeSet operator|(const ChangeSet& other) const
    {
        return {
            posts  || other.posts,
            pages  || other.pages,
            config || other.config,
            theme  || other.theme,
        };
    }

    static ChangeSet everything()
    {
        return {true, true, true, true};
    }

private:
    struct Absorb
    {
        ChangeSet& cs;
        void operator()(const PostsChanged&)  { cs.posts  = true; }
        void operator()(const PagesChanged&)  { cs.pages  = true; }
        void operator()(const ConfigChanged&) { cs.config = true; }
        void operator()(const ThemeChanged&)  { cs.theme  = true; }
    };
};

}
