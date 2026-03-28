---
title: The HTTP Server — epoll, Zero-Copy Writes, and Pre-Serialized Responses
date: 2025-09-29
slug: http-server
tags: internals, architecture, cpp
excerpt: How Loom's HTTP server uses Linux epoll with edge-triggered I/O to handle concurrent connections in a single thread, with zero-copy writes from pre-serialized cache data and zero-allocation request parsing.
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

    // Write state: either owned data or a view into shared cache
    std::string write_owned;              // for dynamic responses
    std::shared_ptr<const void> write_ref; // keeps cached data alive
    const char* write_ptr = nullptr;       // current write position
    size_t write_len = 0;                  // total bytes to write
    size_t write_offset = 0;               // bytes already written

    bool keep_alive = true;
    int64_t last_activity_ms = 0;
};
```

The write side has two paths. For dynamic responses (static file serving, error pages), `write_owned` holds the serialized bytes. For cached pages, `write_ref` holds a `shared_ptr` to the cache snapshot and `write_ptr` points directly into the pre-serialized wire data. Same `write()` syscall, zero copies.

## Startup

```cpp
int epoll_fd = epoll_create1(0);

int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

// Dual-stack: accept both IPv4 and IPv6
int v6only = 0;
setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

bind(server_fd, (sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);
set_nonblocking(server_fd);

epoll_event ev = { .events = EPOLLIN, .data.fd = server_fd };
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
```

`SO_REUSEADDR` lets us restart immediately after a crash without waiting for `TIME_WAIT`. The dual-stack setup means one socket handles both IPv4 and IPv6.

## The Event Loop

```cpp
epoll_event events[256];
while (running) {
    int n = epoll_wait(epoll_fd, events, 256, 1000);
    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        if (fd == server_fd)           accept_connections();
        else if (events[i].events & EPOLLIN)  handle_readable(fd);
        else if (events[i].events & EPOLLOUT) handle_writable(fd);
        else                           close_connection(fd);
    }
    reap_idle_connections(); // close connections idle > 5s
}
```

`epoll_wait` blocks until at least one fd is ready, then returns them all at once. The 1-second timeout is just for running `reap_idle_connections()`.

## Accepting Connections

```cpp
void accept_connections() {
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        set_nonblocking(client_fd);

        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = client_fd };
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        connections[client_fd] = Connection{};
    }
}
```

The `while (true)` loop drains all pending connections — `EPOLLET` (edge-triggered) only fires once when the state changes. We drain until `EAGAIN`.

**TCP_NODELAY** disables Nagle's algorithm. A 200-byte response doesn't wait for more data that will never come.

## Zero-Allocation Request Parsing

Request parsing uses `string_view` throughout. No `substr()` copies, no `std::string` allocations for headers, paths, or query strings:

```cpp
bool parse_request(std::string& raw, HttpRequest& request) {
    std::string_view sv(raw);

    auto header_end = sv.find("\r\n\r\n");
    if (header_end == std::string_view::npos) return false;

    // Method
    auto method_end = sv.find(' ');
    request.method = parse_method(sv.substr(0, method_end)); // string_view, not string

    // Path and query — views into the raw buffer
    auto uri = sv.substr(path_start, path_end - path_start);
    auto qpos = uri.find('?');
    request.path = (qpos != npos) ? uri.substr(0, qpos) : uri;

    // Headers — keys lowercased in-place, stored as string_view pairs
    while (pos < header_end) {
        for (size_t i = pos; i < colon; ++i)
            raw[i] = std::tolower(raw[i]); // mutate in-place

        request.headers_.push_back({
            sv.substr(pos, colon - pos),        // key: string_view
            sv.substr(val_start, line_end - val_start) // value: string_view
        });
    }
    return true;
}
```

`HttpRequest` stores `string_view` fields pointing into the connection's `read_buf`. The request lives on the stack during `process_request()` — the views are valid for the entire handler call. After the handler returns, `read_buf` is cleared and the views become dangling, but they're never accessed again.

Header keys are lowercased in-place by mutating the raw buffer directly. This avoids allocating a lowercase copy for every header. Lookups use `string_view` comparison — a linear scan over typically 10-15 headers, which is faster than hash map overhead for small N.

## Pre-Serialized Wire Responses

For cached HTML pages, the entire HTTP response — status line, headers, and body — is pre-built at cache time. The server writes this directly to the socket with zero per-request work:

```cpp
struct CachedPage {
    std::string etag;
    std::string gzip_ka;        // HTTP/1.1 200 OK\r\n...body (keep-alive, gzip)
    std::string gzip_close;     // same but Connection: close
    std::string plain_ka;       // uncompressed, keep-alive
    std::string plain_close;    // uncompressed, close
    std::string not_modified_ka;  // 304
    std::string not_modified_close;
};
```

Six variants per page: `{gzip, plain, 304} × {keep-alive, close}`. The response handler just picks the right one:

```cpp
HttpResponse serve_cached(const CachedPage& page, const HttpRequest& req,
                          const std::shared_ptr<const SiteCache>& snap) {
    bool ka = req.keep_alive();
    auto inm = req.header("if-none-match");

    if (!inm.empty() && inm == page.etag)
        return HttpResponse::prebuilt(snap, ka ? page.not_modified_ka : page.not_modified_close);
    if (accepts_gzip(req))
        return HttpResponse::prebuilt(snap, ka ? page.gzip_ka : page.gzip_close);
    return HttpResponse::prebuilt(snap, ka ? page.plain_ka : page.plain_close);
}
```

`HttpResponse::prebuilt` stores a `shared_ptr<const void>` (keeping the cache alive) and a raw pointer into the wire data. No string copy, no serialization, no `Content-Length` formatting. The response is a pointer.

## Zero-Copy Write Path

The server has two write paths:

```cpp
void start_write_owned(int fd, std::string data) {
    conn.write_owned = std::move(data);
    conn.write_ptr = conn.write_owned.data();
    conn.write_len = conn.write_owned.size();
    handle_writable(fd);
}

