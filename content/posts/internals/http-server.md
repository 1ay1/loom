---
title: The HTTP Server — epoll, Keep-Alive, and Zero-Copy Responses
date: 2025-09-29
slug: http-server
tags: internals, architecture, cpp
excerpt: How Loom's HTTP server uses Linux epoll with edge-triggered I/O to handle concurrent connections in a single thread without blocking.
---

Loom's HTTP server is roughly 400 lines of C++ with no networking library. It uses Linux epoll with edge-triggered I/O, handles keep-alive connections, serves gzip-compressed responses with ETag caching, and does all of this in a single thread. This post walks through exactly how.

## The Core Idea: Event-Driven I/O

Most web servers handle connections one of two ways: thread-per-connection (simple, but a thread costs ~8MB of stack and a context switch per request) or a thread pool with blocking I/O (better, but still limited by thread count).

Loom uses neither. It uses **epoll** — the Linux kernel's mechanism for watching many file descriptors for readiness events. One thread, one epoll instance, thousands of concurrent connections.

The key insight: most connections are *waiting* most of the time. A connection that sent a request is waiting for the response. One that received a response is waiting for the next request. A single thread that only does work when work is actually available is enough.

## The Data Structures

Each connection is tracked in a `Connection` struct:

```cpp
struct Connection {
    std::string read_buf;   // accumulates incoming bytes
    std::string write_buf;  // response waiting to be sent
    size_t write_offset;    // how many bytes of write_buf already sent
    bool keep_alive;        // honour Connection: keep-alive?
    int64_t last_activity;  // milliseconds since epoch (for idle reaping)
};
```

The server keeps a `std::unordered_map<int, Connection>` keyed by file descriptor. When epoll says fd 47 is readable, we look up `connections[47]` and handle it.

## Startup

```cpp
// Create epoll instance
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

// Create listening socket
int server_fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);

// SO_REUSEADDR lets us restart immediately after a crash
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

// Bind to all interfaces, port 8080
bind(server_fd, (sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);

// Register listening socket with epoll
epoll_event ev = { .events = EPOLLIN, .data.fd = server_fd };
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
```

`EPOLL_CLOEXEC` ensures the epoll fd closes automatically on exec — good hygiene. `SOCK_NONBLOCK` makes the listening socket non-blocking so accept() never sleeps.

## The Event Loop

```cpp
epoll_event events[256];
while (running) {
    int n = epoll_wait(epoll_fd, events, 256, 1000); // 1s timeout for idle reaping
    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        if (fd == server_fd)
            handle_accept();
        else if (events[i].events & EPOLLIN)
            handle_readable(fd);
        else if (events[i].events & EPOLLOUT)
            handle_writable(fd);
        else
            close_connection(fd); // EPOLLERR or EPOLLHUP
    }
    reap_idle_connections(); // close connections idle > 5s
}
```

`epoll_wait` blocks until at least one fd is ready, then returns them all at once. The 1-second timeout is just for running `reap_idle_connections()` — we don't want dead connections building up indefinitely.

## Accepting Connections

```cpp
void handle_accept() {
    while (true) { // drain all pending connections
        int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // done
            break; // other error
        }

        // Disable Nagle's algorithm — send immediately, don't buffer
        int one = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        // Register with epoll — edge-triggered
        epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = client_fd };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

        connections[client_fd] = {};
        connections[client_fd].last_activity = now_ms();
    }
}
```

The `while (true)` loop drains all pending connections — `EPOLLET` (edge-triggered) only fires *once* when the state changes from "no connections" to "connections available." If we only accept one and return, we'd miss the rest until the next new connection arrives. We drain until `EAGAIN`.

**TCP_NODELAY:** Nagle's algorithm batches small writes to fill a full TCP segment. Good for throughput, bad for interactive HTTP. With TCP_NODELAY, each `send()` goes out immediately. A 200-byte response doesn't wait for more data that will never come.

## Edge-Triggered I/O

`EPOLLIN | EPOLLET` means: tell me once when data arrives, then don't tell me again until I've read everything. The implication: we *must* read until `EAGAIN`, otherwise we'll miss data.

```cpp
void handle_readable(int fd) {
    auto& conn = connections[fd];
    char buf[8192];

    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            conn.read_buf.append(buf, n);
            if (conn.read_buf.size() > 1048576) { // 1MB limit
                close_connection(fd);
                return;
            }
        } else if (n == 0) {
            close_connection(fd); // clean shutdown from client
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // no more data
            close_connection(fd); // real error
            return;
        }
    }

    try_parse_and_respond(fd);
}
```

We read in 8KB chunks until `EAGAIN`. A complete HTTP request might arrive in one read or several — we accumulate into `read_buf` and only parse when we have something usable.

