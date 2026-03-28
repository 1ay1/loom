---
title: "Containers, Iterators, and Algorithms — Thinking in Collections"
date: 2025-08-08
slug: containers-and-iteration
tags: cpp, containers, algorithms, stl
excerpt: The standard library gives you the data structures. Your job is to pick the right one.
---

The previous post was about individual values. But programs don't deal with one value at a time — they deal with collections. A blog has posts. A request has headers. A tag cloud has tags. The standard library gives you a toolkit of containers, and the most important skill in C++ is knowing which one to reach for.

## vector: The Default Container

If you only learn one container, learn `std::vector`. It's a dynamically-sized array — contiguous in memory, fast to iterate, fast to append to:

```cpp
#include <vector>

std::vector<Post> posts;
posts.push_back(post1);
posts.push_back(post2);

size_t count = posts.size();
bool empty = posts.empty();
```

In Loom, the Site struct holds all posts in a vector:

```cpp
struct Site {
    std::vector<Post> posts;
    std::vector<Page> pages;
    // ...
};
```

Why vector? Because iteration is the most common operation on a collection of posts, and nothing beats contiguous memory for iteration speed. The CPU's cache prefetcher loads the next elements before you even ask for them. A vector of 1000 posts will iterate faster than a linked list of 100.

Access by index is O(1):

```cpp
const Post& first = posts[0];
const Post& last = posts.back();
```

Appending to the end is amortized O(1). Inserting in the middle is O(n). If you find yourself inserting in the middle frequently, you might want a different container. But most of the time, you don't. Most of the time, vector is the answer.

## unordered_map: The Hash Table

When you need to look up values by key, `std::unordered_map` is your friend:

```cpp
#include <unordered_map>

std::unordered_map<std::string, HttpResponse> cache;
cache["/"s] = index_response;
cache["/about"s] = about_response;

// Lookup — O(1) average
auto it = cache.find("/post/my-slug");
if (it != cache.end()) {
    return it->second;  // found it
}
```

Loom's HTTP server uses an `unordered_map` to track connections by file descriptor:

```cpp
class HttpServer {
private:
    std::unordered_map<int, Connection> connections_;
};
```

File descriptors are integers, so they hash trivially. Looking up a connection by fd is O(1). This matters when you're handling thousands of concurrent connections in an event loop — you don't want to scan a vector every time a socket becomes readable.

The inotify watcher uses the same pattern — mapping watch descriptors to their metadata:

```cpp
std::unordered_map<int, WatchEntry> watches_;
```

## map: The Ordered Map

Sometimes you need your keys in order. `std::map` is a balanced binary tree — O(log n) lookup, but keys are always sorted:

```cpp
#include <map>

// Posts grouped by year, descending order
std::map<int, std::vector<PostSummary>, std::greater<int>> posts_by_year;
```

This is from Loom's blog engine. The `std::greater<int>` comparator makes the map sort years in descending order (2025, 2024, 2023...), so when you iterate, the newest year comes first. An `unordered_map` can't guarantee any order.

Use `map` when you need sorted iteration. Use `unordered_map` when you need fast lookup and don't care about order. In practice, `unordered_map` is the right choice 90% of the time.

## set: Unique Elements

`std::set` is a sorted collection of unique elements:

```cpp
#include <set>

std::set<std::string> seen;
for (const auto& post : site.posts) {
    for (const auto& tag : post.tags) {
        seen.insert(tag.get());  // duplicates are silently ignored
    }
}
```

Loom's blog engine uses exactly this pattern to collect all unique tags and series names. The `set` handles deduplication automatically — you just throw values in and it figures out what's new.

There's also `std::unordered_set` if you don't need the elements sorted. Same tradeoff as `map` vs `unordered_map`.

## optional: Maybe a Value, Maybe Not

`std::optional<T>` is a container that holds zero or one value. It replaces the need for null pointers, sentinel values, and boolean flags:

```cpp
#include <optional>

std::optional<Post> get_post(Slug slug) const {
    for (const auto& p : site_.posts) {
        if (p.slug.get() == slug.get()) return p;
    }
    return std::nullopt;  // not found
}
```

This is from Loom's BlogEngine. The return type tells you everything: "this function might not find a post." The caller must handle both cases:

```cpp
auto post = engine.get_post(Slug("my-slug"));
if (post) {
    // use *post or post->title
} else {
    // handle not found
}
```

