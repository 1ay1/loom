---
title: Linux epoll — The Event Loop That Powers Loom
date: 2024-03-01
slug: epoll-event-loop
tags: cpp, linux, internals
excerpt: epoll lets a single thread watch thousands of file descriptors for I/O readiness without polling. Here's how it works and how Loom uses it.
---

When a process wants to read from a file descriptor, two things can happen: data is ready, in which case `read()` returns immediately, or data isn't ready, in which case `read()` blocks until it is. Blocking is fine for one fd. For thousands of concurrent network connections, it's catastrophic.

Linux provides three mechanisms for watching multiple fds: `select`, `poll`, and `epoll`. Loom uses epoll — the only one that scales.

## Why Not select or poll

`select` and `poll` both work by passing a list of fds to the kernel, which scans all of them and returns which are ready. If you have 10,000 connections, the kernel scans 10,000 fds on every call, even if only two have data. O(n) per call, where n is the total number of watched fds.

`epoll` inverts the model. You register fds with the kernel once, and the kernel maintains an internal data structure. When you call `epoll_wait`, the kernel only returns fds that are actually ready — it doesn't scan anything. Amortised O(1) per ready event, regardless of how many fds are registered.

## The epoll API

Three syscalls:

```c
// Create an epoll instance; returns a file descriptor
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

// Register/modify/remove an fd from the epoll instance
int epoll_ctl(epoll_fd, op, watched_fd, &event);
// op: EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL

// Wait for events; returns count of ready events
int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);
```

The `epoll_event` struct specifies what to watch for and carries user data:

```c
struct epoll_event {
    uint32_t events;  // EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP, EPOLLET, ...
    epoll_data_t data; // union: int fd, void* ptr, uint64_t u64, ...
};
```

Loom stores the file descriptor in `data.fd` and uses it to look up the connection in its map.

## Level-Triggered vs Edge-Triggered

This is the most important thing to understand about epoll.

**Level-triggered (default):** `epoll_wait` keeps returning an fd as ready as long as there's data available. If you read 100 bytes and there are 900 more, the next `epoll_wait` will return that fd again. Easy to use but slightly less efficient — you'll get the notification even if you don't process it.

**Edge-triggered (`EPOLLET`):** `epoll_wait` returns an fd exactly once, when its state transitions from "no data" to "data available." If you don't drain all available data in one go, you won't get notified again until more data arrives. More efficient, but requires draining completely.

Loom uses `EPOLLIN | EPOLLET`:

```cpp
epoll_event ev;
ev.events  = EPOLLIN | EPOLLET;
ev.data.fd = client_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
```

And drains completely in `handle_readable`:

```cpp
while (true) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0)      { conn.read_buf.append(buf, n); continue; }
    if (n == 0)     { close_connection(fd); return; }    // EOF
    if (errno == EAGAIN) break; // fully drained — done
    close_connection(fd); return; // real error
}
```

Without the loop, a large HTTP request arriving in multiple TCP segments would get stuck after the first segment — we'd read it, stop, and never see the rest.

## Non-Blocking Sockets

Edge-triggered epoll only works correctly with non-blocking sockets. A blocking socket's `read()` would block when there's no more data; a non-blocking socket's `read()` returns `EAGAIN` immediately. That `EAGAIN` is the signal to stop.

```cpp
// Accept with SOCK_NONBLOCK
int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK);

// Or set after the fact
int flags = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

Loom uses `accept4` with `SOCK_NONBLOCK` — a Linux extension that combines accept and fcntl in one syscall.

## The Full Event Loop

```
startup:
  epoll_create1       → epoll_fd
  socket + bind + listen → server_fd (SOCK_NONBLOCK)
  epoll_ctl ADD server_fd EPOLLIN

loop:
  epoll_wait(epoll_fd, events, 256, 1000ms)
    for each event:
      if fd == server_fd:
        accept4 all pending connections (SOCK_NONBLOCK)
        TCP_NODELAY on each
        epoll_ctl ADD client_fd EPOLLIN|EPOLLET
      else if EPOLLIN:
        read until EAGAIN into conn.read_buf
        if request complete: build response, try send
        if send returns EAGAIN: epoll_ctl MOD fd EPOLLOUT
      else if EPOLLOUT:
        send remaining write_buf until EAGAIN or done
        if done and keep-alive: epoll_ctl MOD fd EPOLLIN|EPOLLET
        if done and close: close(fd)
      else (EPOLLERR | EPOLLHUP):
        close(fd)
  reap connections idle > 5s
```

The loop handles connection accept, request reading, response writing, and keep-alive — all without any threads, any mutexes, or any blocking.

## SIGPIPE

When a client closes its connection mid-response, the next `send()` to that fd raises `SIGPIPE`. By default, `SIGPIPE` terminates the process. Loom handles this two ways:

```cpp
// Globally ignore SIGPIPE at startup
signal(SIGPIPE, SIG_IGN);

// Per-send with MSG_NOSIGNAL flag
send(fd, buf, len, MSG_NOSIGNAL);
```

`MSG_NOSIGNAL` is safer — it affects only that send, not the entire process. The global ignore is defense-in-depth.

## EPOLL_CLOEXEC

```cpp
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
```

`CLOEXEC` sets the close-on-exec flag on the epoll fd. If Loom ever forks a child (for `git show`, for example), the epoll fd is automatically closed in the child. Without this, the child would inherit the epoll fd, which could cause confusing behavior when the parent's connections become readable in the child.

## SO_REUSEADDR

```cpp
int one = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
```

Without `SO_REUSEADDR`, restarting Loom immediately after a crash fails with `EADDRINUSE`. The old socket is in `TIME_WAIT` state — the kernel keeps it for 2×MSL (typically 2 minutes) to ensure any straggling packets from the previous session are absorbed.

`SO_REUSEADDR` tells the kernel it's ok to reuse the address even in `TIME_WAIT`. Safe for servers; don't use it for clients.

## Performance Characteristics

A single epoll thread handling 10,000 idle connections costs almost nothing — `epoll_wait` with no ready events returns in microseconds, having done zero work proportional to connection count.

With all connections active (which doesn't happen in a blog), the bottleneck becomes CPU time processing requests. At that point you'd add worker threads or processes, and use `EPOLLONESHOT` to hand off connections. Loom doesn't need this — serving pre-rendered HTML from an `std::unordered_map` is fast enough for any blog load.
