---
title: "The Problem — Why Terminals Are Broken"
date: 2026-03-03
slug: the-problem-terminals-are-broken
tags: c++23, terminal, tui, ansi-escape, race-condition, claude-code
excerpt: Two threads. One stdout. Total corruption. This is the bug that sent me reverse-engineering Claude Code's binary — and the fundamental reason every serious TUI needs a rendering engine.
---

Every programmer's first terminal UI looks something like this:

```cpp
void show_status(const char* msg) {
    printf("\033[1;1H");      // move to top-left
    printf("\033[2K");         // clear line
    printf("\033[1m%s\033[0m", msg);  // bold text
    fflush(stdout);
}

void show_spinner() {
    while (running) {
        show_status("⠋ Thinking...");
        usleep(80000);
        show_status("⠙ Thinking...");
        usleep(80000);
    }
}
```

Looks fine. Works fine. Ship it.

Then you add a second thread.

## The Bug That Started Everything

Your spinner thread and your main output thread are both calling `write(STDOUT_FILENO, ...)`. Their ANSI escape sequences interleave *mid-sequence*:

```
Thread A writes: \033[38;2;137;180;
Thread B writes: \033[1;1H\033[2K
Thread A writes: 250m⠋ Thinking...
```

The terminal sees `\033[38;2;137;180;\033[1;1H` — a malformed escape sequence that no terminal knows how to parse. The result: garbled colors, the cursor jumping to wrong positions, phantom characters that shouldn't exist. The terminal is **corrupted**.

This is not a hypothetical. This is the exact bug I found in my own codebase. The presenter's spinner thread (`presenter.cpp:62-117`) was doing `::write(STDOUT_FILENO, ...)` every 80ms while the main thread wrote through a buffered renderer. Classic race condition. Terminal corruption on every other run.

I stared at the garbled output for a while, fixed the immediate bug with a mutex, and then asked myself: *how does Claude Code handle this?* Its terminal UI is gorgeous — styled markdown, syntax-highlighted code blocks, animated spinners, tool call cards — and I've never seen it flicker. Not once.

So I decompiled it.

## The Fundamental Issue

Here's the thing about terminals that makes this problem so nasty: **terminals are character-stream devices.** They interpret a linear sequence of bytes as a mix of printable characters and control sequences. There is no framing. There are no transaction boundaries. If two writers interleave their bytes, the terminal's state machine sees garbage.

This is unlike a GPU framebuffer where you can write pixels at arbitrary positions independently. In a terminal:

- **Cursor position is global state.** Moving the cursor in one thread affects all subsequent output from all threads.
- **Style state is global.** Setting bold in one escape sequence affects all subsequent characters until reset.
- **Escape sequences are multi-byte.** A partial escape sequence is indistinguishable from data corruption.

You can't fix this with smaller writes. You can't fix it with clever buffering. The *only* fix is architectural: you need a rendering engine between your application and stdout.

## What I Found Inside Claude Code

Every production terminal UI solves this the same way. When I dug into Claude Code's binary, I found all four pillars of the solution:

### 1. Single Writer

All terminal output goes through **one** codepath that owns stdout. No thread may write to the terminal directly. Period. Not the spinner. Not the input handler. Not the error reporter. Nobody.

### 2. Buffered Output

Output is accumulated into a buffer, then flushed in a single syscall:

```cpp
// Bad: N syscalls, interruptible between any two
write(fd, "\033[1;1H", 6);
write(fd, "\033[2K", 4);
write(fd, "hello", 5);

// Good: 1 syscall, atomic from the terminal's perspective
std::string buf;
buf += "\033[1;1H";
buf += "\033[2K";
buf += "hello";
write(fd, buf.data(), buf.size());
```

One `write()` call. One chance for interleaving. The kernel may still split a large write internally, but the next pillar handles that.

### 3. Synchronized Output

Modern terminals support the **synchronized output** protocol (DEC private mode 2026). This tells the terminal to buffer all incoming bytes and display them as a single atomic frame:

```
\033[?2026h   <-- start synchronized update
...all your ANSI output...
\033[?2026l   <-- end synchronized update, display frame
```

Even if the kernel splits your `write()` call across multiple pty reads, the terminal holds everything until it sees the end marker. No mid-frame flicker. Kitty, WezTerm, iTerm2, Ghostty, Windows Terminal — they all support this. Terminals that don't recognize the sequence ignore it harmlessly.

### 4. Differential Rendering

This is the real insight. The most critical optimization: **don't redraw what hasn't changed.**

If your status bar says "Thinking..." and it still says "Thinking...", emit zero bytes for it. If one character changed in a 200-column line, emit a cursor move to that position and write just that character. Don't clear and redraw. Don't even think about clear and redraw.

This is where the rendering engine comes in.

## The Rendering Engine

A terminal rendering engine is the layer between "what should be on screen" and "what bytes to write to stdout." Its responsibilities:

1. Maintain a **screen buffer** — a 2D grid of cells representing the desired terminal state.
2. Compare the current buffer against the **previous frame's buffer**.
3. Emit the **minimal set of ANSI escape sequences** to transform the terminal from the previous state to the current state.
4. Do all of this **atomically** and **thread-safely**.

Claude Code's rendering engine does exactly this. It maintains two screen buffers (current and previous), diffs them cell by cell, and emits only the changed cells with minimal cursor movement and style transitions. The architecture I found:

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Presenter   │───>│ Compositor  │───>│  Terminal    │
│ (what to     │    │ (diff +     │    │  (stdout)    │
│  display)    │    │  serialize) │    │              │
└─────────────┘    └─────────────┘    └─────────────┘
       │                  │
       │                  ├── Screen Buffer (current)
       │                  ├── Screen Buffer (previous)
       │                  ├── StylePool (interned styles)
       │                  ├── CharPool (interned strings)
       │                  └── Mutex (thread safety)
       │
       ├── Markdown Renderer -> RenderedLines
       ├── Status Bar (cell-based)
       ├── Spinner (mutable region)
       └── Box Drawing (tool cards, diffs, permissions)
```

The key insight: the **Compositor** is the single point of serialization. Every thread writes through it. It diffs, serializes, and flushes. The terminal never sees interleaved output.

## The Performance Bar

Claude Code renders at interactive speed with a ~16ms frame budget. The numbers I extracted from profiling:

- **Cell comparison**: 1 integer comparison (8 bytes via `BigInt64Array` / `uint64_t`)
- **Style transition**: 1 hash table lookup (cached ANSI string)
- **Unchanged cell**: 0 bytes emitted
- **Cursor skip**: 1 escape sequence (~6 bytes) vs rewriting N spaces

Our C++ implementation should be significantly faster than JS because of the things C++ is fundamentally good at:
- No GC pauses mid-frame
- Flat memory layout (no object headers, no pointer chasing)
- SIMD-friendly cell arrays (contiguous, aligned, fixed-size)
- Compile-time style computation where possible

## What's Coming

The rest of this series walks through each component of the rendering engine. We start with the lowest-level primitive (the cell) and build up through the screen buffer, interning pools, output builder, diff engine, and finally the compositor that ties it all together.

By the end, you'll understand exactly how a production terminal UI works — not the simplified "just use ncurses" version, but the real thing. The architecture that runs inside Claude Code right now, every time you ask it a question.

Let's start with the atom of terminal rendering: the cell.

---

**Next: [Part 2 — The Screen Buffer](/post/screen-buffer-2d-cell-grid)**
