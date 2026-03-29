#include "../../include/loom/content/git_source.hpp"
#include "../../include/loom/util/config_parser.hpp"
#include "../../include/loom/util/markdown.hpp"
#include "../../include/loom/util/git.hpp"

#include <algorithm>
#include <sstream>

namespace loom
{

GitSource::GitSource(GitSourceConfig config)
    : config_(std::move(config))
{
    // Verify git is available and repo exists
    git_rev_parse(config_.repo_path, config_.branch);
    load();
}

std::string GitSource::prefix(const std::string& path) const
{
    if (config_.content_prefix.empty())
        return path;
    return config_.content_prefix + "/" + path;
}

std::string GitSource::read_blob(const std::string& path) const
{
    try
    {
        return git_read_blob(config_.repo_path, ref(), prefix(path));
    }
    catch (const GitError&)
    {
        return "";
    }
}

std::vector<std::string> GitSource::list_files(const std::string& subdir, const std::string& ext) const
{
    return git_list_tree(config_.repo_path, ref(), prefix(subdir), ext);
}

std::vector<std::string> GitSource::list_subdirs(const std::string& subdir) const
{
    std::vector<std::string> result;
    try
    {
        auto lines = git_exec_lines(config_.repo_path,
            "ls-tree --name-only -d " + ref() + " " + prefix(subdir) + "/");
        for (const auto& line : lines)
        {
            auto name = line;
            auto slash = name.rfind('/');
            if (slash != std::string::npos)
                name = name.substr(slash + 1);
            if (!name.empty())
                result.push_back(name);
        }
    }
    catch (const GitError&) {}

    std::sort(result.begin(), result.end());
    return result;
}

void GitSource::load()
{
    load_config();
    load_theme();
    load_posts();
    load_pages();
}

static std::chrono::system_clock::time_point parse_date(const std::string& date_str)
{
    std::tm tm{};
    tm.tm_isdst = 0;
    if (date_str.size() >= 10)
    {
        tm.tm_year = std::stoi(date_str.substr(0, 4)) - 1900;
        tm.tm_mon  = std::stoi(date_str.substr(5, 2)) - 1;
        tm.tm_mday = std::stoi(date_str.substr(8, 2));
    }
    // Parse optional THH:MM:SS
    if (date_str.size() >= 19 && date_str[10] == 'T')
    {
        tm.tm_hour = std::stoi(date_str.substr(11, 2));
        tm.tm_min  = std::stoi(date_str.substr(14, 2));
        tm.tm_sec  = std::stoi(date_str.substr(17, 2));
    }
    auto time = timegm(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

void GitSource::load_config()
{
    auto text = read_blob("site.conf");
    if (text.empty()) return;

    auto cfg = parse_config(text);

    config_data_.title       = cfg["title"];
    config_data_.description = cfg["description"];
    config_data_.author      = cfg["author"];
    config_data_.base_url    = cfg["base_url"];

    if (!config_data_.base_url.empty() && config_data_.base_url.back() == '/')
        config_data_.base_url.pop_back();

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

            auto trim = [](std::string& s) {
                auto start = s.find_first_not_of(" \t");
                auto end = s.find_last_not_of(" \t");
                s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };
            trim(title);
            trim(url);

            config_data_.navigation.items.push_back({title, url});
        }
    }

    if (cfg.count("theme"))
        theme_.name = cfg["theme"];

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

    // Sidebar config
    if (cfg.count("sidebar_widgets"))
    {
        std::istringstream ss(cfg["sidebar_widgets"]);
        std::string widget;
        while (std::getline(ss, widget, ','))
        {
            auto start = widget.find_first_not_of(" \t");
            auto end = widget.find_last_not_of(" \t");
            if (start != std::string::npos)
                config_data_.sidebar.widgets.push_back(widget.substr(start, end - start + 1));
        }
    }
    if (cfg.count("sidebar_recent_count"))
        config_data_.sidebar.recent_posts_count = std::stoi(cfg["sidebar_recent_count"]);
    if (cfg.count("sidebar_about"))
        config_data_.sidebar.about_text = cfg["sidebar_about"];

    // Layout config
    if (cfg.count("header_style"))
        config_data_.layout.header_style = cfg["header_style"];
    if (cfg.count("show_description"))
        config_data_.layout.show_description = (cfg["show_description"] == "true");
    if (cfg.count("show_theme_toggle"))
        config_data_.layout.show_theme_toggle = (cfg["show_theme_toggle"] != "false");
    if (cfg.count("post_list_style"))
        config_data_.layout.post_list_style = cfg["post_list_style"];
    if (cfg.count("show_post_dates"))
        config_data_.layout.show_post_dates = (cfg["show_post_dates"] != "false");
    if (cfg.count("show_post_tags"))
        config_data_.layout.show_post_tags = (cfg["show_post_tags"] != "false");
    if (cfg.count("show_excerpts"))
        config_data_.layout.show_excerpts = (cfg["show_excerpts"] != "false");
    if (cfg.count("show_reading_time"))
        config_data_.layout.show_reading_time = (cfg["show_reading_time"] != "false");
    if (cfg.count("show_breadcrumbs"))
        config_data_.layout.show_breadcrumbs = (cfg["show_breadcrumbs"] != "false");
    if (cfg.count("external_links_new_tab"))
        config_data_.layout.external_links_new_tab = (cfg["external_links_new_tab"] == "true");
    if (cfg.count("sidebar_position"))
        config_data_.layout.sidebar_position = cfg["sidebar_position"];
    if (cfg.count("date_format"))
        config_data_.layout.date_format = cfg["date_format"];
    if (cfg.count("custom_css"))
        config_data_.layout.custom_css = cfg["custom_css"];
    if (cfg.count("custom_head_html"))
        config_data_.layout.custom_head_html = cfg["custom_head_html"];
    if (cfg.count("posts_per_page"))
        config_data_.layout.posts_per_page = std::stoi(cfg["posts_per_page"]);

    // Footer
    config_data_.footer.copyright = cfg["footer_copyright"];

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

            config_data_.footer.links.push_back({title, url});
        }
    }
}

