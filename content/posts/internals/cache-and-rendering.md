---
title: Pre-Rendering and the Atomic Cache — How Loom Serves Sub-Millisecond HTML
date: 2024-02-05
slug: cache-and-rendering
tags: internals, architecture, cpp
excerpt: Every page is rendered once at startup, gzip-compressed, and stored in memory. Requests are hash table lookups. The cache swaps atomically on content changes.
---

Loom's core performance trick is simple: do all the work upfront. At startup, every post, page, tag listing, series page, RSS feed, sitemap, and 404 is rendered to HTML, minified, and gzip-compressed. Requests are hash table lookups. No template engine runs per request, no disk is touched, no markdown is parsed.

## The SiteCache

```cpp
struct CachedPage {
    std::string raw;      // minified HTML
    std::string gzipped;  // gzip-compressed HTML
    std::string etag;     // quoted hash for ETag/304
};

struct SiteCache {
    std::unordered_map<std::string, CachedPage> pages; // path → page
    CachedPage not_found;   // 404 page
    CachedPage sitemap;     // /sitemap.xml
    CachedPage robots;      // /robots.txt
    CachedPage rss;         // /feed.xml
};
```

`pages` is keyed by URL path: `"/post/hello-world"`, `"/tag/cpp"`, `"/"`. Looking up a request is `cache.pages.find(req.path)` — one hash table lookup.

## Building a CachedPage

Every page, regardless of type, goes through `make_cached`:

```cpp
CachedPage make_cached(const std::string& html) {
    CachedPage page;
    page.raw     = minify_html(html);
    page.etag    = "\"" + std::to_string(std::hash<std::string>{}(page.raw)) + "\"";
    page.gzipped = gzip_compress(page.raw);
    return page;
}
```

Three steps:

**1. Minify.** Strip redundant whitespace between HTML tags. A typical page goes from 50KB to 35KB. The minifier preserves `<pre>`, `<script>`, and `<style>` content verbatim — those need their whitespace.

**2. Hash.** `std::hash<std::string>` is the standard library's default hash. Wrapped in quotes, it becomes the ETag: `"14923847261"`. This is not a cryptographic hash — it doesn't need to be. It just needs to change when the content changes, and `std::hash` does that.

**3. Compress.** `gzip_compress` uses zlib's `deflateInit2` with `Z_BEST_COMPRESSION` (level 9) and the gzip format flag (`15 + 16`). Compression happens once; serving compressed content costs nothing extra per request.

## The Build Process

`build_cache` takes a content source, renders everything, and returns a `shared_ptr<SiteCache>`. Here's the structure:

```cpp
std::shared_ptr<SiteCache> build_cache(ContentSource& source) {
    auto cache = std::make_shared<SiteCache>();
    auto site  = build_site(source);    // load posts, pages, config
    BlogEngine engine(site);            // query layer (related posts, etc.)

    // Pre-render navigation HTML once — reused on every page
    auto nav = render_navigation(site.navigation);

    // Pre-render sidebar HTML once
    auto sidebar = render_sidebar(site, engine);

    // Index page
    cache->pages["/"] = make_cached(
        render_layout(site, nav, render_index(engine.list_posts(), site.layout), sidebar));

    // Every published, non-future post
    for (const auto& post : site.posts) {
        if (post.draft || post.published > now()) continue;

        PageMeta meta;
        meta.title          = post.title.get();
        meta.canonical_path = "/post/" + post.slug.get();
        meta.og_type        = "article";
        meta.published_date = format_iso_date(post.published);
        meta.tags           = /* tag strings */;
        meta.og_image       = post.image;

        auto content = render_post(post, engine.related(post), engine.prev_next(post), site.layout);
        cache->pages["/post/" + post.slug.get()] = make_cached(render_layout(site, nav, content, sidebar, meta));
    }

    // Tag pages, series pages, archive, 404, RSS, sitemap, robots...
    return cache;
}
```

Navigation and sidebar HTML are rendered once and reused for every page. They're identical across the whole site so this is safe, and it avoids re-rendering the same strings thousands of times.

## The Render Pipeline

