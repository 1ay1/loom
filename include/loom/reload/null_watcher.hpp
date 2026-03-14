#pragma once

#include "change_event.hpp"
#include "watch_policy.hpp"

#include <atomic>
#include <optional>

namespace loom
{

// Null watcher for in-memory sources. Changes are triggered manually
// via signal(), not by filesystem events.
class NullWatcher
{
public:
    void start() {}
    void stop()  {}

    std::optional<ChangeSet> poll()
    {
        if (dirty_.exchange(false))
            return ChangeSet::everything();
        return std::nullopt;
    }

    // Manual trigger — call this after mutating the MemorySource
    void signal()
    {
        dirty_.store(true);
    }

private:
    std::atomic<bool> dirty_{false};
};

static_assert(WatchPolicy<NullWatcher>);

}
