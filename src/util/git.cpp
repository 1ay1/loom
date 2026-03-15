#include "../../include/loom/util/git.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <algorithm>
#include <filesystem>

namespace loom
{

std::string git_exec(const std::string& repo_path, const std::string& args)
{
    // Use timeout to prevent hung git processes from accumulating
    std::string cmd = "timeout 30 git -C " + repo_path + " " + args + " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw GitError("Failed to run: " + cmd);

    std::string result;
    std::array<char, 4096> buf;
    while (auto n = fread(buf.data(), 1, buf.size(), pipe))
        result.append(buf.data(), n);

    int status = pclose(pipe);
    if (status != 0)
        throw GitError("git command failed: " + args);

    // Trim trailing newline
    while (!result.empty() && result.back() == '\n')
        result.pop_back();

    return result;
}

std::vector<std::string> git_exec_lines(const std::string& repo_path, const std::string& args)
{
    auto output = git_exec(repo_path, args);
    if (output.empty())
        return {};

    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < output.size())
    {
        auto end = output.find('\n', start);
        if (end == std::string::npos)
        {
            lines.push_back(output.substr(start));
            break;
        }
        lines.push_back(output.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

std::string git_read_blob(const std::string& repo_path, const std::string& ref,
                          const std::string& path)
{
    return git_exec(repo_path, "show " + ref + ":" + path);
}

std::vector<std::string> git_list_tree(const std::string& repo_path, const std::string& ref,
                                       const std::string& dir, const std::string& ext)
{
    std::vector<std::string> result;

    std::vector<std::string> lines;
    try
    {
        lines = git_exec_lines(repo_path, "ls-tree --name-only " + ref + " " + dir + "/");
    }
    catch (const GitError&)
    {
        return result; // directory doesn't exist in tree
    }

    for (const auto& name : lines)
    {
        // ls-tree returns "dir/filename", strip the dir prefix
        std::string filename = name;
        auto slash = filename.rfind('/');
        if (slash != std::string::npos)
            filename = filename.substr(slash + 1);

        if (filename.size() > ext.size() &&
            filename.substr(filename.size() - ext.size()) == ext)
        {
            result.push_back(filename);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::chrono::system_clock::time_point parse_iso8601(const std::string& date_str)
{
    // Handles: 2026-03-15T10:30:00+05:30  or  2026-03-15T10:30:00Z
    std::tm tm{};
    if (date_str.size() < 10)
        return std::chrono::system_clock::now();

    tm.tm_year = std::stoi(date_str.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(date_str.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(date_str.substr(8, 2));

    if (date_str.size() >= 19)
    {
        tm.tm_hour = std::stoi(date_str.substr(11, 2));
        tm.tm_min  = std::stoi(date_str.substr(14, 2));
        tm.tm_sec  = std::stoi(date_str.substr(17, 2));
    }

    auto time = timegm(&tm);

    // Parse timezone offset if present (e.g. +05:30 or -08:00)
    if (date_str.size() > 19)
    {
        char tz_sign = date_str[19];
        if (tz_sign == '+' || tz_sign == '-')
        {
            int tz_hours = std::stoi(date_str.substr(20, 2));
            int tz_mins  = 0;
            if (date_str.size() >= 25)
                tz_mins = std::stoi(date_str.substr(23, 2));

            int offset_secs = (tz_hours * 3600 + tz_mins * 60);
            if (tz_sign == '+')
                time -= offset_secs;
            else
                time += offset_secs;
        }
    }

    return std::chrono::system_clock::from_time_t(time);
}

std::chrono::system_clock::time_point git_first_commit_date(const std::string& repo_path,
                                                             const std::string& path)
{
    try
    {
        auto output = git_exec(repo_path, "log --follow --diff-filter=A --format=%aI -- " + path);
        if (!output.empty())
            return parse_iso8601(output);
    }
    catch (const GitError&) {}

    return std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point git_last_commit_date(const std::string& repo_path,
                                                            const std::string& path)
{
    try
    {
        auto output = git_exec(repo_path, "log -1 --format=%aI -- " + path);
        if (!output.empty())
            return parse_iso8601(output);
    }
    catch (const GitError&) {}

    return std::chrono::system_clock::now();
}

std::string git_rev_parse(const std::string& repo_path, const std::string& ref)
{
    return git_exec(repo_path, "rev-parse " + ref);
}

bool is_remote_url(const std::string& path)
{
    // https://, git://, ssh://, or git@host:path
    if (path.find("://") != std::string::npos)
        return true;
    if (path.size() > 4 && path.substr(0, 4) == "git@" && path.find(':') != std::string::npos)
        return true;
    return false;
}

std::string git_clone_bare(const std::string& url, const std::string& dest)
{
    // Derive a directory name from the URL
    auto name = url;
    // Strip trailing .git
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git")
        name = name.substr(0, name.size() - 4);
    // Take the last path component
    auto slash = name.rfind('/');
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    auto colon = name.rfind(':');
    if (colon != std::string::npos)
        name = name.substr(colon + 1);

    auto local_path = dest + "/" + name + ".git";

    if (std::filesystem::exists(local_path))
    {
        // Already cloned — just fetch
        git_fetch(local_path);
        return local_path;
    }

    std::string cmd = "timeout 120 git clone --bare " + url + " " + local_path + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw GitError("Failed to run git clone");

    std::string output;
    std::array<char, 4096> buf;
    while (auto n = fread(buf.data(), 1, buf.size(), pipe))
        output.append(buf.data(), n);

    int status = pclose(pipe);
    if (status != 0)
        throw GitError("git clone failed: " + output);

    return local_path;
}

void git_fetch(const std::string& repo_path)
{
    try
    {
        git_exec(repo_path, "fetch --quiet origin");
    }
    catch (const GitError&)
    {
        // Fetch failure is non-fatal — we still have the old data
    }
}

}
