#pragma once

#include "../domain/site.hpp"
#include "../content/content_source.hpp"

namespace loom
{
    struct SiteConfig
    {
        std::string title;
        std::string description;
        std::string author;
        Navigation navigation;
        Theme theme;
        Footer footer;
    };

    template<ContentSource Source>
    Site build_site(Source& source, SiteConfig config)
    {
        return Site {
            .title = std::move(config.title),
            .description = std::move(config.description),
            .author = std::move(config.author),
            .posts = source.all_posts(),
            .pages = source.all_pages(),
            .navigation = std::move(config.navigation),
            .theme = std::move(config.theme),
            .footer = std::move(config.footer),
        };
    }
}
