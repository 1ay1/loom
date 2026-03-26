---
title: Loom vs Hugo vs Jekyll
date: 2026-03-25
slug: loom-vs-hugo-vs-jekyll
tags: comparison, architecture
excerpt: An honest side-by-side of three blog engines — what each does well, where each falls short, and why they exist.
---

Three tools that turn markdown into a blog. Different languages, different architectures, different tradeoffs. Here's how they actually compare.

## The basics

| | Loom | Hugo | Jekyll |
|---|---|---|---|
| Language | C++20 | Go | Ruby |
| Architecture | Live server | Static site generator | Static site generator |
| Binary / install | ~856 KB binary | ~50–75 MB binary | Ruby runtime + ~30 gems |
| Runtime deps | zlib only | None (self-contained) | Ruby, Bundler, GCC for native gems |
| Build system | `make` (17-line Makefile) | Download binary or `go install` | `gem install jekyll bundler` |
| Source lines | ~6,400 | ~100,000+ | ~18,000+ (Ruby, plus gem deps) |
| GitHub stars | New project | ~80k+ | ~49k+ |

Hugo and Jekyll are mature, battle-tested tools with massive ecosystems. Loom is a new project with a different philosophy.

## Setup — time to first page

**Loom:**

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make          # needs g++ with C++20 and zlib
./loom content/
```

Four commands, one dependency (`zlib`), blog is live at `localhost:8080`. The build takes ~16 seconds from clean (compiling C++20), but you only do it once. No themes to install — 21 are built in.

**Hugo:**

```bash
# download binary from gohugo.io or use a package manager
hugo new site mysite
cd mysite
git init
git submodule add https://github.com/theNewDynamic/gohugo-theme-ananke themes/ananke
echo "theme = 'ananke'" >> hugo.toml
hugo new content posts/first-post.md
hugo server
```

More steps, but the binary is pre-built so there's nothing to compile. The theme ecosystem is huge — but you need to pick one before you can see anything. Hugo's install is painless.

**Jekyll:**

```bash
gem install bundler jekyll    # requires Ruby already installed
jekyll new mysite
cd mysite
bundle exec jekyll serve
```

Fewer commands, but `gem install` can be a minefield. Ruby version conflicts, native extension compilation failures, and platform-specific gem issues are common enough that Jekyll's own docs have a dedicated [troubleshooting page](https://jekyllrb.com/docs/troubleshooting/). Once it works, it works.

## Build and serve model

This is the fundamental architectural difference.

**Hugo and Jekyll** are static site generators. They read your markdown, produce a directory of HTML files, and stop. You then point a web server (nginx, Caddy, Netlify, GitHub Pages) at that output directory.

```
edit → build → output/ → web server → browser
```

**Loom** is the web server. It reads markdown, renders HTML, compresses it, and serves it over HTTP — all in one process. There is no output directory.

```
edit → loom detects change → re-renders in memory → serves directly
```

What this means in practice:

- Hugo/Jekyll have a build step. Loom doesn't.
- Hugo/Jekyll produce files you can host anywhere (S3, GitHub Pages, any CDN). Loom needs to run as a process.
- Loom's hot reload is instant — inotify triggers a re-render, no manual rebuild. Hugo's dev server also has live reload, and it's extremely fast. Jekyll's rebuild is noticeably slower.
- Hugo/Jekyll scale to any hosting platform. Loom needs a Linux server.

**This is a real tradeoff.** If you want to deploy to GitHub Pages or Netlify, use Hugo or Jekyll. If you want a self-hosted blog on a VPS with zero build pipeline, Loom is simpler.

## Performance

Apples-to-oranges warning: Hugo and Jekyll generate static files, so "performance" means different things. For Hugo/Jekyll, it's build time. For Loom, it's response time.

**Build speed (10–50 posts):**

| | Time |
|---|---|
| Hugo | < 100ms (often under 50ms) |
| Jekyll | 1–5 seconds |
| Loom | No build step — pre-renders at startup in < 50ms, re-renders on file change |

Hugo is phenomenally fast at generating static sites. It routinely handles 10,000+ pages in under 3 seconds. Jekyll is slower — known and documented.

Loom doesn't have a build step in the same sense. It renders everything to memory at startup and re-renders incrementally when files change. For a small blog, startup is near-instant.

**Serving speed:**

With Hugo/Jekyll, this depends entirely on your web server and CDN. A static file behind Cloudflare will be fast no matter what generated it.

Loom serves pre-compressed (gzip level 9) responses from an in-memory cache through a Linux epoll event loop. Response times are sub-millisecond. But — it's a single-process server. For a personal blog getting normal traffic, that's more than enough. For a high-traffic site, you'd want Hugo's static output behind a CDN.

## Dependencies

This is where the philosophies diverge most.

**Loom** links against libc, libstdc++, libpthread, and zlib. That's it. The HTTP server, markdown parser, router, template engine, config parser, HTML minifier, and gzip compression are all written from scratch. ~6,400 lines of C++.

**Hugo** is a single Go binary with no runtime dependencies. Internally it uses Go's standard library plus vendored dependencies (goldmark for markdown, various Go packages). The binary is self-contained — download and run.

**Jekyll** needs Ruby, Bundler, and pulls in ~25–35 gems for a default setup. Each gem may have its own dependencies. Native extensions need GCC. This dependency chain is Jekyll's biggest pain point — the "works on my machine" problem is real.

| | Runtime deps | Install friction |
|---|---|---|
| Loom | zlib | Need a C++20 compiler (build from source) |
| Hugo | None | Download one binary |
| Jekyll | Ruby ecosystem | Ruby version management, native gems |

Hugo has the smoothest install. Loom has the fewest runtime dependencies but requires compilation. Jekyll has the most friction.

## Features

Hugo and Jekyll have years of development and thousands of contributors. Loom does not compete on feature count.

| Feature | Loom | Hugo | Jekyll |
|---|---|---|---|
| Themes | 21 built-in | 400+ community themes | 200+ community themes |
| Plugins / extensions | None | Shortcodes, modules, pipes | Plugin system, Liquid filters |
| Multilingual | No | Yes, first-class | Via plugins |
| Taxonomy | Tags, series | Fully custom taxonomies | Categories, tags |
| Template language | None (compiled layouts) | Go templates | Liquid |
| RSS | Auto-generated | Configurable | Via plugin |
| Sitemap | Auto-generated | Auto-generated | Via plugin |
| Sass/SCSS | No | Yes (extended edition) | Yes |
| Data files (JSON/YAML/CSV) | No | Yes | Yes |
| Shortcodes | No | Yes | No (use Liquid includes) |
| Content from git | Yes (serve from branch directly) | No (needs checkout) | No |

If you need multilingual support, custom taxonomies, data-driven pages, or a deep plugin ecosystem — use Hugo. It's the clear winner on features.

If you want GitHub Pages integration with minimal setup — Jekyll has first-class support.

If you want a zero-config blog that's running in four commands with no build step, no output directory, and no template language to learn — that's what Loom is for.

## Binary size

| | Size |
|---|---|
| Loom | 856 KB (unstripped), 759 KB (stripped) |
| Hugo | ~50 MB (standard), ~75 MB (extended) |
| Jekyll | N/A (interpreted, but Ruby runtime + gems is 50–100 MB+) |

Loom is about 60x smaller than Hugo. This doesn't matter for most people, but it's a real difference on minimal VPS instances or embedded systems.

## Who should use what

**Use Hugo if:**
- You want the largest ecosystem and most features
- You need static output for CDN/GitHub Pages/Netlify
- You need multilingual support
- You want battle-tested software with a huge community

**Use Jekyll if:**
- You want native GitHub Pages integration
- You're comfortable with Ruby
- You like Liquid templates and the plugin ecosystem
- Your blog is small-to-medium and build speed isn't critical

**Use Loom if:**
- You want a self-hosted blog with zero build pipeline
- You like running a single binary on a VPS
- You don't want to learn a template language
- You value simplicity over features
- You want to serve content directly from a git repo without CI/CD

## What Loom doesn't do

Being honest about limitations:

- **No static output.** You can't generate a directory of HTML files and host them on a CDN. Loom is a live server.
- **No plugin system.** What's built in is what you get.
- **No Windows/macOS.** The HTTP server uses Linux-specific APIs (epoll, inotify). It runs on Linux only.
- **No template language.** You can't customize the HTML structure of pages beyond theme CSS and config options.
- **Young project.** Hugo and Jekyll have been around for 10+ years. Loom hasn't been battle-tested at scale.

These are deliberate constraints, not missing features. Loom is small on purpose.

## The bottom line

Hugo is the Swiss Army knife — it does everything, and it does it fast. Jekyll is the established default, especially for GitHub Pages. Loom is a scalpel — it does one thing (serve a markdown blog) with minimal moving parts.

If you need features, pick Hugo. If you need simplicity, try Loom.
