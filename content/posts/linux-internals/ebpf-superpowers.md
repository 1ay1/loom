---
title: eBPF: Programmable Kernel Superpowers
date: 2024-07-15
slug: ebpf-superpowers
tags: linux, performance, observability, networking
excerpt: eBPF lets you run sandboxed programs inside the Linux kernel — tracing, networking, security, all without kernel modules.
---

The Linux kernel used to be a black box. You could read `/proc`, attach kprobes if you were brave, or write a kernel module if you were reckless. eBPF changed everything.

## What Is eBPF?

eBPF (extended Berkeley Packet Filter) is a virtual machine inside the Linux kernel. You write small programs in a restricted C subset, compile them to eBPF bytecode, and the kernel's verifier proves they're safe before running them at native speed via JIT compilation.

No kernel modules. No reboots. No crashing the box.

## The Verification Step

This is what makes eBPF fundamentally different from kernel modules:

1. **Bounded loops**: The verifier proves every loop terminates
2. **Memory safety**: All pointer accesses are bounds-checked
3. **No arbitrary kernel memory access**: Only approved helper functions
4. **Stack size limit**: 512 bytes per program

If your program can't be proven safe, it doesn't load. Period.

## Tracing: See Everything

Attach eBPF programs to tracepoints, kprobes, and uprobes to observe the kernel in real time:

```cpp
// Trace every TCP connection
SEC("tracepoint/sock/inet_sock_set_state")
int trace_tcp_state(struct trace_event_raw_inet_sock_set_state* ctx)
{
    if (ctx->newstate != TCP_ESTABLISHED)
        return 0;

    struct event e = {};
    e.pid = bpf_get_current_pid_tgid() >> 32;
    e.sport = ctx->sport;
    e.dport = ctx->dport;
    bpf_get_current_comm(&e.comm, sizeof(e.comm));

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &e, sizeof(e));
    return 0;
}
```

This runs *inside* the kernel, on every TCP state transition, with zero overhead when nothing matches. Tools like `bpftrace`, `bcc`, and Cilium are built on this.

## Networking: XDP

eXpress Data Path runs eBPF at the earliest point in the network stack — before the kernel allocates an `sk_buff`. This means you can:

- Drop DDoS packets at line rate
- Load balance without touching iptables
- Redirect packets between interfaces

Facebook uses XDP to handle all incoming traffic. Cloudflare uses it for DDoS mitigation. It's not experimental anymore.

## The Performance Story

Traditional tracing tools (strace, ltrace) use `ptrace`, which context-switches on every syscall. eBPF runs in-kernel:

- **strace overhead**: 100-1000x slowdown
- **eBPF overhead**: 1-5% for most programs, often unmeasurable
- **XDP packet processing**: millions of packets per second per core

## Security: LSM Hooks

eBPF can attach to Linux Security Module hooks to enforce security policies:

- Block specific file accesses per-container
- Restrict network connections by process
- Audit syscall patterns in real time

This is how modern container runtimes are starting to enforce security policies — no kernel patches needed.

## When to Use eBPF

**Use it for**: observability, network filtering, security enforcement, performance profiling, custom metrics.

**Don't use it for**: general-purpose compute, anything that needs large memory allocations, complex data structures, or long-running computations. The 512-byte stack and verification constraints are real limits.

eBPF is the most significant Linux kernel innovation of the last decade. If you're doing systems work on Linux, it's worth learning.