void GitSource::load_theme()
{
    auto css = read_blob("theme/style.css");
    if (!css.empty())
    {
        theme_.name = "custom";
        theme_.css = css;
    }
}

static std::string first_img_src(const std::string& html)
{
    auto pos = html.find("<img ");
    if (pos == std::string::npos) return "";
    auto src = html.find("src=\"", pos);
    if (src == std::string::npos) return "";
    src += 5;
    auto end = html.find('"', src);
    if (end == std::string::npos) return "";
    return html.substr(src, end - src);
}

Post GitSource::load_git_post(const std::string& rel_path, const std::string& series_name, int& counter) const
{
    auto text = read_blob(rel_path);
    auto doc = parse_frontmatter(text);

    // Filename from path
    auto fname = rel_path.substr(rel_path.rfind('/') + 1);
    auto slug = doc.meta.count("slug") ? doc.meta["slug"] : "";
    if (slug.empty())
        slug = fname.substr(0, fname.size() - 3);

    auto date = doc.meta.count("date")
        ? parse_date(doc.meta["date"])
        : git_first_commit_date(config_.repo_path, prefix(rel_path));

    auto mtime = git_last_commit_date(config_.repo_path, prefix(rel_path));

    std::vector<Tag> tags;
    if (doc.meta.count("tags"))
    {
        std::istringstream ss(doc.meta["tags"]);
        std::string tag;
        while (std::getline(ss, tag, ','))
        {
            auto start = tag.find_first_not_of(" \t[");
            auto end = tag.find_last_not_of(" \t]");
            if (start != std::string::npos)
                tags.push_back(Tag(tag.substr(start, end - start + 1)));
        }
    }

    bool draft = false;
    if (doc.meta.count("draft"))
        draft = (doc.meta["draft"] == "true");

    bool featured = false;
    if (doc.meta.count("featured"))
        featured = (doc.meta["featured"] == "true");

    auto html_content = markdown_to_html(doc.body);

    int reading_time = 0;
    {
        int words = 0;
        bool in_word = false;
        bool in_html_tag = false;
        for (char c : html_content)
        {
            if (c == '<') { in_html_tag = true; continue; }
            if (c == '>') { in_html_tag = false; continue; }
            if (in_html_tag) continue;

            bool is_space = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
            if (!is_space && !in_word) { ++words; in_word = true; }
            else if (is_space) { in_word = false; }
        }
        reading_time = std::max(1, (words + 100) / 200);
    }

    std::string excerpt;
    if (doc.meta.count("excerpt"))
    {
        excerpt = doc.meta["excerpt"];
    }
    else
    {
        bool in_html_tag = false;
        for (char c : html_content)
        {
            if (c == '<') { in_html_tag = true; continue; }
            if (c == '>') { in_html_tag = false; continue; }
            if (in_html_tag) continue;
            if (c == '\n') c = ' ';
            excerpt += c;
            if (excerpt.size() >= 200) break;
        }
        if (excerpt.size() >= 200)
        {
            auto last_space = excerpt.rfind(' ');
            if (last_space != std::string::npos && last_space > 100)
                excerpt = excerpt.substr(0, last_space) + "...";
        }
    }

    std::string image;
    if (doc.meta.count("image"))
        image = doc.meta["image"];
    else
        image = first_img_src(html_content);

    return Post{
        PostId(std::to_string(counter++)),
        Title(doc.meta.count("title") ? doc.meta["title"] : slug),
        Slug(slug),
        Content(std::move(html_content)),
        std::move(tags),
        date,
        draft,
        featured,
        std::move(excerpt),
        std::move(image),
        reading_time,
        Series(series_name),
        mtime,
    };
}

