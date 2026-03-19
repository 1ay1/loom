---
title: Documentation
slug: docs
---

## Content Structure

```
content/
  site.conf              # Site configuration
  posts/                 # Blog posts (markdown)
    my-post.md
    series-name/         # Subdirectory = series
      first-post.md
      second-post.md
  pages/                 # Static pages
    about.md
    projects.md
  images/                # Static assets (served as-is)
    cover.png
  theme/
    style.css            # Custom CSS override (optional)
```

Any non-markdown file in the content directory is served as a static asset. Put images, fonts, or downloads anywhere and reference them by path (e.g. `![photo](/images/cover.png)`).

## Post Frontmatter

```
---
title: My Post Title
date: 2024-01-15
slug: my-post
tags: systems, cpp, linux
excerpt: A one-line summary for listings and SEO.
image: /images/cover.png
draft: true
---
```

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `title` | no | filename | Post title |
| `date` | no | current time | Publish date (`YYYY-MM-DD`) |
| `slug` | no | filename | URL path (`/post/slug`) |
| `tags` | no | none | Comma-separated tag list |
| `excerpt` | no | auto-generated | Summary for listings and meta tags |
| `image` | no | first image in post | Social preview image (`og:image`, `twitter:image`) |
| `draft` | no | false | Set `true` to hide from all listings |

If `excerpt` is omitted, the first ~200 characters of content are used.

Reading time is auto-calculated at 200 words per minute.

File modification time is used as a tiebreaker when posts share the same publish date.

## Page Frontmatter

```
---
title: About
slug: about
---
```

Pages are served at `/:slug` (e.g., `/about`).

## Series

Create a subdirectory inside `posts/`. The folder name becomes the series name. Posts are ordered by publish date, oldest first.

```
posts/
  linux-internals/
    virtual-memory.md     # Part 1 (earliest date)
    cgroups.md            # Part 2
    io-uring.md           # Part 3
```

Each post in a series displays a navigation panel listing all parts, with the current post highlighted.

Series appear in the sidebar widget and at `/series` and `/series/:name`.

## Site Configuration

`site.conf` uses `key = value` format.

### Core

| Key | Description |
|-----|-------------|
| `title` | Site title (shown in header and `<title>`) |
| `description` | Site description |
| `author` | Author name (used in meta tags, RSS, JSON-LD) |
| `base_url` | Full base URL for canonical links, sitemap, RSS |
| `theme` | Theme name: `default`, `mono`, `serif`, `nord`, `rose`, `cobalt`, `earth`, `hacker` |

### Navigation

```
nav = Home:/, Blog:/, About:/about, GitHub:https://github.com
```

Comma-separated `Label:url` pairs. External URLs work.

### Sidebar

| Key | Description |
|-----|-------------|
| `sidebar_widgets` | Comma-separated widget list |
| `sidebar_recent_count` | Number of recent posts to show (default: 5) |
| `sidebar_about` | Text for the about widget |

Available widgets: `recent_posts`, `tag_cloud`, `archives`, `series`, `about`.

### Layout

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `header_style` | `default`, `centered`, `minimal` | `default` | Header layout |
| `show_description` | `true`, `false` | `false` | Show site description below title |
| `show_theme_toggle` | `true`, `false` | `true` | Dark/light mode toggle button |
| `post_list_style` | `list`, `cards` | `list` | Index page layout |
| `show_post_dates` | `true`, `false` | `true` | Publication dates on listings |
| `show_post_tags` | `true`, `false` | `true` | Tags on listings |
| `show_excerpts` | `true`, `false` | `true` | Excerpts on listings |
| `show_reading_time` | `true`, `false` | `true` | Reading time estimates |
| `sidebar_position` | `right`, `left`, `none` | `right` | Sidebar placement |
| `date_format` | strftime format | `%Y-%m-%d` | Date display format |
| `custom_css` | CSS string | none | Additional CSS injected after theme |
| `custom_head_html` | HTML string | none | HTML injected in `<head>` (fonts, analytics) |

### Footer

```
footer_copyright = &copy; 2024 Your Name
footer_links = GitHub:https://github.com, Twitter:https://twitter.com
```

### Theme Variable Overrides

Override any CSS variable without writing a full theme:

```
theme = nord
theme_accent = #ff6600
theme_dark-accent = #ff8833
theme_font_size = 18px
theme_max_width = 800px
```

Underscores in key names become hyphens in CSS. Keys prefixed with `dark-` apply only to dark mode.

Available variables: `bg`, `text`, `muted`, `border`, `accent`, `font`, `font-size`, `max-width`, and any custom CSS variable defined in the theme.

## Markdown

Loom's hand-written markdown parser supports:

**Block elements:** headings (ATX `#` and setext), paragraphs, fenced code blocks (backticks and tildes with language hints), blockquotes, unordered lists (`-`, `*`, `+`), ordered lists, task lists (`- [ ]`, `- [x]`), nested lists, tables with alignment, horizontal rules, footnote definitions, raw HTML blocks.

**Inline elements:** bold, italic, bold italic, strikethrough, inline code, links, reference links, images, reference images, autolinks, footnote references, backslash escapes, line breaks (trailing spaces).

**Tables:**

```
| Left | Center | Right |
|:-----|:------:|------:|
| a    |   b    |     c |
```

**Footnotes:**

```
Something[^1] interesting.

[^1]: The footnote text.
```

## Routes

| Route | Description |
|-------|-------------|
| `/` | Post index |
| `/post/:slug` | Single post (with prev/next, related posts, series nav) |
| `/tag/:tag` | Posts filtered by tag |
| `/tags` | All tags |
| `/archives` | Posts grouped by year |
| `/series` | All series |
| `/series/:name` | Posts in a series |
| `/:slug` | Static page |
| `/feed.xml` | RSS 2.0 feed (latest 20 posts) |
| `/sitemap.xml` | XML sitemap |
| `/robots.txt` | Robots exclusion |
| `/*` | Static assets (images, fonts, etc.) |

## Git Source

Serve content directly from a git repository without a working tree:

```bash
./loom --git /path/to/repo main content/
```

Arguments: `repo_path`, `branch` (default: `main`), `content_prefix` (default: root).

Content is read via `git show` and `git ls-tree`. The git watcher polls for new commits and hot-reloads automatically.

## Hot Reload

**Filesystem mode:** inotify watches the content directory for file creates, modifies, deletes, and moves. Changes are debounced (500ms) then the cache is rebuilt.

**Git mode:** polls `git rev-parse HEAD` for commit hash changes. New commit triggers a full rebuild.

In both modes, the new cache is built in the background and swapped atomically via `shared_ptr`. Active requests finish on the old cache. Zero downtime.

## HTTP Server

- Epoll-based non-blocking event loop
- `TCP_NODELAY` enabled
- Keep-alive connections (5s idle timeout)
- Gzip compression (auto-detected from `Accept-Encoding`)
- ETag caching with `304 Not Modified` responses
- `Cache-Control: public, max-age=60, must-revalidate`
- HTML minification
- 1MB max request size
- SEO: canonical URLs, Open Graph, Twitter Cards, JSON-LD structured data, RSS autodiscovery

## Custom Themes

Drop a `style.css` in `content/theme/` to fully override the default stylesheet. Or use a built-in theme and override specific variables via `theme_*` keys in `site.conf`.

## Running

```bash
# Filesystem source (default)
./loom                        # uses content/
./loom /path/to/content       # custom content dir

# Git source
./loom --git /path/to/repo              # main branch, root prefix
./loom --git /path/to/repo develop      # custom branch
./loom --git /path/to/repo main site/   # custom content prefix
```

Port 8080, hardcoded.
