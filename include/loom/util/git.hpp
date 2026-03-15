#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>

namespace loom
{

struct GitError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Run a git command in the given repo directory. Returns stdout.
// Throws GitError on non-zero exit.
std::string git_exec(const std::string& repo_path, const std::string& args);

// Run a git command and split stdout by newline.
std::vector<std::string> git_exec_lines(const std::string& repo_path, const std::string& args);

// Read a blob at a given ref and path: git show <ref>:<path>
std::string git_read_blob(const std::string& repo_path, const std::string& ref,
                          const std::string& path);

// List files in a tree directory: git ls-tree --name-only <ref> <dir>/
// Returns just filenames (not full paths).
std::vector<std::string> git_list_tree(const std::string& repo_path, const std::string& ref,
                                       const std::string& dir, const std::string& ext);

// Get the first commit date for a file (when it was added).
std::chrono::system_clock::time_point git_first_commit_date(const std::string& repo_path,
                                                             const std::string& path);

// Get the last commit date for a file (most recent modification).
std::chrono::system_clock::time_point git_last_commit_date(const std::string& repo_path,
                                                            const std::string& path);

// Get the current HEAD commit hash for a ref.
std::string git_rev_parse(const std::string& repo_path, const std::string& ref);

// Parse an ISO 8601 date string (git --format=%aI output) to time_point.
std::chrono::system_clock::time_point parse_iso8601(const std::string& date_str);

}