void GitSource::load_posts()
{
    int counter = 1;

    // Top-level posts (no series)
    auto files = list_files("posts", ".md");
    for (const auto& filename : files)
    {
        auto text = read_blob("posts/" + filename);
        if (text.empty()) continue;
        posts_.push_back(load_git_post("posts/" + filename, "", counter));
    }

    // Subdirectories = series
    auto subdirs = list_subdirs("posts");
    for (const auto& dir : subdirs)
    {
        auto series_files = list_files("posts/" + dir, ".md");
        for (const auto& filename : series_files)
        {
            auto text = read_blob("posts/" + dir + "/" + filename);
            if (text.empty()) continue;
            posts_.push_back(load_git_post("posts/" + dir + "/" + filename, dir, counter));
        }
    }
}

void GitSource::load_pages()
{
    auto files = list_files("pages", ".md");

    for (const auto& filename : files)
    {
        auto text = read_blob("pages/" + filename);
        if (text.empty()) continue;

        auto doc = parse_frontmatter(text);

        auto slug = doc.meta.count("slug") ? doc.meta["slug"] : "";
        if (slug.empty())
        {
            slug = filename.substr(0, filename.size() - 3);
        }

        auto page_html = markdown_to_html(doc.body);
        std::string image;
        if (doc.meta.count("image"))
            image = doc.meta["image"];

        pages_.push_back(Page{
            Slug(slug),
            Title(doc.meta.count("title") ? doc.meta["title"] : slug),
            Content(std::move(page_html)),
            std::move(image),
        });
    }
}

void GitSource::reload(const ChangeSet& changes)
{
    if (changes.config)
    {
        config_data_ = {};
        theme_ = {};
        load_config();
    }
    if (changes.theme)
        load_theme();
    if (changes.posts)
    {
        posts_.clear();
        load_posts();
    }
    if (changes.pages)
    {
        pages_.clear();
        load_pages();
    }
}

std::vector<Post> GitSource::all_posts()
{
    return posts_;
}

std::vector<Page> GitSource::all_pages()
{
    return pages_;
}

SiteConfig GitSource::site_config() const
{
    auto cfg = config_data_;
    cfg.theme = theme_;
    return cfg;
}

Theme GitSource::theme() const
{
    return theme_;
}

}
