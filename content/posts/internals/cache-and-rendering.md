---
title: "Pre-Rendering and the Atomic Cache — Sub-Millisecond Responses"
date: 2025-12-12
slug: cache-and-rendering
tags: [loom-internals, c++20, performance, caching, http]
excerpt: "How Loom pre-renders every page into 6 HTTP wire variants, serves them with zero copies and zero allocations, and keeps responses under a millisecond."
---

Most web frameworks generate HTML on every request. Loom generates HTML zero times per request. Every page is pre-rendered at startup into complete HTTP responses — headers, body, gzip compression, ETag, everything. When a request arrives, the server writes pre-built bytes directly to the socket. No template rendering, no string concatenation, no memory allocation.

This is what makes Loom fast: it does all the work upfront and caches the results in a format that is ready to write to the wire.

This post builds on [post #2 on containers](/post/containers-and-iteration), [post #5 on shared_ptr](/post/ownership-and-move-semantics), and [post #6 on string_view](/post/string-view-and-zero-copy).

## SiteCache: The Central Data Structure

The entire pre-rendered site lives in a single struct:

```cpp
struct SiteCache
{
    std::unordered_map<std::string, CachedPage, PageHash, std::equal_to<>> pages;
    CachedPage not_found;
    CachedPage sitemap;
    CachedPage robots;
    CachedPage rss;
};
```

The `pages` map holds every HTML page keyed by URL path (`/`, `/post/some-slug`, `/tag/c++`, etc.). Fixed resources like the 404 page, sitemap, robots.txt, and RSS feed have their own fields.

The map uses a transparent hash and comparator — `PageHash` with `is_transparent` and `std::equal_to<>`:

```cpp
struct PageHash
{
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept
    { return std::hash<std::string_view>{}(sv); }
};
```

This is the technique from [post #6](/post/string-view-and-zero-copy). With a transparent hash, you can look up a `std::string_view` key in a `std::string`-keyed map without allocating a temporary `std::string`. The HTTP request gives us a `string_view` path (pointing into the read buffer), and we can use it directly for the map lookup. Zero copies on the hot path.

## CachedPage: Six Wire Variants

Each page is stored as six pre-serialized HTTP responses:

```cpp
struct CachedPage
{
    std::string etag;
    std::string gzip_ka;          // gzipped, Connection: keep-alive
    std::string gzip_close;       // gzipped, Connection: close
    std::string plain_ka;         // uncompressed, keep-alive
    std::string plain_close;      // uncompressed, close
    std::string not_modified_ka;  // 304, keep-alive
    std::string not_modified_close; // 304, close
};
```

Why six variants? Because the HTTP response differs based on three binary choices:

1. **Gzip or plain?** Depends on the client's `Accept-Encoding` header.
2. **Keep-alive or close?** Depends on the client's `Connection` header.
3. **Full response or 304?** Depends on whether the client sent a matching `If-None-Match` ETag.

That is 2 x 2 x 2 = 8 combinations, but the 304 response does not need gzip/plain variants (it has no body), so we get 4 + 2 = 6.

Each variant is a complete HTTP response: status line, headers, content-length, connection header, blank line, body. Ready to be written to a socket with a single `write()` call.

## The Build Pipeline

The `make_cached()` function converts raw HTML into a `CachedPage`:

```cpp
static CachedPage make_cached(std::string html,
    std::string_view content_type = "text/html; charset=utf-8",
    bool do_minify = true, int status = 200)
{
    if (do_minify)
        html = loom::minify_html(html);

    auto hash = std::hash<std::string>{}(html);
    std::string etag = "\"" + std::to_string(hash) + "\"";
    auto gz = loom::gzip_compress(html);

    auto sl = status_line_for(status);
    CachedPage page;
    page.etag = etag;

    page.gzip_ka = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"},
         {"Content-Encoding", "gzip"}},
        gz, true);
    page.gzip_close = build_http_response(sl,
        {{"Content-Type", content_type}, {"ETag", etag},
         {"Cache-Control", "public, max-age=60, must-revalidate"},
         {"Content-Encoding", "gzip"}},
        gz, false);
    page.plain_ka = build_http_response(sl, /* ... */, html, true);
    page.plain_close = build_http_response(sl, /* ... */, html, false);
    page.not_modified_ka = build_http_response("HTTP/1.1 304 Not Modified\r\n",
        {{"ETag", etag}, {"Cache-Control", "public, max-age=60, must-revalidate"}},
        "", true);
    page.not_modified_close = build_http_response("HTTP/1.1 304 Not Modified\r\n",
        {{"ETag", etag}, {"Cache-Control", "..."}}, "", false);

    return page;
}
```

The pipeline for each page:

1. **Minify**: Strip whitespace from HTML (unless `do_minify` is false, as for XML feeds).
2. **Hash**: Compute a `std::hash<std::string>` of the HTML for the ETag.
3. **Compress**: Gzip the HTML.
4. **Serialize**: Build six complete HTTP response strings.

The ETag is a quoted hash string like `"12345678901234"`. It is not a cryptographic hash — just `std::hash`, which is fast and collision-resistant enough for cache validation. The same ETag appears in all six variants.

`build_http_response()` constructs a complete HTTP response as a single string:

```cpp
static std::string build_http_response(
    std::string_view status_line,
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers,
    std::string_view body, bool keep_alive)
{
    std::string out;
    out.reserve(256 + body.size());
    out.append(status_line.data(), status_line.size());

    for (const auto& [k, v] : headers)
    {
        out.append(k.data(), k.size());
        out += ": ";
        out.append(v.data(), v.size());
        out += "\r\n";
    }

    out += "Content-Length: ";
    out += std::to_string(body.size());
    out += "\r\nConnection: ";
    out += keep_alive ? "keep-alive" : "close";
    out += "\r\n\r\n";
    out.append(body.data(), body.size());
    return out;
}
```

One allocation (the `reserve`), one linear write. The result is a string that can be written directly to a TCP socket.

## The Full Build

The `build_cache()` function orchestrates the entire process:

```cpp
template<loom::ContentSource Source>
static std::shared_ptr<const SiteCache> build_cache(Source& source)
{
    auto config = source.site_config();
    auto site = loom::build_site(source, std::move(config));
    loom::BlogEngine engine(site);
    auto ctx = component::Ctx::from(site);

    auto all_tags = engine.all_tags();
    auto all_series = engine.all_series();
    component::SidebarData sidebar_data{engine.list_posts(), all_tags, all_series};

    auto cache = std::make_shared<SiteCache>();

    // Index page
    auto posts = engine.list_posts();
    cache->pages["/"] = make_cached(
        render_page(meta, ctx(component::Index{.posts = &posts})));

    // Each post
    for (const auto& post : site.posts)
    {
        // ... build PostContext (prev/next, related, series)
        cache->pages["/post/" + post.slug.get()] = make_cached(
            render_page(meta, ctx(component::PostFull{.post = &post, .context = &pctx})));
    }

    // Tag pages, archives, series pages, static pages, 404, sitemap, robots, RSS
    // ... same pattern for each

    return cache;
}
```

The pattern is the same for every page:

1. Build the component props (which post, which tag, etc.)
2. Render to HTML via the component system: `ctx(Component{.props = ...})`
3. Wrap in the page layout: `ctx.page(meta, content, &sidebar_data).render()`
4. Convert to a cached page: `make_cached(html)`
5. Store in the map under the URL path

The result is a `shared_ptr<const SiteCache>` — immutable and reference-counted. This is critical for the hot reload system.

## Related Posts: Tag Overlap Scoring

One interesting part of the build is related post scoring:

```cpp
std::vector<PostSummary> related_posts(const Post& post, int count = 3) const
{
    std::vector<std::pair<int, PostSummary>> scored;

    for (const auto& p : site_.posts)
    {
        if (p.slug.get() == post.slug.get()) continue;

        int overlap = 0;
        for (const auto& t1 : post.tags)
            for (const auto& t2 : p.tags)
                if (t1.get() == t2.get()) ++overlap;

        if (overlap > 0)
            scored.push_back({overlap, make_summary(p)});
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second.published > b.second.published;
    });

    // Return top `count` results
}
```

For each post, count how many tags it shares with every other post. Sort by overlap (descending), then by date. Take the top 3. This is O(n * m^2) where n is the number of posts and m is the average tag count — fine for a blog with hundreds of posts and single-digit tags per post.

The scoring runs at build time, not request time. The results are baked into the HTML.

## Serving: Zero Copies

When a request arrives, the server selects the right variant:

```cpp
static loom::HttpResponse serve_cached(
    const CachedPage& page,
    const loom::HttpRequest& req,
    const std::shared_ptr<const SiteCache>& snap)
{
    bool ka = req.keep_alive();

    auto inm = req.header("if-none-match");
    if (!inm.empty() && inm == page.etag)
        return loom::HttpResponse::prebuilt(snap,
            ka ? page.not_modified_ka : page.not_modified_close);

    if (accepts_gzip(req))
        return loom::HttpResponse::prebuilt(snap,
            ka ? page.gzip_ka : page.gzip_close);

    return loom::HttpResponse::prebuilt(snap,
        ka ? page.plain_ka : page.plain_close);
}
```

Three checks: ETag match, gzip support, keep-alive. Each one selects a pre-built string.

The `HttpResponse::prebuilt()` static method creates a response that points directly into the cache:

```cpp
static HttpResponse prebuilt(std::shared_ptr<const void> owner,
                              const std::string& wire)
{
    HttpResponse r;
    r.wire_owner_ = std::move(owner);
    r.wire_data_ = wire.data();
    r.wire_len_ = wire.size();
    return r;
}
```

`wire_data_` is a raw pointer into the `SiteCache`'s string. `wire_owner_` is a `shared_ptr<const void>` that keeps the cache alive. The server writes from `wire_data_` directly — no copy, no allocation.

The `shared_ptr<const void>` trick is worth noting. The `SiteCache` is a `shared_ptr<const SiteCache>`, but the response stores a `shared_ptr<const void>`. This works because `shared_ptr` preserves the deleter regardless of the pointer type. You can convert `shared_ptr<const SiteCache>` to `shared_ptr<const void>` and the original deleter still runs when the reference count hits zero. This is discussed in [post #5 on ownership](/post/ownership-and-move-semantics).

## Memory Budget

Each `CachedPage` stores six strings. For a typical blog post:

- **Plain HTML**: ~15KB after minification
- **Gzipped HTML**: ~5KB
- **HTTP headers**: ~200 bytes per variant
- **304 responses**: ~100 bytes each

That is roughly 15 + 15 + 5 + 5 + 0.1 + 0.1 + overhead = ~42KB per page. The two plain variants share the same body content but differ in the Connection header, so they are about the same size. The two gzip variants similarly.

For a blog with 100 posts plus tag pages, archives, and series pages — maybe 150 pages total — that is about 6MB of pre-built HTTP responses. Plus the map overhead, the sidebar data, and the feed/sitemap strings. Call it 8-10MB total.

This is tiny. A single image is often larger than the entire pre-rendered site. And unlike an image, these bytes are the complete, ready-to-write response. The server does not touch them — it just writes pointers.

## The Sidebar Optimization

There is one more optimization worth mentioning. The sidebar (recent posts, tag cloud, series list) is the same on every page. Rather than rendering it separately for each page, it is computed once:

```cpp
component::SidebarData sidebar_data{engine.list_posts(), all_tags, all_series};

auto render_page = [&](loom::PageMeta meta, loom::dom::Node content) -> std::string {
    return ctx.page(meta, std::move(content), &sidebar_data).render();
};
```

The `sidebar_data` is shared across all `render_page` calls. The component system renders the sidebar once per page (because it needs to be in the HTML), but the data collection happens just once for the entire site.

## Summary

The rendering pipeline works like a compiler:

1. **Source** (markdown, frontmatter, config) is loaded from the content source
2. **Domain model** (Site, Posts, Tags, Series) is built by `build_site()`
3. **Components** render the domain model into a DOM tree
4. **Serialization** converts the DOM tree to an HTML string
5. **Minification** strips unnecessary whitespace
6. **Hashing** produces an ETag
7. **Compression** produces a gzip variant
8. **Wire serialization** produces complete HTTP response strings
9. **Caching** stores everything in an immutable `SiteCache`

Steps 1-9 happen once at startup (and once per hot reload). Steps 0 happen per request: look up the path, check ETag, select the variant, write the bytes. That is why responses are sub-millisecond — the "response" is just a pointer into pre-built data.
