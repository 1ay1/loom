---
title: Directory-Based Series and Archive Pages
date: 2025-02-01
slug: series-and-archives
tags: feature
excerpt: Series are now defined by directory structure, not frontmatter. Plus archive pages grouped by year.
---

## Series

Series in Loom are directory-based. Create a subdirectory inside `posts/` and every markdown file in it belongs to that series:

```
posts/
  linux-internals/       # series name: linux-internals
    virtual-memory.md
    cgroups.md
    io-uring.md
  standalone-post.md     # not in any series
```

No frontmatter needed — the folder name is the series name. Posts within a series are ordered by publish date, oldest first.

Each post in a series gets an ordered navigation panel:

1. Virtual Memory (current)
2. Cgroups and Namespaces
3. io_uring

The current post is highlighted. Other posts are links.

## Archive Pages

`/archives` shows all posts grouped by year in reverse chronological order. Each year section lists its posts sorted by date.

`/series` lists all series. `/series/:name` shows all posts in a specific series.

All three page types are also available as sidebar widgets via `sidebar_widgets = archives, series` in `site.conf`.

## Routes

| Route | Content |
|-------|---------|
| `/archives` | All posts by year |
| `/series` | All series |
| `/series/:name` | Posts in a series |
