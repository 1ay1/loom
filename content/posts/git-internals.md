---
title: Git Is a Content-Addressable Filesystem
date: 2024-09-01
slug: git-internals
tags: git, systems
excerpt: Git isn't a version control system. It's a content-addressable object store with a version control UI bolted on top.
---

Most people learn Git as a set of commands: commit, push, pull, merge. But Git makes a lot more sense once you understand it's actually a content-addressable filesystem with four object types.

## Four Objects

Everything in Git is one of four objects, stored by the SHA-1 hash of its content:

1. **blob** — file contents (no filename, no permissions)
2. **tree** — directory listing (maps names → blobs/trees + permissions)
3. **commit** — snapshot (points to a tree + parent commits + metadata)
4. **tag** — named pointer to a commit with a message

That's it. The entire `.git/objects/` directory is just these four types.

## Content Addressing

```
$ echo "hello" | git hash-object --stdin
ce013625030ba8dba906f756967f9e9ca394464a
```

The same content always produces the same hash. Two files with identical content share one blob. Rename a file? New tree object, same blob. This is why `git mv` is cheap — no data is copied.

## Commits Are Snapshots, Not Diffs

A common misconception: Git stores diffs between versions. It doesn't. Every commit points to a complete tree — a full snapshot of every file. Storage efficiency comes from:

1. **Identical blobs are shared** — unchanged files aren't duplicated
2. **Packfiles** — Git periodically compresses objects with delta encoding, but this is a storage optimization, not the data model

This is why `git checkout` to any commit is O(tree size), not O(history length). It doesn't replay patches — it just materializes the snapshot.

## Branches Are Pointers

A branch is a 41-byte file containing a commit hash:

```
$ cat .git/refs/heads/main
a1b2c3d4e5f6...
```

Creating a branch is writing 41 bytes. Switching branches updates HEAD. There's no copying, no branching tax. This is why Git branches are so cheap compared to SVN.

## The Reflog: Your Safety Net

Every time HEAD moves, Git logs it:

```
$ git reflog
a1b2c3d HEAD@{0}: commit: add feature
f4e5d6c HEAD@{1}: checkout: moving from main to feature
```

Even after `git reset --hard`, your commits aren't gone — they're in the reflog for 90 days. `git reflog` + `git checkout` can recover almost anything.

## Why This Matters

Understanding the object model explains Git's behavior:

- **Merge conflicts** happen when two commits modify the same blob differently — Git can't auto-merge the content
- **Rebase** creates new commit objects with new hashes (same content, different parents)
- **Cherry-pick** creates a new commit that applies the same diff to a different tree
- **Detached HEAD** just means HEAD points to a commit hash instead of a branch ref

Git isn't complicated. It's simple primitives composed in non-obvious ways.