void start_write_view(int fd, std::shared_ptr<const void> owner,
                      const char* data, size_t len) {
    conn.write_ref = std::move(owner);  // cache stays alive
    conn.write_ptr = data;              // point into cache
    conn.write_len = len;
    handle_writable(fd);
}
```

`handle_writable` is the same for both — it writes from `write_ptr`:

```cpp
while (conn.write_offset < conn.write_len) {
    ssize_t n = write(fd, conn.write_ptr + conn.write_offset,
                      conn.write_len - conn.write_offset);
    if (n > 0) { conn.write_offset += n; continue; }
    if (errno == EAGAIN) { /* register EPOLLOUT, return */ }
    // error → close
}
```

For cached pages, the bytes flow directly from the `SiteCache`'s pre-serialized strings through the kernel's socket buffer to the network. The only copy is the kernel's `write()` from userspace to kernel space — unavoidable without `io_uring` or `sendfile`.

After the write completes, `write_ref.reset()` releases the cache snapshot. If a hot-reload happened mid-write, the old cache is freed now. The new cache is already serving new requests.

## The Dispatch Function

The server doesn't know about routing. It stores a single dispatch function:

```cpp
class HttpServer {
    std::function<HttpResponse(HttpRequest&)> dispatch_;
    // ...
};
```

In `process_request`:

```cpp
HttpRequest request;
parse_request(conn.read_buf, request);
conn.keep_alive = request.keep_alive();

auto response = dispatch_(request);
conn.read_buf.clear();

if (response.is_prebuilt())
    start_write_view(fd, std::move(response.wire_owner_),
                     response.wire_data_, response.wire_len_);
else
    start_write_owned(fd, response.serialize(conn.keep_alive));
```

The dispatch function is the [compile-time route table](/post/router) — a fold expression over template-encoded routes. The server doesn't care how dispatch works; it just calls it and writes the result.

## ETag and 304

Every cached page has a pre-computed ETag and pre-serialized 304 response. When a browser sends `If-None-Match`, the handler compares one `string_view` against the stored ETag. On match, the pre-built 304 response (roughly 150 bytes of headers, no body) goes straight to the socket. The browser uses its local cache.

`Cache-Control: public, max-age=60, must-revalidate` means browsers cache pages for 60 seconds, then revalidate with an ETag check. Hot reloads are visible within a minute.

## Gzip

Every `CachedPage` stores both compressed and uncompressed wire responses, built at cache time with zlib level 9. The server checks `Accept-Encoding` at response time — a `string_view::find("gzip")` call — and picks the right pre-built variant. No per-request compression.

A 30KB HTML page compresses to roughly 7KB. For a blog that serves the same post thousands of times, this saves real bandwidth with zero CPU cost per request.

## Keep-Alive and Idle Reaping

HTTP/1.1 defaults to keep-alive. Loom honours it by switching the fd back to `EPOLLIN | EPOLLET` after a write completes. Connections idle for 5 seconds are reaped once per second.

With edge-triggered epoll, data may arrive while a response is being written. After completing a write and re-arming `EPOLLIN`, we immediately call `handle_readable(fd)` to drain any data the kernel buffered during the write. Without this, the connection would hang until new data triggers another edge.

## Why Single-Threaded Works

The server does almost no computation per request. For cached pages: one atomic `shared_ptr` load, one hash table lookup, one `string_view` comparison, one `write()` syscall. Everything else — rendering, compression, minification — happened at cache build time. The bottleneck is network I/O, not CPU.