## Request Parsing

`try_parse_and_respond` checks whether `read_buf` contains a complete HTTP request. A request is complete when:
- The header section ends (`\r\n\r\n` present)
- If `Content-Length` is set, that many body bytes have arrived

If not complete, we return and wait for more data. If complete, we parse the request line and headers:

```
GET /post/http-server HTTP/1.1\r\n
Host: localhost:8080\r\n
Accept-Encoding: gzip, deflate\r\n
If-None-Match: "1234567890"\r\n
Connection: keep-alive\r\n
\r\n
```

The parsed `Request` struct gets: method (GET), path (`/post/http-server`), HTTP version, and a header map. It then goes to the router.

## Sending Responses

`handle_writable` handles the case where a `send()` returned `EAGAIN` — the kernel's send buffer was full. We registered `EPOLLOUT` on the fd, and now the buffer has room.

```cpp
void handle_writable(int fd) {
    auto& conn = connections[fd];

    while (conn.write_offset < conn.write_buf.size()) {
        ssize_t n = send(fd,
            conn.write_buf.data() + conn.write_offset,
            conn.write_buf.size() - conn.write_offset,
            MSG_NOSIGNAL);
        if (n > 0) {
            conn.write_offset += n;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; // still full, wait for next EPOLLOUT
        } else {
            close_connection(fd);
            return;
        }
    }

    // Fully sent
    if (conn.keep_alive) {
        // Reset for next request
        conn.read_buf.clear();
        conn.write_buf.clear();
        conn.write_offset = 0;
        // Switch back to EPOLLIN
        epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = fd };
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    } else {
        close_connection(fd);
    }
}
```

`MSG_NOSIGNAL` prevents `SIGPIPE` from killing the process if the client disconnected mid-send. The server also ignores `SIGPIPE` globally at startup, but this is defense-in-depth.

## ETag and 304 Not Modified

Every cached page has a pre-computed ETag — a quoted hash of the uncompressed HTML:

```cpp
std::string etag = "\"" + std::to_string(std::hash<std::string>{}(html)) + "\"";
```

When a response is built:

```cpp
// Always include ETag
headers += "ETag: " + page.etag + "\r\n";
headers += "Cache-Control: public, max-age=60, must-revalidate\r\n";

// Check If-None-Match from request
if (req.headers.count("if-none-match") &&
    req.headers.at("if-none-match") == page.etag) {
    // Identical — send 304
    return "HTTP/1.1 304 Not Modified\r\n" + headers + "\r\n";
}

// Different or absent — send full response
```

`max-age=60` means browsers cache pages for 60 seconds. `must-revalidate` means they must check with the ETag after that. This gives snappy repeat visits without serving stale content longer than a minute.

## Gzip

Every `CachedPage` stores two versions: `raw` (minified HTML) and `gzipped` (zlib-compressed, level 9). The server checks `Accept-Encoding` at response time:

```cpp
bool wants_gzip = req.headers.count("accept-encoding") &&
    req.headers.at("accept-encoding").find("gzip") != std::string::npos;

if (wants_gzip && !page.gzipped.empty()) {
    headers += "Content-Encoding: gzip\r\n";
    headers += "Content-Length: " + std::to_string(page.gzipped.size()) + "\r\n";
    body = page.gzipped;
} else {
    headers += "Content-Length: " + std::to_string(page.raw.size()) + "\r\n";
    body = page.raw;
}
```

The gzip is computed once at cache-build time, not per-request. A 30KB HTML page compresses to roughly 7KB. For a blog that might serve the same post thousands of times, this saves real bandwidth.

## Keep-Alive and Idle Reaping

HTTP/1.1 defaults to keep-alive — the client can send multiple requests over the same TCP connection. This amortises the cost of TCP handshake and TLS setup (if behind a TLS proxy). Loom honours `Connection: keep-alive` by default and only closes after `Connection: close` or a 5-second idle timeout:

```cpp
void reap_idle_connections() {
    int64_t now = now_ms();
    for (auto it = connections.begin(); it != connections.end(); ) {
        if (now - it->second.last_activity > 5000) {
            close(it->first);
            it = connections.erase(it);
        } else {
            ++it;
        }
    }
}
```

This runs once per second (driven by the 1-second `epoll_wait` timeout). A connection idle for 5 seconds gets closed regardless of what the client wants.

## Why Single-Threaded Works

The server does almost no computation per request: look up the fd in the connection map, look up the path in the cache (hash table lookup), decide gzip vs raw, build the response string, send it. The bottleneck is network I/O, not CPU.

A single thread can sustain tens of thousands of concurrent connections as long as we never block. The only thing that could change this is if the route handlers did expensive work — but they don't. Every response is pre-built.
