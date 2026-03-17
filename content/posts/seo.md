---
title: SEO Out of the Box
date: 2025-01-15
slug: seo
tags: feature
excerpt: Canonical URLs, Open Graph, Twitter Cards, JSON-LD, RSS, sitemap — all generated automatically.
---

Every page Loom serves includes proper SEO metadata with zero configuration beyond `base_url` in `site.conf`.

## What's Generated

**Meta tags:** `<title>`, `description`, `author`, `canonical`, `robots` (noindex on 404).

**Open Graph:** `og:type` (website or article), `og:title`, `og:description`, `og:url`, `og:site_name`, `article:author`, `article:published_time`, `article:tag`.

**Twitter Cards:** `twitter:card`, `twitter:title`, `twitter:description`.

**JSON-LD:** `BlogPosting` schema on posts (headline, datePublished, author, url, publisher). `WebSite` schema on the homepage.

**RSS:** `/feed.xml` — RSS 2.0 with Atom self-link, latest 20 posts, per-post categories from tags.

**Sitemap:** `/sitemap.xml` — all posts, pages, tag pages, series pages, archives. Priority-weighted.

**Robots:** `/robots.txt` with sitemap reference.

## Per-Post Metadata

Post excerpts (from frontmatter or auto-generated) are used as `meta description` and OG/Twitter descriptions. Tags become `article:tag` properties. The publish date populates `article:published_time` and `datePublished` in JSON-LD.

## Caching Headers

Every response includes `ETag` and `Cache-Control: public, max-age=60, must-revalidate`. Conditional requests with `If-None-Match` get `304 Not Modified`.
