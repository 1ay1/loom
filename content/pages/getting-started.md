---
title: Getting Started
slug: getting-started
---

## 30 Seconds to a Live Blog

```bash
git clone https://github.com/1ay1/loom.git
cd loom
make
./loom content/
```

Open `http://localhost:8080`. You're running a blog. That's it.

Edit any file in `content/` and refresh — changes show up instantly. No build step, no restart.

---

## Make It Yours

Copy the example site as a starting point:

```bash
cp -r content/ myblog/
```

Open `myblog/site.conf` and change the basics:

```
title = My Blog
description = What I'm working on
author = Your Name
base_url = https://yourdomain.com
theme = nord
```

Pick any theme: `default`, `terminal`, `nord`, `gruvbox`, `rose`, `hacker`. See them all on the [Themes](/themes) page.

Run your blog:

```bash
./loom myblog/
```

---

## Write Your First Post

Create `myblog/posts/hello.md`:

```
---
title: Hello World
date: 2025-10-01
tags: meta
excerpt: The first post.
---

This is my first post. **Bold**, *italic*, `code`, and [links](https://example.com) all work.

## A Heading

- List item one
- List item two
```

Save it. Refresh the browser. It's live.

## Add a Page

Pages are for content that isn't time-ordered — an about page, a projects page, a contact page.

Create `myblog/pages/about.md`:

```
---
title: About
slug: about
---

This is the about page.
```

Add it to your navigation in `site.conf`:

```
nav = Home:/, About:/about
```

## Add Images

Put images anywhere in your content directory:

```bash
mkdir myblog/images
cp photo.png myblog/images/
```

Use them in markdown: `![alt text](/images/photo.png)`. Any file that isn't markdown or `site.conf` is served as a static asset.

## Create a Series

Group related posts by putting them in a subdirectory:

```bash
mkdir myblog/posts/my-series
```

Add posts inside it. The directory name becomes the series name. Each post gets a navigation panel linking to all parts.

## Deploy

Loom is the server. Run it behind nginx or caddy for TLS, or expose port 8080 directly.

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

Or serve straight from a git repo — push to publish:

```bash
./loom --git /path/to/repo.git main content/
```

## Requirements

- Linux (uses epoll and inotify)
- g++ with C++20 support (GCC 10+)
- zlib (usually pre-installed)

Build takes under 3 seconds.
