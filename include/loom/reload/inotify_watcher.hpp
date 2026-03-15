#pragma once

#include "change_event.hpp"
#include "watch_policy.hpp"

#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>

#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace loom
{

// Inotify-based watcher for filesystem content sources.
// Maps directory paths to change event categories via a classification function.
class InotifyWatcher
{
public:
    explicit InotifyWatcher(const std::string& content_dir)
        : content_dir_(content_dir)
    {}

    ~InotifyWatcher()
    {
        stop();
    }

    InotifyWatcher(const InotifyWatcher&) = delete;
    InotifyWatcher& operator=(const InotifyWatcher&) = delete;

    void start()
    {
        if (running_.exchange(true))
            return;

        fd_ = inotify_init1(IN_NONBLOCK);
        if (fd_ < 0)
            return;

        // Watch content directories, classifying each by what it contains
        watch_dir(content_dir_, [](const std::string&) -> ChangeEvent { return ConfigChanged{}; });
        watch_dir(content_dir_ + "/posts", [](const std::string& f) -> ChangeEvent { return PostsChanged{{f}}; });
        watch_dir(content_dir_ + "/pages", [](const std::string& f) -> ChangeEvent { return PagesChanged{{f}}; });
        watch_dir(content_dir_ + "/theme", [](const std::string&) -> ChangeEvent { return ThemeChanged{}; });
    }

    void stop()
    {
        if (!running_.exchange(false))
            return;

        for (auto& [wd, _] : watches_)
            inotify_rm_watch(fd_, wd);
        watches_.clear();

        if (fd_ >= 0)
        {
            close(fd_);
            fd_ = -1;
        }
    }

    // Non-blocking poll. Drains inotify events, classifies them, and returns
    // accumulated changes. Returns nullopt if nothing happened.
    std::optional<ChangeSet> poll()
    {
        if (fd_ < 0)
            return std::nullopt;

        alignas(inotify_event) char buf[4096];
        ChangeSet changes;
        bool any = false;

        for (;;)
        {
            auto len = read(fd_, buf, sizeof(buf));
            if (len <= 0)
                break;

            for (char* ptr = buf; ptr < buf + len; )
            {
                auto* ev = reinterpret_cast<inotify_event*>(ptr);

                if (ev->mask & (IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM))
                {
                    auto it = watches_.find(ev->wd);
                    if (it != watches_.end())
                    {
                        std::string name = (ev->len > 0) ? ev->name : "";
                        changes << it->second.classify(name);
                        any = true;

                        // Auto-watch newly created subdirectories
                        if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) && (ev->mask & IN_ISDIR))
                        {
                            watch_dir(it->second.path + "/" + name, it->second.classify);
                        }
                    }
                }

                ptr += sizeof(inotify_event) + ev->len;
            }
        }

        return any ? std::optional{changes} : std::nullopt;
    }

private:
    struct WatchEntry
    {
        std::string path;
        std::function<ChangeEvent(const std::string&)> classify;
    };

    void watch_dir(const std::string& path, std::function<ChangeEvent(const std::string&)> classify)
    {
        // Ensure directory exists
        DIR* d = opendir(path.c_str());
        if (!d) return;

        constexpr uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM;
        int wd = inotify_add_watch(fd_, path.c_str(), mask);
        if (wd >= 0)
            watches_[wd] = {path, classify};

        // Recurse into subdirectories so nested content is watched too
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr)
        {
            if (entry->d_type == DT_DIR &&
                std::strcmp(entry->d_name, ".") != 0 &&
                std::strcmp(entry->d_name, "..") != 0)
            {
                watch_dir(path + "/" + entry->d_name, classify);
            }
        }
        closedir(d);
    }

    std::string content_dir_;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::unordered_map<int, WatchEntry> watches_;
};

static_assert(WatchPolicy<InotifyWatcher>);

}
