---
title: Using Loom as a Project Documentation Platform
date: 2024-03-20
slug: loom-as-docs-platform
tags: feature, architecture
excerpt: Loom isn't just for blogs. Its series, tags, pages, and navigation make it a capable project documentation platform — and it's already serving its own docs.
---

This site is Loom's own documentation, served by Loom itself. Every page you're reading — the configuration reference, the internals deep-dives, the C++ guides — is a markdown file in a `content/` directory, rendered by the same engine that would power your project's docs.

Here's how to structure a project documentation site with Loom, and what makes it work well for technical content.

## Documentation Needs vs Blog Needs

A blog and a documentation site have different shapes:

| Concern | Blog | Docs |
|---|---|---|
| Entry point | Chronological feed | Structured index/overview |
| Navigation | Tags, archives | Sections, topics |
| Content organisation | Time-ordered | Hierarchy + cross-refs |
| Updates | New posts | Edits to existing pages |
| Pages vs posts | Mostly posts | Both — reference + guides |

Loom covers both. The key is knowing which primitives to reach for.

## Pages for Reference Documentation

Loom's `pages/` directory is for content that isn't time-ordered — things that get updated in-place rather than replaced with a new entry. Reference documentation is exactly this.

```
content/
  pages/
    docs.md           ← full reference (routes, config, markdown)
    getting-started.md
    themes.md
    api.md            ← your project's API reference
    changelog.md      ← updated in place, not a new post per version
```

Pages appear at `/:slug`. Navigation links can point directly to them:

```ini
nav = Home:/, Docs:/docs, API:/api, Getting Started:/getting-started
```

For a library or tool, `docs.md` can be a comprehensive single-page reference, while `getting-started.md` is the onboarding path.

## Series for Sequential Guides

If your docs have learning sequences — "install, then configure, then deploy" or "beginner, intermediate, advanced" — series are the right structure:

```
content/posts/
  getting-started/
    01-installation.md
    02-configuration.md
    03-your-first-post.md
    04-deployment.md
  internals/
    http-server.md
    router.md
    cache-and-rendering.md
    strong-types.md
  cpp/
    epoll-event-loop.md
    cpp20-concepts.md
    atomic-cache-pattern.md
```

Each subdirectory of `posts/` becomes a series. Posts are ordered by publish date. Series pages are auto-generated at `/series/:name` and listed in the sidebar's series widget.

Each post in a series shows a navigation box listing all parts:

```
Part of series: internals
1. The HTTP Server
2. The Trie Router          ← you are here
3. Pre-Rendering and Cache
4. Strong Types
```

This is the Loom docs site's exact structure. You're probably reading this inside a series navigation box right now.

## Tags for Cross-Referencing

Tags cut across series and stand-alone posts. Use them to group by topic rather than by sequence:

```
tags: internals, architecture, cpp
```

A post about epoll belongs in both the `cpp` series (sequential learning path) and has tags `linux`, `internals`, `cpp` (for topic-based navigation). `/tag/cpp` links to all C++ content regardless of which series it's in.

Good tag taxonomy for technical docs:
- Technology: `cpp`, `linux`, `http`, `markdown`
- Role: `internals`, `architecture`, `feature`, `reference`
- Audience: `beginner`, `advanced` (if needed)

Avoid having a different tag for every concept — too many tags dilute navigation. Aim for tags you'd actually use to filter (`show me everything about the HTTP layer`).

## Navigation as Information Architecture

The `nav` setting is your top-level information architecture. For a docs-oriented site:

```ini
nav = Home:/, Docs:/docs, Getting Started:/getting-started, Internals:/series/internals, C++ Deep Dive:/series/cpp, Source:https://github.com/you/project
```

This creates a navigation bar that reflects the site's structure: entry points (Home, Docs), learning paths (Getting Started, series), and the source. External links like GitHub open in a new tab with `external_links_new_tab = true`.

The sidebar `series` widget also links to series, so power users can navigate directly. But the top nav sets the first impression and the main paths.

## This Site as an Example

Here's exactly how Loom's own docs are structured:

**`site.conf`:**
```ini
nav = Home:/, Docs:/docs, Getting Started:/getting-started, Internals:/series/internals, C++ Deep Dive:/series/cpp, Source:https://github.com/1ay1/loom
sidebar_widgets = about, recent_posts, tag_cloud, series, archives
external_links_new_tab = true
show_breadcrumbs = true
```

**Pages** (reference, updated in-place):
- `/docs` — full configuration and routes reference
- `/getting-started` — quickstart guide
- `/themes` — theme reference

**Series** (sequential learning):
- `internals/` — HTTP server, router, cache, strong types, hot reload, static assets, markdown parser
- `cpp/` — epoll, C++20 concepts, atomic cache pattern

**Stand-alone posts** (announcements, essays):
- Introducing Loom
- SEO Out of the Box
- Using Loom as a Docs Platform (this post)
- Git Source

## Recommended Setup for a Project Docs Site

```ini
# site.conf
title = MyProject
description = Fast, reliable widgets for C++ developers
author = Your Name
base_url = https://docs.myproject.io

# Docs-oriented nav
nav = Home:/, Reference:/docs, Quick Start:/getting-started, Examples:/series/examples, GitHub:https://github.com/you/project

# Layout: no chrono pressure, breadcrumbs help orientation
show_breadcrumbs = true
show_post_dates = false      # dates matter less for reference docs
show_reading_time = true
external_links_new_tab = true
header_style = centered

# Sidebar for navigation
sidebar_widgets = recent_posts, tag_cloud, series
sidebar_position = right
```

Set `show_post_dates = false` if your docs aren't time-ordered — nothing feels more irrelevant than "this reference page was last published 2 years ago." If you do want dates, set `date_format = %B %Y` to show month/year without the specific day.

## Markdown Features Useful for Docs

Loom's markdown parser covers everything you need for technical writing:

**Fenced code blocks with language hints:**
````
```cpp
int main() { return 0; }
```
````

**Tables:**
```markdown
| Key | Default | Description |
|-----|---------|-------------|
| `base_url` | — | Full URL for canonical links |
```

**Footnotes for asides:**
```markdown
Loom uses epoll[^1] for I/O multiplexing.

[^1]: Linux-only. macOS has kqueue; Windows has IOCP. Loom doesn't support those.
```

**New-tab links for external references:**
```markdown
See the [epoll man page](https://man7.org/linux/man-pages/man7/epoll.7.html)^
for the full API reference.
```

**Task lists for changelogs or roadmaps:**
```markdown
## v2.0 Roadmap
- [x] HTTP/1.1 keep-alive
- [x] Gzip compression
- [ ] HTTP/2
- [ ] WebSocket support
```

## Hot Reload Is Essential for Docs

The most impactful feature for documentation work is hot reload. Edit a file, save, refresh — the change is live. No build step, no `make docs`, no waiting.

For long-form technical writing, this is the difference between a tight feedback loop and constant context switching. Write a section, save, see it rendered in the browser, adjust. The 500ms rebuild-and-swap is fast enough to be invisible.

In git source mode, push a commit and the docs site updates within seconds — useful for team docs where writers don't run Loom locally.
