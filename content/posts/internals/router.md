---
title: The Trie Router — O(segments) Path Matching
date: 2025-10-06
slug: router
tags: internals, architecture, cpp
excerpt: Loom uses a trie to match URL paths. Each segment is one node traversal. Parameters like :slug are first-class citizens, not regex afterthoughts.
---

URL routing sounds simple until it isn't. You need literal matches (`/tags`), parameter captures (`/post/:slug`), and a sane fallback for everything else. Regex works but is hard to extend. Linear scan works but is O(routes). Loom uses a trie — a tree of path segments where matching is O(number of segments in the URL).

## The Trie Structure

```cpp
struct TrieNode {
    std::unordered_map<std::string, std::unique_ptr<TrieNode>> children; // literal segments
    std::unique_ptr<TrieNode> param_child;  // :param wildcard
    std::string param_name;                 // name of the captured parameter
    std::unordered_map<HttpMethod, RouteHandler> handlers; // GET, POST, etc.
};
```

The root node represents `/`. Each level represents one path segment. A node can have both literal children (exact matches) and a single parameter child (wildcard).

For Loom's routes, the trie looks like this:

```
root /
├── "post" → node
│   └── :slug (param_child, param_name="slug")
│       └── [GET handler]
├── "tag" → node
│   └── :slug (param_child, param_name="slug")
│       └── [GET handler]
├── "tags" → node [GET handler]
├── "series" → node [GET handler]
│   └── :name (param_child)
│       └── [GET handler]
├── "archives" → node [GET handler]
├── "feed.xml" → node [GET handler]
├── "sitemap.xml" → node [GET handler]
└── "robots.txt" → node [GET handler]
```

The root node itself has a GET handler for `/`.

## Registering Routes

```cpp
void add_route(HttpMethod method, const std::string& pattern, RouteHandler handler) {
    auto segments = split_path(pattern); // "/post/:slug" → ["post", ":slug"]
    TrieNode* node = &root_;

    for (const auto& seg : segments) {
        if (seg.starts_with(':')) {           // C++20 starts_with
            if (!node->param_child)
                node->param_child = std::make_unique<TrieNode>();
            node->param_child->param_name = seg.substr(1); // strip ':'
            node = node->param_child.get();
        } else {
            if (!node->children.count(seg))
                node->children[seg] = std::make_unique<TrieNode>();
            node = node->children.at(seg).get();
        }
    }

    node->handlers[method] = std::move(handler);
}
```

`split_path` splits on `/` and strips empty segments from leading/trailing slashes. `"/post/:slug"` → `["post", ":slug"]`. The `:` prefix indicates a parameter.

`std::make_unique<TrieNode>()` allocates the node on the heap. The parent owns it via `unique_ptr` — no manual memory management, no leaks.

## Matching a Request

```cpp
Response route(Request& req) {
    auto segments = split_path(req.path);
    TrieNode* node = &root_;
    std::vector<std::string> params; // captured parameter values

    for (const auto& seg : segments) {
        if (node->children.count(seg)) {
            node = node->children.at(seg).get(); // literal match: prefer this
        } else if (node->param_child) {
            params.push_back(seg);               // parameter match: capture value
            node = node->param_child.get();
        } else {
            return fallback_(req);               // no match at this level
        }
    }

    // Found a node — check for the HTTP method
    if (node->handlers.count(req.method)) {
        req.params = std::move(params);
        try {
            return node->handlers.at(req.method)(req);
        } catch (...) {
            return Response::internal_error();
        }
    }

    // Node exists but not this method
    if (!node->handlers.empty())
        return Response::method_not_allowed();

    return fallback_(req);
}
```

**Literal before parameter:** When both a literal child and a parameter child exist, the literal wins. A request for `/tags` matches the literal `tags` node, not the `:slug` parameter of some other route.

**Parameter capture:** When only the parameter child matches, the segment value goes into `params`. Multiple parameters across multiple levels all accumulate into the same vector, in order.

**Fallback:** Any path that falls off the trie goes to the fallback handler. For Loom, this is the static file server — images, fonts, PDFs, anything that isn't a named route.

## Parameter Access

In a route handler:

```cpp
router.add_route(GET, "/post/:slug", [&](const Request& req) {
    const std::string& slug = req.params[0]; // first captured param
    auto page = cache->pages.find("/post/" + slug);
    if (page == cache->pages.end())
        return Response::not_found(cache->not_found);
    return serve_cached(req, page->second);
});
```

Parameters are positional. For `/series/:name`, `req.params[0]` is the series name. For a hypothetical `/post/:year/:slug`, `params[0]` is the year and `params[1]` is the slug.

## Why a Trie Instead of a Map

A simple map `{"/post/:slug" → handler}` works at first. But matching against parameterised patterns means iterating all routes and testing each regex/pattern. With N routes and M segments, that's O(N×M).

The trie is O(M) — one hash lookup per segment in the URL, regardless of how many routes exist. For Loom's ~15 routes, the difference is negligible. But the trie also handles conflicting patterns naturally: `/tags` (literal) and `/tag/:slug` (parameter) don't interfere because they branch at the second segment.

## Method Not Allowed vs Not Found

There's a semantic difference between "no route matches this path" (404) and "a route matches but not with this method" (405). The trie handles both:

- Path doesn't reach a node with handlers → `fallback_` (potentially 404)
- Path reaches a node, node has handlers, but not for this method → 405

This matters for APIs. If a client sends `POST /tags`, they should get 405, not 404. They found the right endpoint; they used the wrong verb.

## The Fallback Handler

```cpp
router.set_fallback([&](const Request& req) -> Response {
    if (!is_safe_path(req.path))
        return Response::bad_request();

    // Try the pre-rendered cache first
    auto it = cache->pages.find(req.path);
    if (it != cache->pages.end())
        return serve_cached(req, it->second);

    // Serve static file (disk, git blob, or GitHub redirect)
    return serve_static_asset(req, source, config);
});
```

The fallback always checks the HTML cache first. This handles pages that don't have explicit routes but are stored by exact path (though in practice Loom registers all routes explicitly). Static assets fall through to the file-serving logic.

## Trie and Memory

The trie is built once at startup and never modified. All nodes are owned by their parents via `unique_ptr`. The root node's destructor recursively frees the tree. There's no need for a custom allocator or any explicit cleanup.

`std::unordered_map` for children means O(1) average lookup per segment. For Loom's small trie, the constant factors dominate, but for a larger routing table, the O(1) per-level guarantee matters.