No null pointer dereferences. No "check the boolean first." The type system makes the optionality visible and enforced.

Loom also uses `optional` for post navigation:

```cpp
struct PostNavigation {
    std::optional<PostSummary> prev;
    std::optional<PostSummary> next;
};
```

The first post has no previous. The last post has no next. `optional` makes this explicit without dummy values or pointer gymnastics.

## pair and Structured Returns

`std::pair` bundles two values together:

```cpp
std::pair<std::optional<PostSummary>, std::optional<PostSummary>>
prev_next(const Post& post) const;
```

This function returns both the previous and next post in one call. But `pair` with nested optionals is hard to read, which is why Loom wraps it in the `PostNavigation` struct. Sometimes a named struct is better than a generic pair, even if the pair is technically sufficient.

## Range-For Loops

The range-for loop is how you iterate in modern C++:

```cpp
for (const auto& post : site.posts) {
    // post is a const reference to each Post
}

for (auto& [fd, conn] : connections_) {
    // fd is the key, conn is the value (mutable reference)
}
```

That second form — `auto& [fd, conn]` — is a structured binding. It destructures the key-value pair from the map. We'll cover structured bindings in detail in the next post.

The `const auto&` pattern means: iterate by const reference, let the compiler figure out the type. Use it whenever you're reading but not modifying elements:

```cpp
for (const auto& tag : post.tags) {
    if (tag.get() == target.get()) {
        result.push_back(make_summary(post));
        break;
    }
}
```

## Algorithms: Don't Write Loops When the Library Has Functions

The `<algorithm>` header is one of the most powerful parts of the standard library. Instead of writing a loop to sort, search, or transform a collection, you call a function:

```cpp
#include <algorithm>

// Sort posts by date (newest first)
std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    if (a.published != b.published) return a.published > b.published;
    return a.modified_at > b.modified_at;
});
```

This is from Loom's `list_posts()`. The third argument is a lambda (a function defined inline) that tells `sort` how to compare two elements. We'll cover lambdas properly in a later post — for now, just read it as "sort by published date descending, then by modified date as a tiebreaker."

`std::find_if` searches for the first element matching a predicate:

```cpp
auto it = std::find_if(posts.begin(), posts.end(),
    [&slug](const auto& p) { return p.slug.get() == slug.get(); });

if (it != posts.end()) {
    // found the post at *it
}
```

Why use algorithms instead of raw loops? Three reasons:

1. **Intent.** `std::sort` says "I'm sorting." A hand-written loop says "figure out what I'm doing."
2. **Correctness.** `std::sort` is tested and optimized. Your hand-written quicksort probably isn't.
3. **Optimization.** The standard library can use SIMD, branch-free comparisons, and other tricks that you'd never bother writing yourself.

## Iterators: The Glue Between Containers and Algorithms

Algorithms don't know about vectors or maps. They work with iterators — abstract pointers that know how to move through a container:

```cpp
auto begin = posts.begin();  // iterator to the first element
auto end = posts.end();      // iterator past the last element

std::sort(begin, end, comparator);
```

Every standard container provides `begin()` and `end()`. Every algorithm takes iterator pairs. This is the genius of the STL: N containers times M algorithms gives you N*M combinations, but you only need to implement N iterator types and M algorithms.

For most code, you won't interact with iterators directly — range-for and algorithms hide them. But understanding that they exist explains why `find` returns an iterator (not an optional) and why you compare it to `end()` to check for failure.

## Choosing the Right Container

Here's the decision tree I use:

- **Need a sequence of things?** `vector`.
- **Need fast lookup by key?** `unordered_map`.
- **Need sorted keys?** `map`.
- **Need unique elements?** `set` or `unordered_set`.
- **Need maybe-a-value?** `optional`.
- **Need a fixed-size collection?** `std::array`.

When in doubt, use `vector`. It's almost always faster than you think, thanks to cache locality. A linear scan through a vector of 100 elements is faster than a hash table lookup for those same 100 elements, because the vector lives in one cache line and the hash table scatters data across memory.

Loom's entire codebase uses vectors for posts, pages, tags, headers, children nodes, CSS declarations — everything that's a collection of things. Maps appear only where keyed lookup is the primary operation: connections by fd, watches by descriptor, posts grouped by year.

The standard library gives you the tools. Your job is to pick the right one for the access pattern. Get that right, and the rest follows.

Next up: structured bindings, designated initializers, and the modern syntax sugar that makes all this code readable.
