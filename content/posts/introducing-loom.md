---
title: Introducing Loom
date: 2024-01-01
slug: introducing-loom
tags: release, architecture
excerpt: A zero-dependency C++20 blog engine. Single binary, sub-millisecond responses, hot reload, and no build step.
---

Loom is a blog engine written in C++20 with zero external dependencies beyond zlib. You give it a directory of markdown files and a `site.conf`, and it serves a complete, fast blog — with RSS, sitemap, Open Graph, dark mode, tag pages, series navigation, and gzip compression — from a single binary under 1MB.

## The Architecture in One Paragraph

At startup, Loom reads your content directory, parses every markdown file and the configuration, renders each page to HTML, minifies it, compresses it with gzip, and stores everything in an in-memory hash map. Requests are hash table lookups — no template rendering, no disk I/O, no allocations on the hot path. When a file changes, inotify detects it and a background thread builds a new cache, which is swapped atomically via `shared_ptr`. Active requests finish on the old cache; new requests get the new one. Zero downtime, no restart.

## Why Build This

**No build step.** Static site generators (Hugo, Jekyll, Eleventy) require a separate build phase. Edit a file, run a command, wait, see the result. Loom's hot reload makes editing feel like a live document editor — save, refresh, done.

**No database.** CMSes (WordPress, Ghost) need a database, cache layer, and plugin ecosystem to stay fast. Loom's "database" is an in-memory hash map of pre-rendered strings.

**No dependencies.** One `make` command, no package manager, no virtualenv, no `node_modules`. The binary links only against libc, libstdc++, libpthread, and zlib.

**Real performance.** An epoll-based single-threaded server handling pre-compressed cached responses has essentially zero overhead per request. Response times are in the sub-millisecond range even under load.

## What Loom Provides

**Content:**
- Markdown to HTML with a hand-written parser (no regex engine, no library)
- Post frontmatter: title, date, slug, tags, excerpt, image, draft
- Series via directory structure (`posts/series-name/post.md`)
- Tag pages, tag index, archive by year
- Static pages (`pages/about.md` → `/about`)

**Navigation and discovery:**
- Configurable header navigation (`nav = Home:/, Tags:/tags, ...`)
- Prev/next post navigation on every post
- Related posts by tag overlap (top 3 by tag count)
- Series navigation panel on posts that are part of a series
- Sidebar with widgets: recent posts, tag cloud, series list, about, archives

**SEO:**
- `<title>`, `<meta name="description">`, `<link rel="canonical">`
- Open Graph: `og:type`, `og:title`, `og:description`, `og:url`, `og:image`, `og:site_name`
- Article meta: `article:author`, `article:published_time`, `article:tag`
- Twitter Cards: `twitter:card` (`summary_large_image` when image present)
- JSON-LD: `BlogPosting` schema on posts, `WebSite` on homepage, `BreadcrumbList` on tag/series/post pages
- `<link rel="preload" as="image">` for hero images on article pages
- RSS 2.0 feed at `/feed.xml` (latest 20 posts)
- XML sitemap at `/sitemap.xml` (all pages, priority-weighted)
- `/robots.txt` with sitemap reference

**Performance:**
- All pages pre-rendered and stored in memory
- Gzip compression at level 9, computed once at cache build time
- ETag-based `304 Not Modified` responses
- `Cache-Control: public, max-age=60, must-revalidate`
- HTML minification before compression
- `TCP_NODELAY` for low-latency response delivery
- Image `width` and `height` attributes auto-detected from local PNG/JPEG files

**Theming:**
- 8 built-in themes: `default`, `mono`, `serif`, `nord`, `rose`, `cobalt`, `earth`, `hacker`
- Dark/light mode toggle with `localStorage` persistence and `prefers-color-scheme` detection
- CSS custom properties for fine-grained overrides (`theme_accent`, `theme_font`, etc.)
- Full CSS override via `content/theme/style.css`

**Deployment:**
- Filesystem mode: `./loom content/` — serves from disk, inotify hot reload
- Git mode: `./loom --git /path/to/repo branch prefix` — reads from git objects, no checkout needed
- Remote git: `./loom --git https://github.com/you/blog.git` — auto-clones to `/tmp`, image requests redirect to GitHub CDN

## Loom as a Docs Platform

Loom isn't just for blogs. This site — Loom's own documentation — is served by Loom itself. The `internals/` series covers the HTTP server, router, pre-rendering pipeline, and strong types system. The `cpp/` series covers the C++20 features and Linux APIs used in the engine. Reference documentation lives in `pages/`.

See [Using Loom as a Project Documentation Platform](/post/loom-as-docs-platform) for the full breakdown.

## Get Started

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
./loom content/
```

Visit `http://localhost:8080`. The example site in `content/` is this documentation site.

Copy `content/` as a starting point for your own site, or read the [Getting Started](/getting-started) guide to build one from scratch.

## The Stack

| Layer | Technology |
|---|---|
| HTTP server | Linux epoll, edge-triggered, single-threaded |
| Router | Hand-written trie, O(path segments) matching |
| Cache | `std::unordered_map` + `shared_ptr` atomic swap |
| Markdown | Hand-written two-pass parser, ~1200 lines |
| Compression | zlib deflate, level 9, pre-computed |
| Content | Filesystem or git objects (local or remote) |
| Hot reload | Linux inotify or git poll |
| Language | C++20 — concepts, structured bindings, ranges |
| Dependencies | zlib only (libc/libstdc++ are standard) |
| Binary size | ~846KB stripped |

No Boost. No ASIO. No external HTTP parser. No templating engine. No JSON library. Just C++20 and one compression library.
