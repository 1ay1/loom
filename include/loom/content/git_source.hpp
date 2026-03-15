#pragma once

#include "../domain/post.hpp"
#include "../domain/page.hpp"
#include "../domain/site.hpp"
#include "../engine/site_builder.hpp"
#include "../reload/change_event.hpp"

#include <string>
#include <vector>

namespace loom
{

struct GitSourceConfig
{
    std::string repo_path;
    std::string branch = "main";
    std::string content_prefix;     // subdirectory within repo, e.g. "content"
};

class GitSource
{
public:
    explicit GitSource(GitSourceConfig config);

    std::vector<Post> all_posts();
    std::vector<Page> all_pages();

    SiteConfig site_config() const;
    Theme theme() const;

    // Reloadable: selectively reload based on what changed
    void reload(const ChangeSet& changes);

    const std::string& content_dir() const { return config_.repo_path; }

private:
    GitSourceConfig config_;

    std::vector<Post> posts_;
    std::vector<Page> pages_;
    SiteConfig config_data_;
    Theme theme_;

    // Resolved ref for git commands (e.g. "main", "refs/heads/main")
    std::string ref() const { return config_.branch; }

    // Content path prefix within the repo
    std::string prefix(const std::string& path) const;

    void load();
    void load_config();
    void load_theme();
    void load_posts();
    void load_pages();

    // Read a file from the git tree
    std::string read_blob(const std::string& path) const;

    // List files in a subdirectory of the content prefix
    std::vector<std::string> list_files(const std::string& subdir, const std::string& ext) const;

    // List subdirectories within a directory
    std::vector<std::string> list_subdirs(const std::string& subdir) const;

    // Load a single post from a git blob path
    Post load_git_post(const std::string& rel_path, const std::string& series_name, int& counter) const;
};

}
