---
title: "The Output Builder — Recording Before Rendering"
date: 2026-03-11
slug: output-builder-recording-before-rendering
tags: c++23, output-builder, variant, blit-optimization, damage-tracking, claude-code
excerpt: "Claude Code doesn't render directly. It records what it wants to do, then replays the recording. This decoupling is what makes blit, clip, and scroll optimizations possible — and it's the reason 95% of the screen costs zero work per frame."
---

There's a moment in every rendering engine's evolution where you stop writing pixels directly and start *recording instructions* instead. In GPU rendering, this is the command buffer. In React, this is the virtual DOM. In Claude Code's terminal renderer, it's the Output builder.

The idea: instead of writing cells into the screen buffer as you walk the component tree, you record *what you intend to do* — blit this rectangle, write this text, clear this region, clip to this box. Then, in a separate pass, you replay all the recorded operations onto the actual buffer.

This sounds like unnecessary indirection. It's actually the most important architectural decision in the entire renderer.

## Why Not Write Directly?

The naive approach — immediate mode rendering, where each component writes cells directly into the screen buffer — fails for three reasons:

**1. The blit optimization becomes impossible.** You can't decide "this subtree hasn't changed, copy it from the previous frame" if you're already in the middle of writing cells. You need to record the *intent* to copy, then replay it. The decision to blit happens during the tree walk; the actual copy happens during replay.

**2. Clip regions require nesting.** A scrollable container clips its children to its bounding box. With immediate rendering, you'd need to check bounds on every single cell write — an `if` statement in the innermost loop. With recorded operations, you push a clip rect before children and pop it after. The replay engine handles clipping in one place.

**3. Scroll deltas need ordered replay.** When a scrollable region scrolls, you need to shift existing cells *before* writing new content into the vacated rows. Recording operations lets you insert the shift at exactly the right point in the sequence.

## Claude Code's Output Builder

Here's what I extracted from the binary:

```js
class Output {
    operations = [];
    width;

    blit(src, x, y, width, height) {
        this.operations.push({ type: "blit", src, x, y, width, height });
    }

    write(x, y, text) {
        if (!text) return;
        this.operations.push({ type: "write", x, y, text });
    }

    clear(region) {
        this.operations.push({ type: "clear", region });
    }

    clip(clip) {
        this.operations.push({ type: "clip", clip });
    }

    unclip() {
        this.operations.push({ type: "unclip" });
    }

    shift(top, bottom, n) {
        this.operations.push({ type: "shift", top, bottom, n });
    }

    noSelect(region) {
        this.operations.push({ type: "noSelect", region });
    }
}
```

Seven operation types. That's the entire vocabulary the render tree uses to describe a frame. Seven words to express any terminal UI update.

| Operation | What it does |
|-----------|-------------|
| `write(x, y, text)` | Write styled text at position (x, y) into the buffer |
| `blit(src, x, y, w, h)` | Copy a rectangle from the previous screen — skip re-rendering |
| `clear(region)` | Zero out a rectangular region of cells |
| `clip(rect)` | Push a clipping rectangle — subsequent writes are masked |
| `unclip()` | Pop the clipping rectangle |
| `shift(top, bot, n)` | Shift rows within [top, bot) by n rows (hardware scroll) |
| `noSelect(region)` | Mark region as non-selectable (terminal selection hint) |

### Replay Order Is Everything

Operations are replayed in recording order. Get the order wrong and the output is wrong:

```
1. clear(old_region)        <-- erase where a node used to be
2. shift(top, bot, delta)   <-- scroll existing content
3. clip(container_rect)     <-- constrain children to container
4. write(x, y, new_text)    <-- write new content into scrolled area
5. blit(prev, x, y, w, h)  <-- reuse unchanged subtree output
6. unclip()                 <-- restore full-screen writes
```

