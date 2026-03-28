---
title: "Linux inotify — Watching Files Change in Real Time"
date: 2025-10-31
slug: inotify-and-filesystem-watching
tags: cpp, inotify, linux, filesystem, hot-reload
excerpt: inotify tells you what changed on disk without polling. It's how Loom rebuilds your blog before you can alt-tab.
---

A blog engine that requires manual restarts after editing is a blog engine nobody wants to use. Loom watches your content directory for changes and rebuilds automatically — write a post, save it, and the site updates within a second. The mechanism behind this is Linux's `inotify` subsystem: a kernel interface that notifies your process when files change.

If epoll is "tell me when sockets are ready," inotify is "tell me when files change." Same philosophy: subscribe to events and react, instead of polling in a loop.

## The Polling Alternative (and Why It's Bad)

The naive approach to detecting file changes:

```cpp
// Don't do this
while (true) {
    for (const auto& file : watched_files) {
        auto mtime = std::filesystem::last_write_time(file);
        if (mtime != last_known_mtime[file]) {
            // file changed!
            handle_change(file);
            last_known_mtime[file] = mtime;
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

This polls every file every second. For a directory with 100 files, that's 100 stat calls per second. For 1000 files, 1000 calls. It's wasteful, it introduces latency (up to 1 second), and it misses rapid changes that happen between polls.

inotify solves all three problems: zero CPU when nothing changes, immediate notification when something does, and no missed events (within buffer limits).

## inotify_init1: Creating the Instance

```cpp
int fd = inotify_init1(IN_NONBLOCK);
```

Like `epoll_create1`, this creates a file descriptor that represents the inotify instance. `IN_NONBLOCK` makes reads non-blocking — essential for integration with an event loop or a polling thread.

Loom creates the inotify instance in its `InotifyWatcher::start()` method:

```cpp
void start() {
    if (running_.exchange(true))
        return;

    fd_ = inotify_init1(IN_NONBLOCK);
    if (fd_ < 0)
        return;

    // ... add watches ...
}
```

The `running_.exchange(true)` ensures idempotent start — calling `start()` twice is a no-op. This is the same atomic pattern we saw in the atomics post.

## inotify_add_watch: Subscribing to Changes

```cpp
constexpr uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE
                        | IN_MOVED_TO | IN_MOVED_FROM;
int wd = inotify_add_watch(fd_, path.c_str(), mask);
```

This tells the kernel: "notify me when files in `path` are created, modified, deleted, or moved." The returned `wd` (watch descriptor) is an integer that identifies this watch.

The event mask determines what changes you care about:

- **IN_CREATE**: A file or directory was created in the watched directory
- **IN_MODIFY**: A file's content was modified
- **IN_DELETE**: A file was deleted from the watched directory
- **IN_MOVED_TO**: A file was moved into the watched directory
- **IN_MOVED_FROM**: A file was moved out of the watched directory
- **IN_ISDIR**: Combined with the above, indicates the event is for a directory

Loom watches different directories with different classification functions:

```cpp
watch_dir(content_dir_,
    [](const std::string&) -> ChangeEvent { return ConfigChanged{}; });

watch_dir(content_dir_ + "/posts",
    [](const std::string& f) -> ChangeEvent { return PostsChanged{{f}}; });

watch_dir(content_dir_ + "/pages",
    [](const std::string& f) -> ChangeEvent { return PagesChanged{{f}}; });

watch_dir(content_dir_ + "/theme",
    [](const std::string&) -> ChangeEvent { return ThemeChanged{}; });
```

Each directory maps to a different `ChangeEvent` variant. Changes in `posts/` produce `PostsChanged`. Changes in `theme/` produce `ThemeChanged`. The classification function is stored alongside the watch descriptor:

```cpp
struct WatchEntry {
    std::string path;
    std::function<ChangeEvent(const std::string&)> classify;
};

std::unordered_map<int, WatchEntry> watches_;
```

When an event arrives, Loom looks up the watch descriptor in the map and calls the classify function to determine what kind of change occurred. The `ChangeEvent` variant carries the information downstream to the hot reloader.

## Reading Events: Variable-Length Structs

inotify events are read from the file descriptor like data from a socket. But there's a catch — events are variable-length:

```cpp
struct inotify_event {
    int      wd;       // watch descriptor
    uint32_t mask;     // event mask
    uint32_t cookie;   // for rename pairing
    uint32_t len;      // length of name field
    char     name[];   // flexible array member
};
```

The `name` field is a flexible array member — its length varies depending on the filename. The `len` field tells you how many bytes the name occupies (including padding to align the next event).

Reading events requires careful pointer arithmetic:

```cpp
alignas(inotify_event) char buf[4096];

