---
title: Introducing Loom
date: 2024-01-01
slug: introducing-loom
tags: release
excerpt: A zero-dependency C++20 blog engine. Single binary, sub-millisecond responses, hot reload.
---

Loom is a blog engine written in C++20 with zero external dependencies. You give it a directory of markdown files, and it serves a complete blog with RSS, sitemap, dark mode, and SEO — all from a single binary.

## Why

Static site generators are great until you need to rebuild 500 pages because you fixed a typo. CMSes are great until you need to manage a database, a cache layer, and 47 npm packages.

Loom takes a different approach: it reads markdown, renders HTML, and serves it from memory. No build step. No database. No node_modules. Edit a file, and the site updates instantly.

## How It Works

At startup, Loom reads your content directory, parses all markdown and configuration, renders every page to HTML, compresses it with gzip, and stores everything in an in-memory cache. When a request comes in, epoll dispatches it to the router, which returns the pre-rendered, pre-compressed response. No disk I/O, no template rendering, no allocations.

When a file changes, inotify triggers a rebuild. The new cache is built in the background and swapped atomically via `shared_ptr`. Active requests finish on the old cache; new requests get the new one. Zero downtime.

## What You Get

- 8 built-in themes with dark/light mode
- RSS feed, XML sitemap, robots.txt
- Open Graph, Twitter Cards, JSON-LD structured data
- Tag pages, archive pages, series navigation
- Related posts by tag overlap
- Prev/next post navigation
- Sidebar with configurable widgets
- Gzip compression with ETag caching
- Hot reload via inotify or git polling
- Sub-millisecond response times

## Get Started

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
./loom content/
```

Your blog is at `http://localhost:8080`.