The shift must happen before the write (so the write goes into the vacated rows). The clip must happen before child writes (so they're bounded). The clear must happen before anything else (so stale content is removed). The tree walk naturally produces this order by visiting nodes top-down, but the replay engine depends on it.

## The Blit Optimization: The Single Biggest Win

This is the optimization that makes Claude Code's UI feel instant. From the `renderNode` function I extracted:

```js
// If node is clean, hasn't moved, and previous screen exists:
if (!node.dirty && node.pendingScrollDelta === undefined
    && prevLayout && prevLayout.x === x && prevLayout.y === y
    && prevLayout.width === w && prevLayout.height === h
    && prevScreen) {
    output.blit(prevScreen, Math.floor(x), Math.floor(y),
                 Math.floor(w), Math.floor(h));
    return;  // SKIP ENTIRE SUBTREE
}
```

Read that `return` statement carefully. If a node is clean (no state change), hasn't moved, and the previous frame exists — the entire subtree is **never visited**. We just record "copy this rectangle from last frame" and move on.

Think about what this means for a typical Claude Code session:

- A 500-line conversation history that hasn't changed: **zero work per frame**
- A status bar that updates every 80ms: only that 1-row region is re-rendered
- A scrolling code block: shift the existing lines, write only the new ones, blit the rest

The blit replays as a `memcpy` from the previous screen's cells to the current screen's cells for the given rectangle. In C++, this compiles to `memmove`, which is SIMD-optimized. In practice, a frame where the user is watching streaming output re-renders maybe 5% of the screen. The other 95% is blitted at memory-copy speed.

## Our C++ Design: Type-Safe Operations

We model the output builder using `std::variant` — a type-safe tagged union:

```cpp
namespace op {
    struct Write {
        int x, y;
        std::span<const StyledChar> text;
    };

    struct Blit {
        const ScreenBuf* src;
        int x, y, w, h;
    };

    struct Clear {
        int x, y, w, h;
    };

    struct Clip {
        int x, y, w, h;
    };

    struct Unclip {};

    struct Shift {
        int top, bot, delta;
    };

    using Op = std::variant<Write, Blit, Clear, Clip, Unclip, Shift>;
}

class OutputBuilder {
public:
    void write(int x, int y, std::span<const StyledChar> text);
    void blit(const ScreenBuf& src, int x, int y, int w, int h);
    void clear(int x, int y, int w, int h);
    void clip(int x, int y, int w, int h);
    void unclip();
    void shift(int top, int bot, int delta);

    // Replay all recorded operations onto the target buffer
    void replay(ScreenBuf& target, const StylePool& styles) const;

    void reset() { ops_.clear(); }

private:
    std::vector<op::Op> ops_;
    std::vector<Rect> clip_stack_;  // for nested clip regions
};
```

### Why `std::variant` Over a Tagged Union

Claude Code uses a plain JavaScript object with a `type` string field. We could do the same with a C-style tagged union. But `std::variant` gives us three things that a tagged union doesn't:

1. **Exhaustive matching.** `std::visit` with a visitor that handles all alternatives — the compiler errors if you forget one. Add a new operation type and every `visit` site in the codebase lights up red until you handle it.

2. **No manual tag management.** The variant tracks its active type. No `switch` on an enum with a dangling `default` case that silently swallows bugs.

3. **Value semantics.** Operations are stored inline in the vector. No heap allocation per operation. The variant's size is the max of its alternatives (in our case, `Blit` at ~24 bytes) plus a small discriminator. All operations live contiguously in memory.

### The Replay Engine

```cpp
void OutputBuilder::replay(ScreenBuf& target,
                           const StylePool& styles) const {
    std::vector<Rect> clip_stack;

    for (const auto& op : ops_) {
        std::visit(overloaded{
            [&](const op::Write& w) {
                int col = w.x;
                for (const auto& sc : w.text) {
                    if (!in_clip(clip_stack, col, w.y)) {
                        ++col;
                        continue;
                    }
                    target.put(w.y, col, sc.ch,
                               styles.intern(sc.style), sc.width);
                    col += sc.width;
                }
            },
            [&](const op::Blit& b) {
                for (int r = 0; r < b.h; ++r) {
                    auto src_row = b.src->row(b.y + r);
                    for (int c = 0; c < b.w; ++c) {
                        if (!in_clip(clip_stack, b.x + c, b.y + r))
                            continue;
                        target.at(b.y + r, b.x + c) = src_row[b.x + c];
                    }
                }
            },
            [&](const op::Clear& cl) {
                SCell blank{};
                for (int r = cl.y; r < cl.y + cl.h; ++r)
                    target.fill(r, cl.x, cl.w, U' ',
                                StyleId{target.at(r, cl.x).style_id});
            },
            [&](const op::Clip& c) {
                clip_stack.push_back({c.x, c.y, c.w, c.h});
            },
            [&](const op::Unclip&) {
                if (!clip_stack.empty()) clip_stack.pop_back();
            },
            [&](const op::Shift& s) {
                target.scroll_up(s.top, s.bot, s.delta);
            },
        }, op);
    }
}
```

The `overloaded` helper is the standard C++ pattern for creating a visitor from multiple lambdas — a struct that inherits from all of them:

```cpp
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
```

Three lines. Lets you write inline visitors without defining a separate class. This is C++17 at its most ergonomic.

### The Clip Stack

Clips nest. A scrollable container inside a panel means two active clip rects. A cell write only proceeds if the position is inside *all* active clips:

```cpp
bool in_clip(const std::vector<Rect>& stack, int x, int y) {
    for (const auto& r : stack)
        if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h)
            return false;
    return true;
}
```

In practice, clip depth rarely exceeds 2. The linear scan is negligible. If someone builds a TUI with 50 nested scroll containers, they have bigger problems than clip performance.

## Damage Tracking: Blit Is Free

When operations are replayed, they implicitly define a **damage rectangle** — the bounding box of all cells that were actually modified. The replay engine tracks this:

```cpp
void OutputBuilder::replay(ScreenBuf& target, const StylePool& styles) const {
    Rect damage = Rect::empty();

    for (const auto& op : ops_) {
        std::visit(overloaded{
            [&](const op::Write& w) {
                damage = damage.union_with({w.x, w.y, /* width */, 1});
                // ... write cells ...
            },
            [&](const op::Clear& cl) {
                damage = damage.union_with({cl.x, cl.y, cl.w, cl.h});
                // ... clear cells ...
            },
            [&](const op::Blit& b) {
                // Blit does NOT expand damage -- content is unchanged
            },
            // ...
        }, op);
    }

    target.set_damage(damage);
}
```

The critical insight: **blit operations do not contribute to damage.** The cells they copy are identical to the previous frame by definition. The diff engine can skip them entirely.

This means a frame where 95% of the screen is blitted and 5% is newly written produces a damage rect covering only 5% of the cells. The diff engine runs in O(damage area), not O(screen area). On a typical streaming-output frame, that's maybe 100 cells out of 4800. The diff is *fast*.

## How It All Connects

```
Render Tree Walk
    |
    |-- clean node -> output.blit(prevScreen, rect)
    |-- moved node -> output.clear(old) + recurse
    |-- text node  -> output.write(x, y, styled_text)
    |-- scroll     -> output.clip() + output.shift() + recurse + output.unclip()
    |
    v
OutputBuilder (vector of op::Op variants)
    |
    |  replay()
    v
ScreenBuf (flat cell array with damage rect)
    |
    |  diff against prevScreen
    v
ANSI output (minimal escape sequences)
```

The output builder is the decoupling layer between intent and execution. The tree walk says *what* should be on screen. The replay engine says *how* to make it happen. And the damage rect tells the diff engine *where* to look.

This separation of concerns is what makes the blit, clip, and scroll optimizations composable. You can blit a subtree *inside* a clip region *after* a scroll — and the replay engine handles the interactions correctly because it processes operations in sequence, with the clip stack tracking what's visible.

Without the output builder, you'd need every component to know about clipping, scrolling, and frame reuse. With it, components just record their intentions. The complexity lives in one place.

---

**Next: [Part 6 — The Render Tree Walk](/post/render-tree-walk-layout-to-cells)**
