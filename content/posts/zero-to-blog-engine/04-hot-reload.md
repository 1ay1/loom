---
title: "Zero to Blog Engine: Hot Reload with Atomic Cache Swapping"
date: 2024-07-15
slug: hot-reload
tags: cpp, concurrency, loom
excerpt: Edit a post, see the change instantly. No restart, no downtime, no stale reads.
---

A blog engine that requires a restart to pick up content changes is a blog engine that breaks your flow. We need hot reload.

## The Architecture

The site is pre-rendered into an immutable cache at startup. Every request grabs a snapshot and serves from it. When content changes, we build a new cache and swap it in atomically.

```cpp
template<typename T>
class AtomicCache
{
public:
    std::shared_ptr<const T> load() const
    {
        std::lock_guard lock(mu_);
        return data_;
    }

    void store(std::shared_ptr<const T> next)
    {
        std::lock_guard lock(mu_);
        data_ = std::move(next);
    }

private:
    mutable std::mutex mu_;
    std::shared_ptr<const T> data_;
};
```

Readers get a `shared_ptr` snapshot — even if the cache is swapped mid-request, they keep serving from the old version. Zero contention on the read path in practice.

## Detecting Changes

On Linux, inotify tells us exactly what changed:

- `site.conf` modified? Reload config.
- File in `posts/` changed? Reload posts.
- `theme/style.css` updated? Reload theme.

We classify each event into a `ChangeSet` — a set of flags indicating what categories of content changed. This lets us reload only what's necessary.

## Debouncing

A single file save in most editors triggers multiple inotify events (write, truncate, close). Without debouncing, we'd rebuild the site three times for one edit.

The fix: when we detect a change, sleep 500ms, drain any additional events, then rebuild once with the accumulated change set.

## The Result

Edit a markdown file, save it, and the site updates within a second. No process restart, no cache invalidation bugs, no stale reads. The old cache stays valid until the new one is ready.
