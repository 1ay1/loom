#pragma once

#include "post.hpp"
#include "page.hpp"
#include "navigation.hpp"
#include "theme.hpp"
#include "footer.hpp"

#include <string>
#include <vector>

namespace loom
{

struct SidebarConfig
{
    std::vector<std::string> widgets;
    int recent_posts_count = 5;
    std::string about_text;
};

struct LayoutConfig
{
    // Header
    std::string header_style = "default";   // default, centered, minimal
    bool show_description = false;
    bool show_theme_toggle = true;

    // Post listings
    std::string post_list_style = "list";   // list, cards
    bool show_post_dates = true;
    bool show_post_tags = true;
    bool show_excerpts = true;
    bool show_reading_time = true;

    // Sidebar
    std::string sidebar_position = "right"; // right, left, none

    // Date
    std::string date_format = "%Y-%m-%d";

    // Custom injections
    std::string custom_css;
    std::string custom_head_html;
};

struct PageMeta
{
    std::string title;
    std::string description;
    std::string canonical_path;     // e.g. "/post/my-slug"
    std::string og_type = "website"; // "article" for posts
    std::string published_date;     // ISO 8601 for articles
    std::string author;
    std::vector<std::string> tags;
    std::string og_image;
    bool noindex = false;
};

struct Site
{
    std::string title;
    std::string description;
    std::string author;
    std::string base_url;           // e.g. "https://example.com"

    std::vector<Post> posts;
    std::vector<Page> pages;
    Navigation navigation;
    Theme theme;
    Footer footer;
    SidebarConfig sidebar;
    LayoutConfig layout;
};

}
