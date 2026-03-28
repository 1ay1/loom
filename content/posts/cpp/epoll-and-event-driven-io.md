---
title: "Linux epoll — How a Single Thread Handles Thousands of Connections"
date: 2025-10-24
slug: epoll-and-event-driven-io
tags: cpp, epoll, linux, networking, event-loop
excerpt: One thread, one epoll instance, ten thousand connections. No thread pool required.
---

Loom serves HTTP on a single thread. Not because it's a toy — because a single-threaded event loop, done right, outperforms a thread-per-connection model for I/O-bound workloads. The key is `epoll`, Linux's mechanism for monitoring thousands of file descriptors simultaneously.

This post explains how epoll works, why it exists, and how Loom uses it to build a production HTTP server in about 400 lines of C++.

## Why Blocking I/O Doesn't Scale

The naive approach to a network server:

```cpp
// Don't do this
while (true) {
    int client = accept(server_fd, nullptr, nullptr);  // blocks
    std::thread([client] {
        char buf[8192];
        int n = read(client, buf, sizeof(buf));         // blocks
        // ... process request ...
        write(client, response.data(), response.size()); // blocks
        close(client);
    }).detach();
}
```

One thread per connection. Each thread blocks on `read()` and `write()`. For 10,000 concurrent connections, you need 10,000 threads. Each thread consumes ~8MB of stack space. That's 80GB of virtual memory just for stacks. The OS scheduler groans under the weight of context-switching between thousands of threads, most of which are just waiting for I/O.

This doesn't scale. The solution is: instead of blocking on each connection, wait for *any* connection to be ready, then handle it.

## The select/poll/epoll History

`select()` (1983): Monitor up to FD_SETSIZE (typically 1024) file descriptors. You pass in a bitmap of fds, the kernel scans all of them, and returns which ones are ready. O(n) per call, fixed upper limit. Ancient.

`poll()` (1986): Like select but no fixed limit — you pass an array of `pollfd` structs. Still O(n) per call because the kernel scans the entire array.

`epoll` (Linux 2.6, 2002): O(1) for adding/removing fds, O(ready) for waiting. The kernel maintains the interest set internally. You only pay for the fds that are actually ready, not the total number you're monitoring.

For a server with 10,000 connections where 10 are ready at any moment, select/poll do 10,000 checks. epoll does 10.

## The Three epoll Calls

### epoll_create1

```cpp
int epoll_fd = epoll_create1(0);
```

Creates an epoll instance. Returns a file descriptor that represents the interest set. The `0` argument means no flags (you could pass `EPOLL_CLOEXEC` if needed).

Loom creates one epoll instance for the server's lifetime:

```cpp
epoll_fd_ = epoll_create1(0);
```

### epoll_ctl

Adds, modifies, or removes file descriptors from the interest set:

```cpp
// Add the server socket — watch for incoming connections
epoll_event ev{};
ev.events = EPOLLIN;       // interested in readability
ev.data.fd = server_fd_;
epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

// Add a client socket — watch for readability, edge-triggered
epoll_event client_ev{};
client_ev.events = EPOLLIN | EPOLLET;
client_ev.data.fd = client_fd;
epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev);

// Switch to watching for writability
epoll_event write_ev{};
write_ev.events = EPOLLOUT | EPOLLET;
write_ev.data.fd = client_fd;
epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &write_ev);

// Remove a connection
epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
```

Each call is O(1). The kernel updates its internal data structure and returns immediately.

### epoll_wait

Blocks until one or more fds are ready:

```cpp
epoll_event events[MAX_EVENTS];
int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
// timeout = 1000ms — returns early if events arrive

for (int i = 0; i < nfds; ++i) {
    int fd = events[i].data.fd;
    if (fd == server_fd_) {
        accept_connections();
    } else if (events[i].events & EPOLLIN) {
        handle_readable(fd);
    } else if (events[i].events & EPOLLOUT) {
        handle_writable(fd);
    }
}
```

`epoll_wait` returns only the fds that are ready — typically a small fraction of all monitored fds. The timeout parameter ensures the event loop wakes up periodically even if no events arrive (useful for housekeeping like idle connection reaping).

## Edge-Triggered vs. Level-Triggered

This is the subtlety that trips up most epoll users.

**Level-triggered** (default): epoll reports a fd as ready whenever there's data available. If you read only part of the data, epoll will report it again on the next `epoll_wait`. This is safe but can cause spurious wakeups.

**Edge-triggered** (EPOLLET): epoll reports a fd only when its state *changes* — when new data arrives, not when data is available. If you don't read all available data, epoll won't notify you again until *more* data arrives.

Loom uses edge-triggered mode:

```cpp
client_ev.events = EPOLLIN | EPOLLET;
```

This means Loom must read *all* available data when a socket becomes readable:

