---
title: Git Source — Serve a Blog Without a Checkout
date: 2025-03-01
slug: git-source
tags: feature, git, architecture
excerpt: Loom can read content directly from git objects. No working tree, no checkout, just git show and git ls-tree.
---

Loom now supports serving content directly from a git repository. No working directory needed — it reads blobs via `git show` and lists files via `git ls-tree`.

## Usage

```bash
./loom --git /path/to/repo main content/
```

Three arguments:

1. **repo path** — bare or non-bare repository
2. **branch** — branch name to serve (default: `main`)
3. **content prefix** — subdirectory within the repo (default: root)

## How It Works

`GitSource` implements the same `ContentSource` concept as `FileSystemSource`. Under the hood:

- `git ls-tree -r <branch>:<prefix>/posts` lists all post files
- `git ls-tree -d <branch>:<prefix>/posts` lists subdirectories (series)
- `git show <branch>:<path>` reads individual file contents
- `git log` provides file timestamps for publish dates and modification times

The git watcher polls `git rev-parse HEAD` every 100ms. When the commit hash changes, it triggers a full cache rebuild with the same atomic swap as filesystem mode.

## Why

Deploy Loom on a server, point it at a bare repo, and push to publish. No build pipeline, no CI, no deploy scripts. `git push` and the site updates within seconds.

Works with post-receive hooks too:

```bash
#!/bin/sh
# .git/hooks/post-receive
# Loom auto-detects the new commit and rebuilds
echo "Push received — Loom will pick up changes automatically"
```
