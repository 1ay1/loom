#pragma once

#include "change_event.hpp"

#include <concepts>
#include <optional>

namespace loom
{

// Concept: a watch policy knows how to detect changes for a given source type.
// Each source type pairs with its own watcher — the watcher is parameterized
// on the source it watches, making the relationship a type-level witness.
template<typename W>
concept WatchPolicy = requires(W w)
{
    // Poll for accumulated changes since last call. Returns nullopt if nothing changed.
    { w.poll() } -> std::same_as<std::optional<ChangeSet>>;

    // Lifecycle
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};

// Concept: a content source that can reload itself given a change set
template<typename S>
concept Reloadable = requires(S s, const ChangeSet& cs)
{
    { s.reload(cs) } -> std::same_as<void>;
};

}
