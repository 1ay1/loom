---
title: Hot Reload — Edit and See Changes Instantly HEHE
date: 2024-02-01
slug: hot-reload
tags: feature, architecture, git
excerpt: Loom watches your content directory with inotify and rebuilds the cache atomically. No restart needed.
---

## Filesystem Watching

Loom uses Linux inotify to watch your content directory. It monitors:

- `content/posts/` — new, modified, deleted, or moved posts
- `content/pages/` — same for static pages
- `content/site.conf` — configuration changes
- `content/theme/` — CSS changes

Changes are debounced for 500ms to batch rapid edits (like saving multiple files). Then the full cache is rebuilt and swapped.

## Atomic Cache Swap

The site cache is a `shared_ptr<const SiteCache>` — an immutable snapshot of every pre-rendered page. On rebuild:

1. New `SiteCache` is built in the background
2. The `shared_ptr` is atomically swapped
3. In-flight requests finish on the old cache
4. New requests see the new cache

No locks on the read path. No downtime during rebuild.

## Git Mode

In git source mode, the watcher polls `git rev-parse HEAD` instead of using inotify. When the commit hash changes, a full rebuild is triggered. Same atomic swap, same zero-downtime behavior.

## Watch Policies

Both watchers satisfy the `WatchPolicy` C++20 concept:

```cpp
template<typename W>
concept WatchPolicy = requires(W w) {
    { w.poll() } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop() } -> std::same_as<void>;
};
```

The hot reloader is generic over the watch policy. Adding a new watcher (webhook-based, polling an S3 bucket, etc.) means implementing three methods.
