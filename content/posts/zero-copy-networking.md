---
title: Zero-Copy Networking in Linux
date: 2024-03-10
slug: zero-copy-networking
tags: linux, networking, performance
excerpt: Every memcpy is a lie your framework tells you. sendfile, splice, and MSG_ZEROCOPY eliminate copies between kernel and user space.
---

A typical HTTP response involves 4 memory copies:

1. Disk → kernel page cache (`read` from disk)
2. Kernel → user buffer (`read()` syscall)
3. User buffer → kernel socket buffer (`write()` syscall)
4. Kernel socket buffer → NIC (`sendmsg` to hardware)

Copies 2 and 3 are pure waste — the data passes through user space without modification. For a static file server, that's half the memory bandwidth consumed for nothing.

## sendfile: The Original Zero-Copy

`sendfile()` transfers data from a file descriptor to a socket without passing through user space:

```cpp
// Instead of read() + write():
off_t offset = 0;
sendfile(socket_fd, file_fd, &offset, file_size);
```

The kernel copies directly from the page cache to the socket buffer. Two copies eliminated, one syscall instead of two.

Nginx, Apache, and every competent static file server use `sendfile`. For large files, the throughput improvement is 2-3x.

## splice: Pipes as Zero-Copy Conduit

`splice()` moves data between two file descriptors where at least one is a pipe, without copying to user space:

```cpp
int pipefd[2];
pipe(pipefd);

// Move data from file to pipe (zero-copy from page cache)
splice(file_fd, &offset, pipefd[1], NULL, chunk_size, SPLICE_F_MOVE);

// Move data from pipe to socket (zero-copy)
splice(pipefd[0], NULL, socket_fd, NULL, chunk_size, SPLICE_F_MOVE);
```

The pipe acts as a kernel-side buffer reference. No data moves — just page table entries get shuffled. This is more flexible than `sendfile` because it works between any fd pair (via the pipe intermediary).

HAProxy uses `splice` for proxying — data flows from client socket → pipe → backend socket without ever touching user space.

## MSG_ZEROCOPY: User Buffer Direct Send

For data already in user space (dynamically generated content), `MSG_ZEROCOPY` tells the kernel to send directly from the user buffer:

```cpp
int one = 1;
setsockopt(fd, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one));

// Kernel reads directly from user buffer
send(fd, user_buffer, length, MSG_ZEROCOPY);

// Must wait for completion notification before reusing buffer
struct msghdr msg = {};
recvmsg(fd, &msg, MSG_ERRQUEUE);
```

The catch: you must not modify the buffer until the kernel signals completion via the error queue. This adds complexity but eliminates the user→kernel copy for large sends (typically worth it above 10KB).

## io_uring + Fixed Buffers

io_uring takes this further with registered buffers:

```cpp
struct iovec iov = { .iov_base = buf, .iov_len = size };
io_uring_register_buffers(&ring, &iov, 1);

// Kernel knows this buffer's physical pages — no page table lookup needed
struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
io_uring_prep_write_fixed(sqe, fd, buf, size, 0, 0);
```

Registered buffers pin physical pages and store the kernel mapping. Subsequent I/O skips the page table walk entirely.

## When Zero-Copy Matters

The math is simple. A modern CPU does ~10GB/s memcpy. If you're pushing 1Gbps (125MB/s), copies consume ~1.25% of one core. Not nothing, but manageable.

At 10Gbps (1.25GB/s), copies consume 12.5% of a core just for shuffling bytes. At 100Gbps, it's physically impossible without zero-copy — you'd need more than one entire core just for memcpy.

**Use zero-copy when:**
- Serving static files: `sendfile`
- Proxying: `splice`
- Sending large dynamically-built buffers: `MSG_ZEROCOPY`
- High-throughput I/O: io_uring with registered buffers

**Skip it when:** the data is small (<4KB), you need to transform it in user space, or your bottleneck is elsewhere. Premature optimization of memory copies is real.