for (;;) {
    auto len = read(fd_, buf, sizeof(buf));
    if (len <= 0)
        break;

    for (char* ptr = buf; ptr < buf + len; ) {
        auto* ev = reinterpret_cast<inotify_event*>(ptr);

        if (ev->mask & (IN_CREATE | IN_MODIFY | IN_DELETE
                       | IN_MOVED_TO | IN_MOVED_FROM)) {
            auto it = watches_.find(ev->wd);
            if (it != watches_.end()) {
                std::string name = (ev->len > 0) ? ev->name : "";
                changes << it->second.classify(name);
            }
        }

        ptr += sizeof(inotify_event) + ev->len;  // advance to next event
    }
}
```

Several things happening here:

**alignas**: The buffer is aligned to `inotify_event`'s alignment requirement. Without this, the `reinterpret_cast` would be undefined behavior on platforms with strict alignment.

**The read loop**: `read()` may return multiple events in a single call. The inner loop walks through the buffer, advancing by `sizeof(inotify_event) + ev->len` for each event.

**The outer loop**: With non-blocking I/O, `read()` returns `-1` with `EAGAIN` when there are no more events. The outer loop keeps reading until the buffer is drained.

**Event to ChangeEvent**: The watch descriptor is looked up in the map, the classify function is called, and the result is folded into the `ChangeSet` using the `operator<<` we saw in the variant post.

This is one of the rougher edges of C system programming: variable-length structs require manual memory management with pointer arithmetic and casts. There's no standard library wrapper that makes this pretty. You just have to get it right.

## Recursive Watching

inotify watches are per-directory — they don't automatically descend into subdirectories. If you have `posts/cpp/hello.md` and you watch `posts/`, you'll see when files in `posts/` change but not when files in `posts/cpp/` change.

Loom solves this by recursively watching subdirectories:

```cpp
void watch_dir(const std::string& path,
               std::function<ChangeEvent(const std::string&)> classify) {
    DIR* d = opendir(path.c_str());
    if (!d) return;

    int wd = inotify_add_watch(fd_, path.c_str(), mask);
    if (wd >= 0)
        watches_[wd] = {path, classify};

    // Recurse into subdirectories
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_type == DT_DIR &&
            std::strcmp(entry->d_name, ".") != 0 &&
            std::strcmp(entry->d_name, "..") != 0) {
            watch_dir(path + "/" + entry->d_name, classify);
        }
    }
    closedir(d);
}
```

And when a new subdirectory is created at runtime:

```cpp
// Auto-watch newly created subdirectories
if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) && (ev->mask & IN_ISDIR)) {
    watch_dir(it->second.path + "/" + name, it->second.classify);
}
```

When someone creates `posts/new-series/`, the IN_CREATE event triggers, Loom detects it's a directory (IN_ISDIR), and adds a watch for the new directory with the same classification function. This means new content directories are automatically monitored without restarting the watcher.

## Debouncing

Text editors don't save files atomically. Some write to a temporary file and rename. Some write the content then update the metadata. Some do multiple writes as auto-save fires. A single "save" operation can produce 3-5 inotify events in rapid succession.

Without debouncing, the hot reloader would rebuild 3-5 times for a single save. Loom debounces by waiting for events to settle:

```cpp
auto changes = watcher_.poll();
if (changes) {
    pending = pending | *changes;

    // Wait for more events to accumulate
    std::this_thread::sleep_for(debounce_);

    // Drain any further events that arrived during the wait
    while (auto more = watcher_.poll())
        pending = pending | *more;

    // Now rebuild once
    source_.reload(pending);
    auto new_cache = rebuild_(source_, pending);
    cache_.store(std::move(new_cache));
    pending = {};
}
```

The debounce window (default 500ms) allows multiple rapid events to coalesce into a single rebuild. The `ChangeSet` accumulates all flags — if both posts and config change within the window, one rebuild handles both.

## Cleanup

When the watcher stops, it removes all watches and closes the inotify file descriptor:

```cpp
void stop() {
    if (!running_.exchange(false))
        return;

    for (auto& [wd, _] : watches_)
        inotify_rm_watch(fd_, wd);
    watches_.clear();

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}
```

And the destructor calls `stop()`:

```cpp
~InotifyWatcher() {
    stop();
}
```

RAII at work. Whether the watcher goes out of scope normally, or an exception unwinds the stack, or `stop()` is called explicitly — the watches are removed and the file descriptor is closed. No resource leaks.

## The static_assert

At the bottom of the inotify_watcher.hpp file:

```cpp
static_assert(WatchPolicy<InotifyWatcher>);
```

This proves at compile time that `InotifyWatcher` satisfies the `WatchPolicy` concept — it has `poll()`, `start()`, and `stop()` with the right signatures. If the header compiles, the contract holds. This is the concepts post made real: a named requirement, verified at the point of definition.

## The Full Picture

inotify + debouncing + ChangeEvent variants + AtomicCache = hot reloading.

1. **inotify** tells Loom when files change (instant notification, zero polling overhead)
2. **Debouncing** coalesces rapid events into a single rebuild trigger
3. **ChangeEvent variants** classify changes by category (posts, pages, config, theme)
4. **ChangeSet** accumulates categories into a boolean flag set
5. **Source.reload()** re-reads only the changed content from disk
6. **The rebuild function** re-renders HTML and pre-serializes responses
7. **AtomicCache.store()** atomically publishes the new cache
8. **The event loop** picks up the new cache on its next request

All of this happens in under a second. Save a markdown file, and the blog is updated before you switch to your browser. That's the power of event-driven I/O: no polling, no wasted CPU, no latency.

Next: the final post — pointer-to-member, tag types, and type-level programming.
