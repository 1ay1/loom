---
title: "How I Ported Claude Code's Rendering Engine to Modern C++"
date: 2026-04-01
slug: claude-rendering-engine
tags: c++23, terminal-rendering, reverse-engineering, tui, claude-code, ansi
excerpt: I decompiled Claude Code's binary, reverse-engineered its terminal rendering pipeline, and rebuilt the whole thing in C++23. This is the story of what I found inside — and how to build a flicker-free, 60fps terminal UI from scratch.
---

# How I Ported Claude Code's Rendering Engine to Modern C++

## Twelve parts. One terminal. Zero flicker.

I wanted to understand how Claude Code renders its terminal UI. So I did what any reasonable person would do: I decompiled the Bun-compiled binary, traced through thousands of lines of minified JavaScript, reverse-engineered the entire rendering pipeline, and then rebuilt it from scratch in C++23.

What I found was genuinely surprising. Claude Code doesn't just `printf` its way through terminal output like most CLI tools. It runs a *real rendering engine* — the kind of architecture you'd find in a game engine or GPU compositor. Double-buffered screen buffers. Interned styles with pre-computed transition caches. Damage-tracked differential rendering. Hardware scroll regions. The works.

This series documents the whole journey: what I found in the binary, why it works the way it does, and how I redesigned it in modern C++ with phantom types, `constexpr` evaluation, zero-cost abstractions, and compile-time guarantees that JavaScript can never provide.

---

## The Series

### Part 1: [The Problem — Why Terminals Are Broken](/post/the-problem-terminals-are-broken)
I found a race condition in my own code. Two threads, one stdout, total corruption. This is the bug that sent me down the rabbit hole — and it turns out every serious TUI has to solve the same fundamental problem.

### Part 2: [The Screen Buffer — A 2D Cell Grid](/post/screen-buffer-2d-cell-grid)
Every pixel on a terminal is a *cell*. Claude Code stores them in a flat array with a dual-view memory trick that makes comparison blazing fast. Our C++ version does the same thing — without the trick.

### Part 3: [Cell Packing — Fitting a Universe into 8 Bytes](/post/cell-packing-eight-bytes)
How do you fit a character, a style reference, a width flag, and room for future expansion into exactly 8 bytes? Claude Code uses bitfield packing. We use struct layout. Both compile down to a single `cmp` instruction.

### Part 4: [Interning Pools — Strings Are Integers Now](/post/interning-pools-strings-are-integers)
The moment you intern a style, comparison becomes integer equality. The moment you cache the *transition* between two styles, rendering becomes a hash table lookup. This is where the real magic happens.

### Part 5: [The Output Builder — Recording Before Rendering](/post/output-builder-recording-before-rendering)
Claude Code doesn't render directly. It *records* what it wants to do — blit, write, clear, clip, shift — then replays the recording onto the screen buffer. This decoupling unlocks optimizations that immediate-mode rendering can't touch.

### Part 6: [The Render Tree Walk — From Layout to Cells](/post/render-tree-walk-layout-to-cells)
The `renderNode` function is the most complex piece in the pipeline. It walks the component tree, decides what to re-render and what to skip, and orchestrates the entire frame. We extracted it from the minified binary, and it's beautiful.

### Part 7: [The Diff Engine — Only Paint What Changed](/post/diff-engine-only-paint-what-changed)
The heart of the renderer: compare two screen buffers, emit the minimal ANSI to transform one into the other. Every optimization upstream exists to make this loop's job easier. Every optimization in this loop directly reduces bytes to stdout.

### Part 8: [Style Transitions — The Transition Cache](/post/style-transitions-transition-cache)
Computing the minimal ANSI to go from style A to style B. Caching the result forever. This single optimization saves 30-50% of terminal output and turns the diff engine's inner loop into a hash map lookup.

### Part 9: [Hardware Scroll — Moving Rows Without Rewriting](/post/hardware-scroll-moving-rows)
The terminal can shift rows for you. You just have to ask nicely (with CSI sequences), and then lie to your own diff engine about what the previous frame looked like. 99% reduction in scroll I/O.

### Part 10: [Double Buffering and Frame Lifecycle](/post/double-buffering-frame-lifecycle)
Two buffers. Swap on every frame. Clear, render, replay, diff, emit, swap. The lifecycle of a single frame, from event to pixel, in ~230 microseconds.

### Part 11: [Thread Safety — The Compositor](/post/thread-safety-compositor)
The spinner thread and the main thread both want stdout. They can't have it. The Compositor is the bouncer: one lock, one writer, zero corruption. Plus: the self-pipe trick, mutable regions, and why the lock must cover the `write()` syscall.

### Part 12: [Putting It All Together — The Full Pipeline](/post/full-pipeline-putting-it-together)
The complete architecture. The `main.cpp` that wires it all. What changed from the broken version to the working one. Performance numbers. Future directions. And what I learned from reading someone else's rendering engine.

---

## The Design Philosophy

Six principles emerged from studying Claude Code's renderer:

1. **Intern everything, compare nothing.** Strings are expensive. Integers are free. Convert every string and style into an integer ID, and suddenly comparison is a single `cmp` instruction.

2. **Cache the transition, not the result.** Don't cache what a style *looks like*. Cache what it takes to *get there from somewhere else*. The transition cache is the highest-leverage optimization in the entire pipeline.

3. **Track damage, don't scan the world.** Only diff the cells that could have changed. Damage rectangles bound the work per frame. Everything else is skipped at full memory bandwidth.

4. **Record first, render later.** The output builder pattern decouples "what to render" from "how to render." This unlocks blit (reuse previous frame), clip (nested scroll regions), and shift (hardware scroll) — optimizations that are impossible with immediate-mode rendering.

5. **Types are free documentation that the compiler enforces.** Phantom-tagged IDs, `constexpr` style builders, `static_assert` on struct sizes. The C++ type system catches an entire class of bugs that JavaScript tests can only hope to find at runtime.

6. **The terminal is a GPU. Treat it like one.** Scroll regions, synchronized output, cursor positioning — these are hardware features. Use them instead of rewriting entire screens character by character.
