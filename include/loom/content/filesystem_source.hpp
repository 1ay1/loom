#pragma once

#include "../domain/post.hpp"
#include "../domain/page.hpp"
#include "../domain/site.hpp"
#include "../engine/site_builder.hpp"

#include <string>
#include <vector>

namespace loom
{
    class FileSystemSource
    {
    public:
        explicit FileSystemSource(const std::string& content_dir);

        std::vector<Post> all_posts();
        std::vector<Page> all_pages();

        SiteConfig site_config() const;
        Theme theme() const;

    private:
        std::string content_dir_;

        std::vector<Post> posts_;
        std::vector<Page> pages_;
        SiteConfig config_;
        Theme theme_;

        void load();
        void load_config();
        void load_theme();
        void load_posts();
        void load_pages();

        static std::string read_file(const std::string& path);
        static std::vector<std::string> list_files(const std::string& dir, const std::string& ext);
    };
}
