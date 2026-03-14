#include "../../include/loom/content/filesystem_source.hpp"
#include "../../include/loom/util/config_parser.hpp"
#include "../../include/loom/util/markdown.hpp"

#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

namespace loom
{

FileSystemSource::FileSystemSource(const std::string& content_dir)
    : content_dir_(content_dir)
{
    load();
}

void FileSystemSource::load()
{
    load_config();
    load_theme();
    load_posts();
    load_pages();
}

std::string FileSystemSource::read_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::vector<std::string> FileSystemSource::list_files(const std::string& dir, const std::string& ext)
{
    std::vector<std::string> files;
    DIR* d = opendir(dir.c_str());
    if (!d) return files;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name = entry->d_name;
        if (name.size() > ext.size() &&
            name.substr(name.size() - ext.size()) == ext)
        {
            files.push_back(dir + "/" + name);
        }
    }

    closedir(d);
    std::sort(files.begin(), files.end());
    return files;
}

static std::chrono::system_clock::time_point parse_date(const std::string& date_str)
{
    std::tm tm{};
    // Parse YYYY-MM-DD
    if (date_str.size() >= 10)
    {
        tm.tm_year = std::stoi(date_str.substr(0, 4)) - 1900;
        tm.tm_mon  = std::stoi(date_str.substr(5, 2)) - 1;
        tm.tm_mday = std::stoi(date_str.substr(8, 2));
    }

    auto time = timegm(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

void FileSystemSource::load_config()
{
    auto text = read_file(content_dir_ + "/site.conf");
    if (text.empty()) return;

    auto cfg = parse_config(text);

    config_.title       = cfg["title"];
    config_.description = cfg["description"];
    config_.author      = cfg["author"];

    // Parse nav: Home:/,Blog:/blog,About:/about
    if (cfg.count("nav"))
    {
        std::istringstream ss(cfg["nav"]);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            auto colon = item.find(':');
            if (colon == std::string::npos) continue;

            auto title = item.substr(0, colon);
            auto url = item.substr(colon + 1);

            // Trim
            auto trim = [](std::string& s) {
                auto start = s.find_first_not_of(" \t");
                auto end = s.find_last_not_of(" \t");
                s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };
            trim(title);
            trim(url);

            config_.navigation.items.push_back({title, url});
        }
    }

    // Parse theme name
    if (cfg.count("theme"))
        theme_.name = cfg["theme"];

    // Parse theme variables (theme_* keys)
    for (const auto& [key, value] : cfg)
    {
        if (key.substr(0, 6) == "theme_")
        {
            std::string var_name = key.substr(6);
            for (auto& c : var_name)
                if (c == '_') c = '-';
            theme_.variables[var_name] = value;
        }
    }

    // Parse sidebar config
    if (cfg.count("sidebar_widgets"))
    {
        std::istringstream ss(cfg["sidebar_widgets"]);
        std::string widget;
        while (std::getline(ss, widget, ','))
        {
            auto start = widget.find_first_not_of(" \t");
            auto end = widget.find_last_not_of(" \t");
            if (start != std::string::npos)
                config_.sidebar.widgets.push_back(widget.substr(start, end - start + 1));
        }
    }
    if (cfg.count("sidebar_recent_count"))
        config_.sidebar.recent_posts_count = std::stoi(cfg["sidebar_recent_count"]);
    if (cfg.count("sidebar_about"))
        config_.sidebar.about_text = cfg["sidebar_about"];

    // Parse footer
    config_.footer.copyright = cfg["footer_copyright"];

    if (cfg.count("footer_links"))
    {
        std::istringstream ss(cfg["footer_links"]);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            auto colon = item.find(':');
            if (colon == std::string::npos) continue;

            auto title = item.substr(0, colon);
            auto url = item.substr(colon + 1);

            auto trim = [](std::string& s) {
                auto start = s.find_first_not_of(" \t");
                auto end = s.find_last_not_of(" \t");
                s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };
            trim(title);
            trim(url);

            config_.footer.links.push_back({title, url});
        }
    }
}

void FileSystemSource::load_theme()
{
    auto css = read_file(content_dir_ + "/theme/style.css");
    if (!css.empty())
    {
        theme_.name = "custom";
        theme_.css = css;
    }
}

void FileSystemSource::load_posts()
{
    auto files = list_files(content_dir_ + "/posts", ".md");
    int counter = 1;

    for (const auto& path : files)
    {
        auto text = read_file(path);
        if (text.empty()) continue;

        auto doc = parse_frontmatter(text);

        auto slug = doc.meta.count("slug") ? doc.meta["slug"] : "";
        if (slug.empty())
        {
            // Derive slug from filename
            auto fname = path.substr(path.rfind('/') + 1);
            slug = fname.substr(0, fname.size() - 3); // remove .md
        }

        auto date = doc.meta.count("date") ? parse_date(doc.meta["date"])
                                            : std::chrono::system_clock::now();

        // Parse tags: comma-separated
        std::vector<Tag> tags;
        if (doc.meta.count("tags"))
        {
            std::istringstream ss(doc.meta["tags"]);
            std::string tag;
            while (std::getline(ss, tag, ','))
            {
                auto start = tag.find_first_not_of(" \t");
                auto end = tag.find_last_not_of(" \t");
                if (start != std::string::npos)
                    tags.push_back(Tag(tag.substr(start, end - start + 1)));
            }
        }

        posts_.push_back(Post{
            PostId(std::to_string(counter++)),
            Title(doc.meta.count("title") ? doc.meta["title"] : slug),
            Slug(slug),
            Content(markdown_to_html(doc.body)),
            std::move(tags),
            date,
        });
    }
}

void FileSystemSource::load_pages()
{
    auto files = list_files(content_dir_ + "/pages", ".md");

    for (const auto& path : files)
    {
        auto text = read_file(path);
        if (text.empty()) continue;

        auto doc = parse_frontmatter(text);

        auto slug = doc.meta.count("slug") ? doc.meta["slug"] : "";
        if (slug.empty())
        {
            auto fname = path.substr(path.rfind('/') + 1);
            slug = fname.substr(0, fname.size() - 3);
        }

        pages_.push_back(Page{
            Slug(slug),
            Title(doc.meta.count("title") ? doc.meta["title"] : slug),
            Content(markdown_to_html(doc.body)),
        });
    }
}

std::vector<Post> FileSystemSource::all_posts()
{
    return posts_;
}

std::vector<Page> FileSystemSource::all_pages()
{
    return pages_;
}

SiteConfig FileSystemSource::site_config() const
{
    auto cfg = config_;
    cfg.theme = theme_;
    return cfg;
}

Theme FileSystemSource::theme() const
{
    return theme_;
}

}
