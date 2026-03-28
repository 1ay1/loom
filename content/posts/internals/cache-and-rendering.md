---
title: Pre-Rendering and the Atomic Cache — How Loom Serves Sub-Millisecond HTML
date: 2025-10-20
slug: cache-and-rendering
tags: internals, architecture, cpp
excerpt: Every page is rendered once at startup and pre-serialized into complete HTTP wire responses. Requests are hash table lookups that return a pointer to pre-built bytes — no serialization, no body copy, no allocation.
---

Loom's core performance trick is simple: do all the work upfront. At startup, every post, page, tag listing, series page, RSS feed, sitemap, and 404 is rendered to HTML, minified, and gzip-compressed. Requests are hash table lookups. No template engine runs per request, no disk is touched, no markdown is parsed.

## The SiteCache

```cpp
struct CachedPage {
    std::string etag;
    std::string gzip_ka;          // complete HTTP response: gzip + keep-alive
    std::string gzip_close;       // complete HTTP response: gzip + close
    std::string plain_ka;         // complete HTTP response: plain + keep-alive
    std::string plain_close;      // complete HTTP response: plain + close
    std::string not_modified_ka;  // 304 response: keep-alive
    std::string not_modified_close; // 304 response: close
};

struct SiteCache {
    std::unordered_map<std::string, CachedPage, PageHash, std::equal_to<>> pages;
    CachedPage not_found;
    CachedPage sitemap;
    CachedPage robots;
    CachedPage rss;
};
```

Each `CachedPage` stores six pre-serialized HTTP wire responses — every combination of `{gzip, plain, 304} × {keep-alive, close}`. The server doesn't build responses per-request; it picks a pre-built string and writes it directly to the socket.

`pages` uses a transparent hash (`PageHash` with `is_transparent`) so lookups accept `string_view` without constructing a `std::string`. The request path is a `string_view` into the connection's read buffer — the lookup is `cache.pages.find(req.path)` with zero allocation.

## Building a CachedPage

Every page goes through `make_cached`, which produces six wire variants:

```cpp
CachedPage make_cached(std::string html, std::string_view content_type, int status) {
    html = minify_html(html);
    std::string etag = "\"" + std::to_string(std::hash<std::string>{}(html)) + "\"";
    std::string gz = gzip_compress(html);

    CachedPage page;
    page.etag = etag;

    // Each variant is a complete HTTP response: status line + headers + body
    page.gzip_ka = build_http_response(status_line, {
        {"Content-Type", content_type}, {"ETag", etag},
        {"Cache-Control", "public, max-age=60, must-revalidate"},
        {"Content-Encoding", "gzip"}
    }, gz, /*keep_alive=*/true);

    page.gzip_close = /* same with Connection: close */;
    page.plain_ka   = /* same without Content-Encoding, body is html */;
    // ... all six variants
    return page;
}
```

Four steps:

**1. Minify.** Strip redundant whitespace between HTML tags. A typical page goes from 50KB to 35KB. The minifier preserves `<pre>`, `<script>`, and `<style>` content verbatim.

**2. Hash.** `std::hash<std::string>` wrapped in quotes becomes the ETag: `"14923847261"`. Not cryptographic — just needs to change when content changes.

**3. Compress.** `gzip_compress` uses zlib level 9 in gzip format. Compression happens once.

**4. Pre-serialize.** Each variant is a complete HTTP response: `HTTP/1.1 200 OK\r\nContent-Type: ...\r\nETag: ...\r\nContent-Length: ...\r\nConnection: keep-alive\r\n\r\n<body>`. Status line, headers, and body concatenated into a single `std::string`. At serve time, the server picks one and writes it to the socket — no `serialize()` call, no body copy, no header formatting.

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
    std::atomic<std::shared_ptr<const T>> data_;
public:
    std::shared_ptr<const T> load() const noexcept {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept {
        data_.store(std::move(next), std::memory_order_release);
    }
};
```

C++20's `std::atomic<std::shared_ptr<T>>` makes both `load()` and `store()` lock-free. No mutex, no contention. Readers never block writers, writers never block readers.

**Why this works without per-request locking:** Each request calls `load()` once, gets a `shared_ptr`, and holds it for the duration of request handling. Even if a rebuild happens mid-request, the request's `shared_ptr` keeps the old cache alive. The new cache is only accessible to requests that call `load()` after the `store()`.

The read path is:

```cpp
auto snap = cache.load();                    // lock-free atomic load
auto it = snap->pages.find(req.path);        // hash lookup (string_view, no alloc)
return HttpResponse::prebuilt(snap, it->gzip_ka); // pointer into pre-serialized data
```

Zero locks. Zero copies. The `shared_ptr` in the response keeps the cache snapshot alive until the write completes — even if a hot-reload swaps in a new cache mid-write.

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

Each page stores six pre-serialized variants. A typical page: ~22KB minified HTML, ~6KB gzipped. The wire response adds ~200 bytes of headers per variant. The 304 responses are ~150 bytes each.

Per page: `(22 + 6) × 2` (ka + close for both plain and gzip) + negligible 304 overhead ≈ 56KB. For 100 posts plus listing pages: roughly 100 × 56KB ≈ 5.6MB. Well within reason for any modern machine, and the payoff is zero per-request work.

The raw HTML is not stored separately — it's embedded in the wire responses. This means the cache is slightly larger than the old approach (which stored raw + gzip + etag), but eliminates the per-request `serialize()` call and body copy that were the main latency bottleneck.