For a post, the call chain is:

```
render_layout(site, nav, render_post(post, ...), sidebar, meta)
    └── render_post builds the article HTML:
            <h1>title</h1>
            <div class="post-meta">date · reading time</div>
            <div class="post-tags">...</div>
            <nav class="series-nav">...</nav>      (if in series)
            <div class="post-content">{{html}}</div>
            <nav class="post-nav">prev | next</nav>
            <section class="related-posts">...</section>
    └── render_layout wraps it in the full HTML document:
            <!DOCTYPE html>
            <head> meta, OG, JSON-LD, preload, CSS </head>
            <body>
              <header> nav </header>
              <div class="container">
                <main>
                  <nav class="breadcrumb">...</nav>
                  {{content}}
                </main>
                <aside class="sidebar">...</aside>
              </div>
              <footer>...</footer>
            </body>
```

The final HTML string goes into `make_cached`. Nothing in this pipeline touches the network or filesystem — it's pure string manipulation.

## Atomic Cache Swap

The cache lives behind an `AtomicCache<SiteCache>`:

```cpp
template<typename T>
class AtomicCache {
    std::shared_ptr<const T> ptr_;
    mutable std::mutex mutex_;
public:
    std::shared_ptr<const T> load() const {
        std::lock_guard lock(mutex_);
        return ptr_; // copies the shared_ptr (bumps ref count), releases lock
    }

    void store(std::shared_ptr<const T> new_ptr) {
        std::lock_guard lock(mutex_);
        ptr_ = std::move(new_ptr); // old ptr's ref count drops; freed if no readers
    }
};
```

The mutex only protects the pointer itself — a single word of memory. `load()` grabs the lock for microseconds, copies the `shared_ptr` (which atomically bumps the reference count), and releases the lock. The caller now holds a reference to the cache snapshot.

`store()` swaps the pointer under the lock. The old `shared_ptr` goes out of scope when the last holder releases it — which might be right now, or might be after a request finishes on the old cache.

**Why this works without per-request locking:** Each request calls `load()` once, gets a `shared_ptr`, and holds it for the duration of request handling. Even if a rebuild happens mid-request, the request's `shared_ptr` keeps the old cache alive. The new cache is only accessible to requests that call `load()` after the `store()`.

The read path is:

```cpp
auto cache = atomic_cache.load(); // ~microseconds under mutex
auto it = cache->pages.find(req.path); // lock-free hash lookup
return serve_cached(req, *it); // no locks at all
```

## Related Posts Scoring

`engine.related(post)` returns the 3 most related posts by tag overlap:

```cpp
std::vector<PostSummary> related(const Post& post) {
    std::vector<std::pair<int, PostSummary>> scored;
    for (const auto& other : posts_) {
        if (other.slug == post.slug) continue;
        int score = 0;
        for (const auto& tag : post.tags)
            if (std::find(other.tags.begin(), other.tags.end(), tag) != other.tags.end())
                score++;
        if (score > 0)
            scored.push_back({score, summarize(other)});
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    return top_n(scored, 3);
}
```

This runs at cache build time, not per request. The O(posts²) scoring is fast enough for any reasonable blog — 100 posts means 10,000 comparisons, done once.

## Sitemap Priority

The sitemap assigns priority scores based on page type:

| Page | Priority |
|---|---|
| Homepage | 1.0 |
| Posts | 0.8 |
| Static pages | 0.6 |
| Series pages | 0.5 |
| Archives, series index | 0.5 |
| Tag pages | 0.4 |
| Tags index | 0.3 |

These are hints for crawlers, not guarantees. The homepage is most important, deep tag pages least.

## Memory Usage

A 30KB HTML page after minification is maybe 22KB. After gzip compression, roughly 6KB. Both versions are stored. For 100 posts plus all listing pages, you're looking at roughly 100 × (22 + 6) KB = 2.8MB for the HTML cache. Well within reason for any modern machine.

The raw strings could be replaced with memory-mapped files or a custom allocator for extreme scale, but for a blog engine serving hundreds of thousands of requests per day, `std::string` is fine.
