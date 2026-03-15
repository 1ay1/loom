#pragma once

#include "change_event.hpp"
#include "watch_policy.hpp"
#include "../util/git.hpp"

#include <atomic>
#include <optional>
#include <string>

namespace loom
{

// Polls a git ref for new commits. When the commit hash changes,
// returns ChangeSet::everything() since a commit can touch anything.
class GitWatcher
{
public:
    GitWatcher(const std::string& repo_path, const std::string& branch, bool fetch_remote = false)
        : repo_path_(repo_path)
        , branch_(branch)
        , fetch_remote_(fetch_remote)
    {}

    ~GitWatcher()
    {
        stop();
    }

    GitWatcher(const GitWatcher&) = delete;
    GitWatcher& operator=(const GitWatcher&) = delete;

    void start()
    {
        if (running_.exchange(true))
            return;

        try
        {
            last_hash_ = git_rev_parse(repo_path_, branch_);
        }
        catch (const GitError&)
        {
            last_hash_.clear();
        }
    }

    void stop()
    {
        running_.store(false);
    }

    std::optional<ChangeSet> poll()
    {
        if (!running_.load())
            return std::nullopt;

        try
        {
            // For remote-cloned repos, fetch to update refs/heads/*
            if (fetch_remote_)
                git_fetch(repo_path_);

            auto current = git_rev_parse(repo_path_, branch_);
            if (current != last_hash_)
            {
                last_hash_ = current;
                return ChangeSet::everything();
            }
        }
        catch (const GitError&) {}

        return std::nullopt;
    }

private:
    std::string repo_path_;
    std::string branch_;
    std::string last_hash_;
    bool fetch_remote_ = false;
    std::atomic<bool> running_{false};
};

static_assert(WatchPolicy<GitWatcher>);

}
