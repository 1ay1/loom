---
title: "Zero to Blog Engine: The Epoll Event Loop"
date: 2024-06-15
slug: epoll-event-loop
tags: cpp, linux, networking, performance, loom
excerpt: One thread, thousands of connections. Epoll turns your server from a toy into something production-worthy.
---

Last time we built an HTTP server that handles one connection at a time. If client A is slow to send its request, client B waits. This is unacceptable.

## The Naive Fix: Threads

The obvious solution is one thread per connection:

```cpp
while (true)
{
    int client = accept(server_fd, ...);
    std::thread([client] {
        handle_request(client);
        close(client);
    }).detach();
}
```

This works until you have 10,000 concurrent connections. Each thread costs ~8MB of stack space. That's 80GB of virtual memory. The context-switching overhead alone will kill your throughput.

## Enter Epoll

Epoll lets a single thread monitor thousands of file descriptors simultaneously:

```cpp
int epfd = epoll_create1(0);

// Add the server socket
epoll_event ev{};
ev.events = EPOLLIN | EPOLLET;
ev.data.fd = server_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
```

The event loop polls for ready file descriptors:

```cpp
epoll_event events[1024];
while (running)
{
    int n = epoll_wait(epfd, events, 1024, -1);
    for (int i = 0; i < n; ++i)
    {
        if (events[i].data.fd == server_fd)
            accept_new_connection(epfd, server_fd);
        else
            handle_client(events[i]);
    }
}
```

One thread. No context switches. The kernel tells us exactly which sockets have data ready to read. We process them in order, never blocking.

## Edge-Triggered vs Level-Triggered

The `EPOLLET` flag enables edge-triggered mode:

- **Level-triggered** (default): epoll_wait returns as long as the fd is readable
- **Edge-triggered**: epoll_wait returns only when new data arrives

Edge-triggered is more efficient but requires draining the socket completely on each notification:

```cpp
while (true)
{
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0)
    {
        if (errno == EAGAIN) break;  // no more data
        // handle error
    }
    if (n == 0) break;  // connection closed
    // process n bytes
}
```

If you don't drain the buffer, you'll never get another notification for that socket.

## Non-Blocking Sockets

Epoll requires non-blocking sockets. Otherwise `read` and `write` could block the event loop, defeating the purpose:

```cpp
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

With non-blocking sockets, every I/O call either succeeds immediately or returns `EAGAIN`. No waiting, no blocking, no surprises.

## Performance

A single-threaded epoll server on modern hardware can handle 100,000+ concurrent connections and saturate a 10Gbps network link. The bottleneck moves from the server to the network — exactly where you want it.

Next: routing requests and rendering markdown into HTML.
