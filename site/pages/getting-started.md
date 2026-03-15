---
title: Getting Started
slug: getting-started
---

## Requirements

- Linux (epoll and inotify are Linux-only)
- g++ with C++20 support (GCC 10+)
- zlib (`-lz`)

## Build

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
```

The binary is `./loom`. Build takes under 3 seconds.

## Create Your Content

```bash
mkdir -p myblog/posts myblog/pages
```

Create `myblog/site.conf`:

```
title = My Blog
description = Thoughts on code
author = Your Name
base_url = https://yourdomain.com
theme = default

nav = Home:/, About:/about

sidebar_widgets = recent_posts, tag_cloud, about
sidebar_about = A few words about you.

footer_copyright = &copy; 2024 Your Name
```

Create your first post at `myblog/posts/hello.md`:

```
---
title: Hello World
date: 2024-01-01
slug: hello
tags: meta
excerpt: The first post.
---

This is my first post. **Bold**, *italic*, `code`, and [links](https://example.com) all work.

## A Heading

- List item one
- List item two

A code block:

\`\`\`cpp
int main() { return 0; }
\`\`\`
```

Create an about page at `myblog/pages/about.md`:

```
---
title: About
slug: about
---

This is the about page.
```

## Run

```bash
./loom myblog/
```

Open `http://localhost:8080`. Edit any file and the site rebuilds instantly.

## Create a Series

Make a subdirectory in `posts/`:

```bash
mkdir myblog/posts/my-series
```

Add posts inside it. The directory name becomes the series name. Posts are ordered by their `date` field.

## Pick a Theme

Change `theme` in `site.conf` to any of: `default`, `mono`, `serif`, `nord`, `rose`, `cobalt`, `earth`, `hacker`.

The site reloads automatically — no restart needed.

## Deploy

Loom is a single binary that serves HTTP. Run it behind nginx/caddy for TLS, or expose port 8080 directly.

```bash
# systemd service example
[Unit]
Description=Loom Blog
After=network.target

[Service]
ExecStart=/usr/local/bin/loom /var/www/myblog
Restart=always

[Install]
WantedBy=multi-user.target
```

## Serve from Git

```bash
./loom --git /path/to/repo.git main content/
```

Loom reads content directly from git objects. Push a commit and the site updates automatically.
