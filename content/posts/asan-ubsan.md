---
title: AddressSanitizer and UBSan Saved My Code
date: 2024-01-25
slug: asan-ubsan
tags: cpp, debugging, tools
excerpt: If you write C or C++ without sanitizers, you're debugging with a blindfold. ASan and UBSan catch bugs that no amount of testing will find.
---

I spent three days debugging a segfault that only happened under load. Valgrind couldn't reproduce it (too slow — the timing changed). GDB caught it once but the stack was corrupted. Then I compiled with `-fsanitize=address` and got the answer in 2 seconds.

## AddressSanitizer (ASan) TUOP

ASan instruments every memory access to detect:

- **Heap buffer overflow**: reading/writing past allocated memory
- **Stack buffer overflow**: overrunning stack arrays
- **Use-after-free**: accessing freed memory
- **Use-after-return**: accessing stack variables after the function returns
- **Double free**: freeing the same pointer twice
- **Memory leaks**: allocated memory never freed (with leak sanitizer)

Enable it with one flag:

```bash
g++ -fsanitize=address -g -O1 mycode.cpp
```

When it catches a bug, you get a precise report:

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000014
READ of size 4 at 0x602000000014 thread T0
    #0 process_data(int*, int) /src/parser.cpp:42
    #1 main /src/main.cpp:15

0x602000000014 is located 0 bytes to the right of 12-byte region [0x602000000008,0x602000000014)
allocated by thread T0 here:
    #0 malloc
    #1 parse_input /src/parser.cpp:30
```

It tells you exactly what happened, where, and where the memory was allocated. The overhead is ~2x runtime and ~3x memory — fast enough for CI.

## UndefinedBehaviorSanitizer (UBSan)

UBSan catches undefined behavior that compilers are free to exploit in terrifying ways:

- **Signed integer overflow**: `INT_MAX + 1` is UB, not wraparound
- **Null dereference**: `nullptr->method()` isn't just a crash — it's UB
- **Shift overflow**: `1 << 33` on a 32-bit int is UB
- **Misaligned access**: accessing a `uint64_t*` at an odd address
- **Type punning violations**: `*(float*)&int_val` via strict aliasing

```bash
g++ -fsanitize=undefined -g mycode.cpp
```

Example catch:

```
parser.cpp:87:15: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

The insidious thing about UB: it often works in debug builds and breaks in release builds. The optimizer assumes UB never happens and optimizes based on that assumption. `-O2` can turn working code into crashing code if UB is present.

## Combining Sanitizers

You can combine ASan and UBSan:

```bash
g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 code.cpp
```

`-fno-sanitize-recover=all` makes any sanitizer finding abort immediately instead of continuing. This is what you want in CI — fail fast.

## ThreadSanitizer (TSan)

For multithreaded code, TSan detects data races:

```bash
g++ -fsanitize=thread -g code.cpp
```

It catches unsynchronized access to shared data — the class of bug that corrupts data once per million operations and is impossible to reproduce reliably. TSan has ~5-15x overhead, so it's slower, but it catches races that no other tool can find deterministically.

Note: ASan and TSan can't be used together. Run them in separate CI jobs.

## The CI Pipeline

Every C/C++ project should have:

```yaml
# Build and test with sanitizers
sanitizer-test:
  - g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -O1
  - ./run_tests

race-test:
  - g++ -fsanitize=thread -g -O1
  - ./run_tests
```

This catches more bugs than code review, static analysis, and manual testing combined. If you're writing C or C++ without sanitizers in CI, you have bugs you don't know about. Guaranteed.