```cpp
void HttpServer::handle_readable(int fd) {
    auto& conn = connections_[fd];
    char buf[8192];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            conn.read_buf.append(buf, static_cast<size_t>(n));
            continue;  // keep reading — there might be more
        }
        if (n == 0) {
            close_connection(fd);  // client closed the connection
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;  // no more data — done for now
        if (errno == EINTR)
            continue;  // interrupted by signal — try again
        close_connection(fd);  // read error
        return;
    }
    process_request(fd);
}
```

The `while (true)` loop reads until `EAGAIN` — that's the edge-triggered contract. If you break out early, you'll miss data and the connection will stall.

Edge-triggered is more efficient (fewer epoll_wait wakeups) but requires more careful coding. Level-triggered is safer for beginners. Loom uses edge-triggered because it's a high-performance server.

## Non-Blocking Sockets

Edge-triggered epoll requires non-blocking sockets. If `read()` blocks on a non-ready socket, the event loop stalls and all other connections wait.

```cpp
static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
```

Loom sets every socket to non-blocking immediately after creation:

```cpp
int client_fd = accept(server_fd_, nullptr, nullptr);
set_nonblocking(client_fd);
```

With non-blocking sockets, `read()` returns `-1` with `errno == EAGAIN` when there's no data, instead of blocking. This lets the event loop check for data, do what's available, and move on to the next ready connection.

## Socket Options: SO_REUSEADDR and TCP_NODELAY

Two socket options that every server needs:

```cpp
// Allow immediate rebind after server restart
int opt = 1;
setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// Disable Nagle's algorithm — send data immediately
int flag = 1;
setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
```

**SO_REUSEADDR**: Without this, restarting the server fails with "address already in use" for 30-60 seconds (the TIME_WAIT state). With it, the kernel allows immediate rebind. Essential for development.

**TCP_NODELAY**: Nagle's algorithm batches small writes into larger TCP segments. Good for throughput, terrible for latency. An HTTP server sends a response and wants it delivered immediately, not batched with the next response 200ms later. TCP_NODELAY disables the batching.

## SIGPIPE: The Silent Killer

When you write to a socket that the client has closed, the kernel sends SIGPIPE, which terminates the process by default. This is catastrophic for a server.

```cpp
signal(SIGPIPE, SIG_IGN);
```

One line. Ignore SIGPIPE. Now `write()` returns `-1` with `errno == EPIPE` instead of killing the process. The server handles it gracefully by closing the connection.

## The Complete Event Loop

Here's Loom's event loop, stripped to its essence:

```cpp
void HttpServer::run() {
    // Setup: socket, bind, listen, epoll_create1
    // Add server_fd to epoll

    signal(SIGPIPE, SIG_IGN);

    epoll_event events[MAX_EVENTS];
    int64_t last_reap = now_ms();

    while (running_.load() && g_running.load()) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);

        if (nfds < 0) {
            if (errno == EINTR) continue;  // signal interrupted
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd_) {
                accept_connections();
            } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                close_connection(fd);
            } else if (events[i].events & EPOLLIN) {
                handle_readable(fd);
            } else if (events[i].events & EPOLLOUT) {
                handle_writable(fd);
            }
        }

        // Reap idle keepalive connections every second
        if (now_ms() - last_reap > 1000) {
            reap_idle_connections();
            last_reap = now_ms();
        }
    }

    // Cleanup: close all connections, epoll_fd, server_fd
}
```

This loop handles everything: accepting new connections, reading requests, writing responses, cleaning up idle connections, and shutting down gracefully on SIGINT. All on one thread.

The connections are tracked in an `unordered_map<int, Connection>`. Each connection holds its read buffer, write state, and keepalive flag. When a socket becomes readable, the handler reads all available data, parses the HTTP request, dispatches to the route table, and starts writing the response. If the write can't complete immediately (the kernel's send buffer is full), the connection is switched to watching for writability, and the event loop moves on to the next ready socket.

## Why Single-Threaded Works

For I/O-bound workloads — serving cached HTML, proxying requests, delivering static assets — the bottleneck is the network, not the CPU. A single thread can saturate a gigabit Ethernet link because it spends most of its time waiting for the network, and epoll makes that waiting free.

The event loop processes one event at a time, but events are fast: a cache lookup and a socket write take microseconds. Between events, the thread is idle (parked in `epoll_wait`), consuming zero CPU. A thread-per-connection model, by contrast, consumes scheduler overhead even when threads are blocked.

Loom's architecture separates the two workloads: the event loop (fast, I/O-bound, single-threaded) and the hot reloader (slow, CPU-bound, background thread). They communicate through an atomic shared pointer. The event loop never blocks, never allocates, never waits for the reloader. The reloader never touches sockets or connections.

This separation of concerns is the real insight. epoll is the mechanism; the design is the architecture.

Next: inotify — using the same event-driven philosophy to watch files change.
