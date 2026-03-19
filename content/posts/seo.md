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

**Open Graph:** `og:type` (website or article), `og:title`, `og:description`, `og:url`, `og:site_name`, `og:image`, `article:author`, `article:published_time`, `article:tag`.

**Twitter Cards:** `twitter:card` (`summary_large_image` when an image is present, otherwise `summary`), `twitter:title`, `twitter:description`, `twitter:image`.

**JSON-LD:** `BlogPosting` schema on posts (headline, datePublished, author, url, publisher). `WebSite` schema on the homepage. `BreadcrumbList` schema on posts, tag, and series pages for search engine trail rendering.

**Breadcrumbs:** Visible breadcrumb nav rendered at the top of post, tag, and series pages (e.g. Home › Tags › performance). Toggle with `show_breadcrumbs = false` in `site.conf`.

**RSS:** `/feed.xml` — RSS 2.0 with Atom self-link, latest 20 posts, per-post categories from tags.

**Sitemap:** `/sitemap.xml` — all posts, pages, tag pages, series pages, archives. Priority-weighted.

**Robots:** `/robots.txt` with sitemap reference.

## Per-Post Metadata

Post excerpts (from frontmatter or auto-generated) are used as `meta description` and OG/Twitter descriptions. Tags become `article:tag` properties. The publish date populates `article:published_time` and `datePublished` in JSON-LD.

Set the social preview image via the `image` frontmatter field:

```markdown
---
title: My Post
image: /images/cover.png
---
```

If `image` is omitted, Loom falls back to the first image embedded in the post body. Relative paths are resolved against `base_url` automatically.

## Caching Headers

Every response includes `ETag` and `Cache-Control: public, max-age=60, must-revalidate`. Conditional requests with `If-None-Match` get `304 Not Modified`.
