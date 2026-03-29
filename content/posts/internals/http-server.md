---
title: "The HTTP Server — An Epoll Event Loop in 400 Lines"
date: 2025-12-26
slug: http-server
tags: [loom-internals, c++20, http, networking, epoll]
excerpt: "How Loom serves HTTP with a single-threaded epoll event loop, zero-copy request parsing via string_view, pre-serialized wire responses, and a dual write path that avoids allocations on the hot path."
---

Most web frameworks delegate HTTP to a library. Nginx, libuv, Boost.Asio — pick your abstraction, configure it, and move on. Loom does not use any of them. Its HTTP server is 400 lines of C++ using the Linux `epoll` API directly. One thread, no mutexes, no thread pool, no coroutines. Just a file descriptor, an event loop, and pre-built byte buffers.

This post covers the three files that make up the entire HTTP stack: `request.cpp` (113 lines), `response.cpp` (99 lines), and `server.cpp` (410 lines). It builds on [post #6 on string_view](/post/string-view-and-zero-copy) for the zero-copy request parsing, and connects directly to the [cache and rendering](/post/cache-and-rendering) post for the pre-built response serving.

## The Request Parser: Zero Allocations

The HTTP request parser operates entirely on `string_view`s into the raw read buffer. The `HttpRequest` struct stores no strings of its own:

```cpp
struct HttpRequest
{
    HttpMethod method = HttpMethod::GET;
    std::string_view path;
    std::string_view query;
    std::vector<std::string_view> params;
    std::string_view body;

    struct Header { std::string_view key; std::string_view value; };
    std::vector<Header> headers_;

    std::string_view header(std::string_view key) const;
    bool keep_alive() const;
};
```

Every field — path, query, body, header keys, header values — is a view into the connection's `read_buf` string. The parser never copies a byte of request data. This is the string_view technique from [post #6](/post/string-view-and-zero-copy) applied at the protocol level.

The function signature reveals the key constraint:

```cpp
bool parse_request(std::string& raw, HttpRequest& request);
```

It takes the raw buffer by mutable reference because it does one thing that requires mutation: lowercasing header keys in-place.

```cpp
// Headers — lowercase keys in-place in the raw buffer
for (size_t i = pos; i < colon; ++i)
    raw[i] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(raw[i])));
```

This is a deliberate choice. HTTP header keys are case-insensitive per the spec, so every lookup would need case-insensitive comparison. Instead of paying that cost on every header access, the parser normalizes once during parsing. After this, all header lookups are simple string equality checks against lowercase keys.

### Fast Keep-Alive Detection

The `Connection: close` header determines whether the server should close the socket or reuse it. Rather than calling a general-purpose case-insensitive comparison, the parser uses bitwise OR to check each character:

```cpp
bool HttpRequest::keep_alive() const
{
    auto v = header("connection");
    if (v.size() == 5)
    {
        return !((v[0] | 0x20) == 'c' && (v[1] | 0x20) == 'l' &&
                 (v[2] | 0x20) == 'o' && (v[3] | 0x20) == 's' &&
                 (v[4] | 0x20) == 'e');
    }
    return true;
}
```

The `| 0x20` trick works because for ASCII letters, the lowercase and uppercase variants differ only in bit 5. `'C' | 0x20` gives `'c'`. `'c' | 0x20` gives `'c'`. This is a single instruction per character — no branch, no function call.

### Content-Length Without strtol

The parser avoids `strtol` for parsing the Content-Length value. Instead it does the digit loop inline:

```cpp
size_t cl = 0;
for (char c : h.value)
    if (c >= '0' && c <= '9') cl = cl * 10 + (c - '0');
```

This skips leading whitespace naturally (non-digit characters are ignored) and compiles to a tight loop without the overhead of a library function that has to handle bases, signs, and error reporting.

## The Response: Two Modes

The response struct supports two fundamentally different serving modes:

```cpp
struct HttpResponse
{
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // Pre-serialized wire data
    std::shared_ptr<const void> wire_owner_;
    const char* wire_data_ = nullptr;
    size_t wire_len_ = 0;

    bool is_prebuilt() const noexcept { return wire_data_ != nullptr; }

    static HttpResponse prebuilt(std::shared_ptr<const void> owner,
                                  const std::string& wire);

    std::string serialize(bool keep_alive) const;
};
```

**Dynamic mode** uses `status`, `headers`, and `body`. The `serialize()` method builds the wire format on demand — status line, headers, Content-Length, Connection header, body. This path is used for error responses and static file fallbacks.

**Prebuilt mode** bypasses serialization entirely. The `wire_data_` pointer points directly into the pre-rendered cache, and `wire_owner_` (a `shared_ptr<const void>`) keeps the cache snapshot alive. This is the path used for every cached page — the response is already a complete HTTP/1.1 byte sequence ready to write to the socket.

The `shared_ptr<const void>` trick deserves a note. The cache is stored as `shared_ptr<const SiteCache>`, but the response stores `shared_ptr<const void>`. This works because `shared_ptr` preserves the deleter regardless of the pointer type — you can type-erase the pointer while keeping the correct destructor. This was covered in [post #5 on ownership](/post/ownership-and-move-semantics).

### Compile-Time Status Lines

The status line is resolved by a `constexpr` switch:

```cpp
static constexpr std::string_view status_line(int status) noexcept
{
    switch (status)
    {
        case 200: return "HTTP/1.1 200 OK\r\n";
        case 301: return "HTTP/1.1 301 Moved Permanently\r\n";
        case 304: return "HTTP/1.1 304 Not Modified\r\n";
        case 400: return "HTTP/1.1 400 Bad Request\r\n";
        case 404: return "HTTP/1.1 404 Not Found\r\n";
        // ...
        default:  return "HTTP/1.1 200 OK\r\n";
    }
}
```

At `-O2` this compiles to a jump table or a chain of comparisons — no string construction, no `std::to_string`, no runtime formatting. The `constexpr` means the compiler can even fold this away completely when the status is known at compile time.

## The Connection

Each accepted client gets a `Connection` struct:

```cpp
struct Connection
{
    std::string read_buf;

    // Write state: either owned data or a view into shared cache data
    std::string write_owned;
    std::shared_ptr<const void> write_ref;
    const char* write_ptr = nullptr;
    size_t write_len = 0;
    size_t write_offset = 0;

    bool keep_alive = true;
    int64_t last_activity_ms = 0;
};
```

The write path has a dual-buffer design:

- `write_owned` holds a dynamically serialized response string (for errors and uncached responses). The connection owns the data.
- `write_ref` holds a shared pointer to cache memory, and `write_ptr` points directly into that cache. The connection borrows the data.

In both cases, `write_ptr` and `write_len` point to the bytes to send. The server does not care which path produced them — it just writes from `write_ptr + write_offset`.

```cpp
void HttpServer::start_write_owned(int fd, std::string data)
{
    auto& conn = it->second;
    conn.write_owned = std::move(data);
    conn.write_ref.reset();
    conn.write_ptr = conn.write_owned.data();
    conn.write_len = conn.write_owned.size();
    conn.write_offset = 0;
    handle_writable(fd);
}

void HttpServer::start_write_view(int fd, std::shared_ptr<const void> owner,
                                   const char* data, size_t len)
{
    auto& conn = it->second;
    conn.write_ref = std::move(owner);
    conn.write_owned.clear();
    conn.write_ptr = data;
    conn.write_len = len;
    conn.write_offset = 0;
    handle_writable(fd);
}
```

The `start_write_view` path is the hot path. For a cached page, the server does: look up the path in the cache map, get a `shared_ptr` to the cache snapshot, get a pointer to the prebuilt wire bytes, and start writing. Zero copies, zero allocations.

## The Event Loop

The server runs a single-threaded event loop using Linux `epoll`:

```cpp
void HttpServer::run()
{
    server_fd_ = socket(AF_INET6, SOCK_STREAM, 0);

    // Dual-stack: accept both IPv4 and IPv6
    int v6only = 0;
    setsockopt(server_fd_, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

    // ...bind, listen...

    set_nonblocking(server_fd_);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    epoll_fd_ = epoll_create1(0);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    epoll_event events[MAX_EVENTS];
    int64_t last_reap = now_ms();

    while (running_.load() && g_running.load())
    {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);

        for (int i = 0; i < nfds; ++i)
        {
            int fd = events[i].data.fd;

            if (fd == server_fd_)           { accept_connections(); continue; }
            if (events[i].events & EPOLLERR
             || events[i].events & EPOLLHUP) { close_connection(fd); continue; }
            if (events[i].events & EPOLLIN)   handle_readable(fd);
            if (events[i].events & EPOLLOUT)  handle_writable(fd);
        }

        // Reap idle connections every second
        auto now = now_ms();
        if (now - last_reap > 1000)
        {
            reap_idle_connections();
            last_reap = now;
        }
    }
}
```

A few details worth noting:

**Dual-stack IPv6.** The server binds to `AF_INET6` with `IPV6_V6ONLY` set to 0. This means a single socket accepts both IPv4 and IPv6 connections. One socket, one `epoll` instance, all clients.

**Edge-triggered epoll.** Client connections use `EPOLLET` (edge-triggered), which means epoll only notifies when new data arrives, not while data remains in the buffer. This requires the read loop to drain all available data on each event:

```cpp
char buf[8192];
while (true)
{
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) { conn.read_buf.append(buf, static_cast<size_t>(n)); continue; }
    if (n == 0) { close_connection(fd); return; }
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
    if (errno == EINTR) continue;
    close_connection(fd);
    return;
}
```

**TCP_NODELAY.** Every accepted connection gets `TCP_NODELAY` set, disabling Nagle's algorithm. For a blog server that sends complete responses in one write, there is no benefit to buffering small packets. The response is either a pre-built buffer of several kilobytes or nothing — so disabling Nagle reduces latency without increasing packet count.

**SIGPIPE ignored.** Writing to a socket whose peer has closed generates `SIGPIPE`, which kills the process by default. Setting `SIG_IGN` lets the server handle the `EPIPE` error code gracefully instead of dying.

## Write Backpressure

The write path handles the case where the kernel's TCP send buffer is full:

```cpp
void HttpServer::handle_writable(int fd)
{
    auto& conn = it->second;

    while (conn.write_offset < conn.write_len)
    {
        ssize_t n = write(fd,
            conn.write_ptr + conn.write_offset,
            conn.write_len - conn.write_offset);

        if (n > 0) { conn.write_offset += static_cast<size_t>(n); continue; }

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Socket buffer full — switch to EPOLLOUT and wait
                epoll_event ev{};
                ev.events = EPOLLOUT | EPOLLET;
                ev.data.fd = fd;
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
                return;
            }
            if (errno == EINTR) continue;
        }

        close_connection(fd);
        return;
    }

    // Write complete — clean up and re-arm for reading
    conn.write_owned.clear();
    conn.write_ref.reset();
    // ...

    if (!conn.keep_alive) { close_connection(fd); return; }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}
```

When `write()` returns `EAGAIN`, the kernel's send buffer is full. The server switches the connection to `EPOLLOUT` mode — epoll will notify when the buffer drains and the socket is ready to accept more data. When the write eventually completes, the connection switches back to `EPOLLIN` to wait for the next request.

This is how a single thread handles many concurrent connections without blocking. While one connection waits for its send buffer to drain, the event loop continues processing other connections.

## Content-Length Guarding

Before parsing a request, the server checks whether the full body has arrived:

```cpp
auto cl_pos = buf_view.find("Content-Length:");
if (cl_pos == std::string_view::npos)
    cl_pos = buf_view.find("content-length:");

if (cl_pos != std::string_view::npos && cl_pos < header_end)
{
    auto val_start = cl_pos + 15;
    auto val_end = buf_view.find("\r\n", val_start);
    size_t content_length = 0;
    for (size_t i = val_start; i < val_end && i < buf_view.size(); ++i)
    {
        char c = buf_view[i];
        if (c >= '0' && c <= '9')
            content_length = content_length * 10 + (c - '0');
    }
    if (conn.read_buf.size() - (header_end + 4) < content_length)
        return;  // Wait for more data
}
```

This runs on the raw buffer before the full parse. It manually scans for the Content-Length header and parses the integer inline — no `strtol`, no temporary strings. If the body is incomplete, the server simply returns and waits for the next `EPOLLIN` event to deliver more data. This avoids wasting time parsing an incomplete request.

## The Dispatch

Once a request is parsed, dispatch is a single function call:

```cpp
auto response = dispatch_(request);

if (response.is_prebuilt())
{
    start_write_view(fd, std::move(response.wire_owner_),
                     response.wire_data_, response.wire_len_);
}
else
{
    start_write_owned(fd, response.serialize(conn.keep_alive));
}
```

The dispatch function is the [compiled route table](/post/router). It returns an `HttpResponse` that is either prebuilt (cache hit) or dynamic (error, static file). The server checks `is_prebuilt()` and takes the appropriate write path.

For a cached page — which is the vast majority of requests — this entire sequence is: parse the request (zero-copy into the read buffer), look up the path in a hash map (with transparent hashing, so the `string_view` path works without allocation), get the prebuilt wire bytes, and start writing them to the socket. No template rendering, no serialization, no string building.

## Keepalive and Idle Reaping

The server supports HTTP/1.1 keep-alive by default. After completing a response, if `keep_alive` is true, the connection stays open and switches back to `EPOLLIN` to wait for the next request.

To prevent leaked connections from accumulating, the event loop reaps idle connections every second:

```cpp
void HttpServer::reap_idle_connections()
{
    auto now = now_ms();
    std::vector<int> to_close;

    for (auto& [fd, conn] : connections_)
    {
        if (now - conn.last_activity_ms > KEEPALIVE_TIMEOUT_MS)
            to_close.push_back(fd);
    }

    for (int fd : to_close)
        close_connection(fd);
}
```

The timeout is 5 seconds. This is aggressive, but for a blog server it is appropriate — clients fetch a page and its assets, then stop. Long-lived connections waste file descriptors without benefit.

## Why a Custom Server

The standard answer is "use a library." For most applications that is the right answer. But Loom's HTTP needs are narrow: serve prebuilt byte buffers with keep-alive and gzip negotiation. No WebSocket, no chunked transfer encoding, no TLS (that is what a reverse proxy is for), no HTTP/2.

A general-purpose HTTP library brings complexity for features Loom does not use. The custom server is 400 lines, has no dependencies beyond POSIX and `epoll`, and is tuned specifically for the pre-rendered cache architecture. The zero-copy write path — where the server writes directly from cache memory without touching the data — only works because the server and the cache were designed together.

The entire HTTP stack — parsing, serialization, event loop, connection management — is 622 lines across three files. That is less than most HTTP library configurations.
