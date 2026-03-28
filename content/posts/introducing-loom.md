---
title: Introducing Loom
date: 2025-09-15
slug: introducing-loom
tags: release, architecture
excerpt: A blog engine that just works. Clone, make, run — your blog is live in 30 seconds with themes, RSS, SEO, and hot reload built in.
---

## Install and Run

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
./loom content/
```

Open `http://localhost:8080`. You're running a blog. The whole setup is four commands.

No Node.js. No Ruby. No Python. No Docker. No config files to debug. Just a C++ compiler and `make`.

## What You Get

Out of the box, with zero configuration:

- **Hot reload** — edit a file, refresh the browser, see the change. No build step, no restart.
- **5 built-in themes** — switch with one line in `site.conf`. Dark mode included.
- **Full SEO** — Open Graph, Twitter Cards, JSON-LD, sitemap, RSS feed. All automatic.
- **Sub-millisecond responses** — every page is pre-rendered, compressed, and cached in memory.
- **Series, tags, archives** — organize content however you want.
- **Single binary, ~846KB** — no runtime dependencies, no frameworks, no `node_modules`.

## How It Works

You give Loom a directory of markdown files and a `site.conf`. It reads everything at startup, renders every page to HTML, compresses it, and holds it all in memory. Serving a page is a hash table lookup — no disk I/O, no template rendering.

When you edit a file, inotify detects the change and Loom rebuilds the cache in the background. The new cache is swapped in atomically — active requests finish on the old cache, new requests get the new one. Zero downtime.

## Your Content Directory

```
myblog/
  site.conf           # Title, theme, nav, sidebar
  posts/
    hello.md          # Blog posts
    my-series/        # Subdirectory = series
      part-one.md
      part-two.md
  pages/
    about.md          # Static pages (served at /about)
  images/
    photo.png         # Static assets (served as-is)
```

That's the whole structure. No layouts, no partials, no frontmatter templates.

## Minimal Configuration

```
title = My Blog
description = What I'm working on
author = Your Name
base_url = https://yourdomain.com
theme = nord

nav = Home:/, About:/about
sidebar_widgets = recent_posts, tag_cloud
```

That's a complete `site.conf`. Everything else has sensible defaults.

## Why Not Hugo / Jekyll / Ghost?

**No build step.** Static site generators require a separate build phase. Edit, build, wait, check. Loom's hot reload makes it: edit, refresh, done.

**No database.** CMSes need a database, cache layer, and plugins to stay fast. Loom's "database" is an in-memory hash map.

**No dependencies.** One `make` command. No package manager, no virtualenv, no `node_modules`. The binary links against libc, libstdc++, libpthread, and zlib — that's it.

**Real performance.** Pre-compressed cached responses served through an epoll event loop. Sub-millisecond response times even under load.

## Deploy

Loom is the server. Put it behind nginx/caddy for TLS, or run it directly:

```bash
./loom /var/www/myblog
```

Or serve straight from git — push to publish, no CI needed:

```bash
./loom --git /path/to/repo.git main content/
```

## Get Started

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
./loom content/
```

Read the [Getting Started](/getting-started) guide to build your own site, or explore the [full documentation](/docs) for everything Loom can do.

## The Stack

| Layer | Technology |
|---|---|
| HTTP server | Linux epoll, edge-triggered, single-threaded |
| Router | Hand-written trie, O(path segments) matching |
| Cache | `std::unordered_map` + `shared_ptr` atomic swap |
| Markdown | Hand-written two-pass parser, ~1200 lines |
| Compression | zlib deflate, level 9, pre-computed |
| Hot reload | Linux inotify or git poll |
| Language | C++20 — concepts, structured bindings, ranges |
| Dependencies | zlib only |
| Binary size | ~846KB stripped |
