---
title: Why epoll beats select for HTTP servers
slug: epoll-vs-select
date: 2024-03-10
tags: linux, networking, performance
---

When building a custom HTTP server, the choice of I/O multiplexing mechanism matters a lot. Here's why I switched from `select()` to `epoll`.

## select() limitations

- Linear scan of all file descriptors on every call
- Fixed `FD_SETSIZE` limit (typically 1024)
- Must rebuild the fd set every time

## epoll advantages

With `epoll`, the kernel tracks the interest list for you:

- O(1) for adding/removing descriptors
- Only returns *ready* descriptors, no scanning
- No arbitrary FD limit

## Real-world impact

In benchmarks with 500 concurrent connections, the epoll-based server handled **3x more requests per second** than the select-based version, with lower tail latency.

The difference grows as connection count increases. For any serious Linux server, epoll is the right choice.
