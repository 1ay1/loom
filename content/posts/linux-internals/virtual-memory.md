---
title: Virtual Memory Is the Greatest Lie Your CPU Tells
date: 2024-05-05
slug: virtual-memory
tags: linux, systems, performance
excerpt: Every process thinks it owns 128TB of contiguous memory. The MMU, page tables, and TLB make this illusion work at nearly zero cost.
---

Every process on a 64-bit Linux system sees a flat 128TB address space, starting at 0x0. No process knows about any other process's memory. This is a complete fiction maintained by hardware and kernel working together.

## The Illusion

When your program does `int* p = new int[1000]`, here's what doesn't happen:
- The kernel doesn't find a contiguous block of physical RAM
- No physical memory is allocated at all (usually)
- The returned pointer is a virtual address that maps to... nothing yet

The kernel just updates its page tables to say "this virtual range is valid". Physical pages get allocated on first access via **page faults**.

## Page Tables: The Translation Layer

x86-64 uses a 4-level page table to translate virtual addresses to physical addresses:

```
Virtual Address (48 bits used):
  PML4 index (9 bits) → PDP index (9 bits) → PD index (9 bits) → PT index (9 bits) → Offset (12 bits)

Each level is a table of 512 entries (8 bytes each = 4KB page)
```

A single address translation requires 4 memory lookups. At ~100ns per DRAM access, that's 400ns per translation — roughly 200x slower than an L1 cache hit. This would be catastrophic.

## TLB: The Cache That Saves Everything

The Translation Lookaside Buffer is a small, fast cache of recent virtual-to-physical translations. On modern Intel CPUs:

- **L1 dTLB**: 64 entries, ~1 cycle lookup
- **L1 iTLB**: 128 entries, ~1 cycle lookup
- **L2 TLB**: 1536 entries, ~7 cycles

TLB hit rate for most workloads: **99.9%+**. The page table walk only happens on a TLB miss.

## Huge Pages: When 4KB Isn't Enough

A database with 256GB of RAM needs 67 million page table entries at 4KB per page. The TLB can only cache ~1500 of those. Solution: huge pages.

- **2MB huge pages**: 1 TLB entry covers 512x more memory
- **1GB huge pages**: 1 TLB entry covers 262,144x more memory

```cpp
// Allocate 1GB of huge pages
void* p = mmap(NULL, 1UL << 30, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
```

Databases (PostgreSQL, MySQL), JVMs, and DPDK all use huge pages for this reason. The TLB coverage improvement is often worth 5-15% throughput.

## Copy-on-Write: fork() Is Free

When you `fork()`, the kernel doesn't copy the process's memory. It marks all pages as read-only in both parent and child. When either process writes to a page, the CPU triggers a page fault, and *then* the kernel copies just that one page.

This is why `fork()` is fast even for processes using gigabytes of RAM — most pages are never written and never copied.

Redis uses this for background saves: `fork()`, iterate all keys in the child (read-only, no copies), write the RDB file, exit. The parent keeps serving requests. Writes during the save cause copy-on-write for just the modified pages.

## Demand Paging: malloc Lies to You

`malloc(1GB)` returns instantly, even if the system only has 4GB free. The kernel doesn't allocate physical pages until you touch them. This is **overcommit**.

```cpp
// This "allocates" 1TB instantly
char* p = (char*)malloc(1UL << 40);
// This actually allocates one 4KB page
p[0] = 42;
// This allocates another 4KB page, 1TB away in virtual space
p[(1UL << 40) - 1] = 42;
```

Only 8KB of physical RAM is used, regardless of the 1TB virtual allocation.

## The OOM Killer

Overcommit has a dark side. If all processes simultaneously touch their allocated memory and physical RAM runs out, the kernel invokes the OOM killer. It picks a process to kill based on memory usage, oom_score_adj, and other heuristics.

If you're running a database, set `oom_score_adj` to -1000. If you're running a web cache, accept that the OOM killer might visit.

## mmap: Files as Memory

`mmap` maps a file into virtual address space. Reading the mapped memory triggers page faults that load file data on demand. The kernel uses the page cache, so frequently accessed regions stay in RAM.

```cpp
int fd = open("data.bin", O_RDONLY);
void* p = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
// Access file data as if it were an array
int value = ((int*)p)[42];
```

This is how most databases access their data files. No `read()` syscalls, no buffer management code — the kernel's page cache does it all.

Virtual memory is one of those abstractions that's so good, you forget it exists. Until you're debugging a performance problem and need to understand why your 99th percentile latency just spiked — probably a TLB miss storm or transparent huge page compaction. Then you need to understand the lie.
