---
title: io_uring and the Future of Linux I/O
date: 2024-08-10
slug: io-uring
tags: linux, performance, systems, networking
excerpt: Epoll was the answer for 20 years. io_uring is the next one — zero-copy, zero-syscall async I/O.
---

Epoll solved the C10K problem. But it still requires a syscall per I/O operation. For high-frequency trading, storage engines, and network proxies doing millions of operations per second, those syscalls add up.

## The Syscall Tax

Every `read()`, `write()`, `send()`, `recv()` crosses the user-kernel boundary:

1. Save user registers
2. Switch to kernel mode
3. Execute the operation
4. Switch back to user mode
5. Restore registers

On modern hardware this costs 100-200 nanoseconds. At 10 million operations per second, that's 1-2 seconds of pure overhead per second. Not great.

## io_uring: Shared Memory Rings

io_uring eliminates syscalls by sharing two ring buffers between user space and the kernel:

- **Submission Queue (SQ)**: user writes I/O requests
- **Completion Queue (CQ)**: kernel writes results

```cpp
// Submit a read
struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe, fd, buf, size, offset);
io_uring_submit(&ring);

// Check for completions (no syscall needed!)
struct io_uring_cqe* cqe;
io_uring_peek_cqe(&ring, &cqe);
if (cqe)
{
    int result = cqe->res;
    io_uring_cqe_seen(&ring, cqe);
}
```

After the initial setup, both submission and completion can be done entirely in user space through shared memory. The kernel polls the submission queue — no syscall needed.

## Batching

io_uring shines when batching multiple operations:

```cpp
// Queue up 100 reads
for (int i = 0; i < 100; ++i)
{
    auto* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fds[i], bufs[i], sizes[i], 0);
}

// Submit all at once — ONE syscall for 100 operations
io_uring_submit(&ring);
```

Compare that to 100 individual `read()` syscalls. The difference is dramatic at scale.

## Linked Operations

You can chain operations so the kernel executes them in sequence without returning to user space:

```cpp
// Read from file, then send to socket — zero user-space involvement
auto* sqe1 = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe1, file_fd, buf, size, 0);
sqe1->flags |= IOSQE_IO_LINK;

auto* sqe2 = io_uring_get_sqe(&ring);
io_uring_prep_send(sqe2, socket_fd, buf, size, 0);
```

The kernel reads from the file and sends to the socket without ever returning to user space. This is approaching zero-copy territory.

## Should You Use It?

If you're building a new storage engine, network proxy, or anything that does millions of I/O operations per second — absolutely. For a blog engine serving cached HTML? Epoll is more than enough. But it's good to know what's coming.
