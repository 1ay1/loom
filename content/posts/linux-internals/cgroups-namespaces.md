---
title: Cgroups and Namespaces — How Containers Actually Work
date: 2024-06-20
slug: cgroups-namespaces
tags: linux, containers, systems
excerpt: Containers aren't VMs. They're just cgroups + namespaces + a clever filesystem trick. Here's what's actually happening.
---

Every time someone says "containers are lightweight VMs", a kernel developer loses a year of their life. Containers are processes. They just can't see or touch anything outside their sandbox.

Two kernel features make this work: **namespaces** (isolation) and **cgroups** (resource limits).

## Namespaces: What You Can See

A namespace wraps a global system resource so that processes inside the namespace think they have their own isolated instance. Linux has 8 namespace types:

| Namespace | Isolates |
|-----------|----------|
| `pid` | Process IDs |
| `net` | Network stack |
| `mnt` | Mount points |
| `uts` | Hostname |
| `ipc` | IPC objects |
| `user` | UID/GID mappings |
| `cgroup` | Cgroup root |
| `time` | Boot/monotonic clocks |

Creating a new namespace is one syscall:

```cpp
// Create a new PID namespace for a child process
int flags = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS;
pid_t pid = clone(child_fn, stack + STACK_SIZE, flags | SIGCHLD, NULL);
```

The child process gets PID 1 in its namespace. It can't see host processes. It has its own network stack with its own interfaces and routing table. Its mount table is independent.

## Cgroups: What You Can Use

Namespaces control visibility. Cgroups control consumption. A cgroup limits how much CPU, memory, I/O, and network a group of processes can use.

Cgroups v2 (unified hierarchy) looks like this:

```
/sys/fs/cgroup/
  my-container/
    cgroup.procs          # PIDs in this group
    memory.max            # Memory limit in bytes
    memory.current        # Current usage
    cpu.max               # CPU quota (e.g., "50000 100000" = 50%)
    io.max                # I/O bandwidth limits
```

Writing to these files is the entire API:

```cpp
// Limit a cgroup to 512MB memory and 50% of one CPU
write_file("/sys/fs/cgroup/myapp/memory.max", "536870912");
write_file("/sys/fs/cgroup/myapp/cpu.max", "50000 100000");
write_file("/sys/fs/cgroup/myapp/cgroup.procs", std::to_string(pid));
```

When a process hits its memory limit, the OOM killer targets only processes in that cgroup. No collateral damage.

## The Filesystem Layer

The third ingredient is `pivot_root` (or `chroot`). Give the container its own root filesystem — typically an overlay of read-only image layers plus a writable top layer:

```
overlay mount:
  lower = image layers (read-only)
  upper = container writes
  merged = what the container sees
```

This is why container images are layered and why pulls only download missing layers. It's also why containers start instantly — there's nothing to boot, just `pivot_root` and `exec`.

## Putting It Together

What Docker/Podman/containerd actually do:

1. `clone()` with `CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUTS`
2. Set up cgroup limits (memory, CPU, I/O)
3. Mount the overlay filesystem
4. `pivot_root` into it
5. Drop capabilities, set seccomp filters
6. `exec` the container entrypoint

That's it. No hypervisor, no virtual hardware, no guest kernel. Just a process that can't see or touch anything it shouldn't.

## Why This Matters

Understanding the primitives matters when things go wrong:

- Container networking issues? It's namespace + veth pair configuration
- OOM kills? Check cgroup memory limits vs actual usage
- File permission problems? User namespace UID mapping
- "Works on my machine"? The overlay filesystem layers differ

Containers aren't magic. They're three well-understood kernel features composed together.
