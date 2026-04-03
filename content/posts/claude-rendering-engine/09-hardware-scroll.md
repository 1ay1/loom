---
title: "Hardware Scroll — Moving Rows Without Rewriting"
date: 2026-04-01
slug: hardware-scroll-moving-rows
tags: c++23, hardware-scroll, csi-sequences, scroll-region, memmove, terminal-rendering, claude-code
excerpt: "The terminal can shift rows for you. You just have to ask nicely with CSI sequences, and then lie to your own diff engine about what the previous frame looked like. Result: 99% reduction in scroll I/O."
---

A coding agent TUI scrolls constantly. Every streamed token pushes content up. Every user scroll-back moves content down. Without optimization, scrolling a 40-row content area by 1 line means the diff engine sees 39 rows that have "changed" — even though those rows are identical, just shifted by one position.

Hardware scroll eliminates this problem entirely. The idea is beautifully devious: tell the terminal to shift its displayed rows (a near-instant operation inside the terminal emulator), then update your *previous frame buffer* to match what the terminal now shows, so the diff engine sees the rows as unchanged.

Two levels work together:
1. **Terminal hardware scroll**: CSI sequences that shift displayed rows
2. **Buffer-level cell shifting**: Moving cells in the screen buffer to match

## Terminal Scroll Regions

The terminal supports **scroll regions** — a vertical range of rows that can be scrolled independently:

```
CSI top;bottom r     Set scroll region to rows [top, bottom]
CSI n S              Scroll region up by n lines
CSI n T              Scroll region down by n lines
CSI r                Reset scroll region to full screen
```

When a scroll region is active, `CSI S` shifts the content within that region upward: the top n rows disappear, remaining rows move up, and n blank rows appear at the bottom:

```
Before:        CSI 1 S         After:
+-- status --+   (scroll up 1)  +-- status --+
| line 1     |                   | line 2     |
| line 2     |   ----------->   | line 3     |
| line 3     |                   | line 4     |
| line 4     |                   |            |  <-- blank
+-- input ---+                   +-- input ---+
```

The terminal does this at display speed — it's a buffer operation inside the terminal emulator, not a byte-stream operation. No characters need to be sent for the 3 shifted rows. The terminal just rearranges its internal cell grid.

This is what I mean when I say "the terminal is a GPU." It has hardware operations. Use them.

## What I Found in Claude Code

### Buffer-Level Scroll: `dJT`

Claude Code shifts cells using `BigInt64Array.copyWithin`:

```js
function dJT(screen, top, bottom, delta) {
    if (delta === 0 || top < 0 || bottom >= screen.height || top > bottom)
        return;
    let { width, cells64, noSelect } = screen;

    if (Math.abs(delta) > bottom - top) {
        cells64.fill(0n, top * width, (bottom + 1) * width);
        noSelect.fill(0, top * width, (bottom + 1) * width);
        return;
    }

    if (delta > 0) {
        // Scroll up: copy [top+delta..bottom] -> [top..bottom-delta]
        cells64.copyWithin(
            top * width,
            (top + delta) * width,
            (bottom + 1) * width
        );
        noSelect.copyWithin(
            top * width,
            (top + delta) * width,
            (bottom + 1) * width
        );
        // Clear vacated rows at bottom
        cells64.fill(0n, (bottom - delta + 1) * width, (bottom + 1) * width);
        noSelect.fill(0, (bottom - delta + 1) * width, (bottom + 1) * width);
    } else {
        // Scroll down: symmetric
        cells64.copyWithin(
            (top - delta) * width,
            top * width,
            (bottom + delta + 1) * width
        );
        noSelect.copyWithin(
            (top - delta) * width,
            top * width,
            (bottom + delta + 1) * width
        );
        cells64.fill(0n, top * width, (top - delta) * width);
        noSelect.fill(0, top * width, (top - delta) * width);
    }
}
```

`copyWithin` is the key. It moves cells within the same buffer without allocating a temporary copy. Under the hood, it's `memmove`, which handles overlapping source and destination correctly.

### The Critical Step: Syncing prevScreen

In the render method, when a scroll hint is present:

```js
if (scrollHint) {
    let { top, bottom, delta } = scrollHint;

    // 1. Tell terminal to scroll
    ops.push({ type: "scrollRegion", top, bottom });
    if (delta > 0)
        ops.push({ type: "scrollUp", n: delta });
    else
        ops.push({ type: "scrollDown", n: -delta });
    ops.push({ type: "scrollRegion", top: 0, bottom: screen.height - 1 });

    // 2. Update prevScreen to match terminal's new state
    dJT(prevScreen, top, bottom, delta);
}
```

Step 2 is the insight that makes the whole thing work. After telling the terminal to scroll, the previous frame buffer must be updated to reflect what the terminal *actually shows*. If you skip this step, the diff engine will compare the pre-scroll prevScreen against the post-scroll currentScreen and think every shifted row has changed. It will re-emit all the shifted content, completely negating the hardware scroll.

You're essentially lying to your own diff engine — telling it "this is what was on screen before" when you've secretly rearranged it. But the lie is exactly right, because the terminal rearranged its display the same way. The diff engine sees truth after the lie.

## Our C++ Implementation

### Buffer Scroll

From Part 2, scroll-up shifts cells within the buffer:

```cpp
void ScreenBuf::scroll_up(int top, int bot, int n) {
    if (n <= 0 || top >= bot) return;
    n = std::min(n, bot - top);

    for (int r = top; r < bot - n; ++r)
        std::copy_n(cells_.data() + (r + n) * w_, w_,
                    cells_.data() + r * w_);
    for (int r = bot - n; r < bot; ++r)
        std::fill_n(cells_.data() + r * w_, w_, SCell{});
}
```

Scroll-down needs reverse iteration to avoid overwriting source data:

```cpp
void ScreenBuf::scroll_down(int top, int bot, int n) {
    if (n <= 0 || top >= bot) return;
    n = std::min(n, bot - top);

    // Copy from bottom to top to avoid overwriting
    for (int r = bot - 1; r >= top + n; --r)
        std::copy_n(cells_.data() + (r - n) * w_, w_,
                    cells_.data() + r * w_);
    for (int r = top; r < top + n; ++r)
        std::fill_n(cells_.data() + r * w_, w_, SCell{});
}
```

Claude Code's `copyWithin` handles the overlap direction automatically. In C++, we handle it explicitly with iteration order. The result is the same: `memmove`-equivalent behavior, SIMD-optimized on modern platforms.

### Terminal Scroll Commands

```cpp
namespace csi {
    inline std::string set_scroll_region(int top, int bot) {
        return std::format("\033[{};{}r", top + 1, bot + 1);
    }
    inline std::string scroll_up(int n) {
        return std::format("\033[{}S", n);
    }
    inline std::string scroll_down(int n) {
        return std::format("\033[{}T", n);
    }
    inline constexpr auto reset_scroll_region = "\033[r";
}
```

### Integration in the Diff Engine

```cpp
void DiffEngine::diff(const ScreenBuf& prev, ScreenBuf& prev_mut,
                      const ScreenBuf& next,
                      const StylePool& styles,
                      std::string& out,
                      const ScrollHint* scroll) {
    // Apply hardware scroll if present
    if (scroll && scroll->delta != 0) {
        int top = scroll->top;
        int bot = scroll->bot;
        int delta = scroll->delta;

        // Tell terminal to scroll
        out += csi::set_scroll_region(top, bot);
        if (delta > 0)
            out += csi::scroll_up(delta);
        else
            out += csi::scroll_down(-delta);
        out += csi::reset_scroll_region;

        // Sync prevScreen with terminal's post-scroll state
        if (delta > 0)
            prev_mut.scroll_up(top, bot + 1, delta);
        else
            prev_mut.scroll_down(top, bot + 1, -delta);
    }

    // Now diff -- most rows match because we synced prevScreen
    // ... cell-by-cell diff as in Part 7 ...
}
```

The `prev_mut` parameter is the previous screen buffer passed as non-const. We need to modify it to match the terminal's post-scroll state. This is the same pattern Claude Code uses — mutable access to the "previous" buffer during the diff phase.

## Scroll Hints: Where Do They Come From?

In Claude Code, scroll hints come from the React component tree — when a scrollable container's `scrollTop` changes, the framework computes the delta.

In our system, the presenter tracks scroll state directly:

```cpp
struct ScrollHint {
    int top, bot, delta;
};

class Presenter {
    int scroll_offset_ = 0;

    std::optional<ScrollHint> compute_scroll_hint(int new_offset) {
        int delta = new_offset - scroll_offset_;
        if (delta == 0) return std::nullopt;

        scroll_offset_ = new_offset;
        return ScrollHint{
            content_region_.y,
            content_region_.y + content_region_.h - 1,
            delta
        };
    }
};
```

New content pushes the scroll offset forward. User scroll-back pulls it backward. Either way, the presenter computes the delta and hands it to the diff engine.

## The Numbers: Why This Matters

For a 120-column, 38-row content area scrolling by 1 line:

**Without hardware scroll:**
- 37 rows appear to have changed (shifted content)
- 37 x 120 = 4440 cell comparisons, all reporting "changed"
- The diff engine emits cursor moves + style transitions + characters for all 4440 cells
- ANSI output: ~20KB

**With hardware scroll:**
- Terminal scroll: ~20 bytes of CSI sequences
- prevScreen buffer update: `memmove` of 37 x 120 x 8 = 35,520 bytes (memory operation, not I/O)
- Diff comparison: 4440 cells, but now ~4400 match (only the 1 new row differs)
- ANSI output for new content: ~200 bytes
- Total ANSI output: ~220 bytes

**Savings: 99% reduction in terminal I/O.**

The `memmove` is essentially free compared to writing 20KB through a pty. Memory bandwidth is ~50GB/s on modern hardware. Pty throughput is maybe 10MB/s. We're trading a sub-microsecond memory operation for 20KB of I/O. It's not even close.

## When Not to Use Hardware Scroll

Hardware scroll is counterproductive in three cases:

1. **Delta exceeds half the region height.** More rows are new than are shifted. A full redraw might be cheaper than scroll + partial redraw.

2. **Multiple non-contiguous regions scroll simultaneously.** Scroll regions are global terminal state — you can only have one active at a time. If two panels scroll independently, you can't use hardware scroll for both in the same frame.

3. **The terminal doesn't support scroll regions.** Ancient terminals or some Windows console implementations may not handle `CSI r` correctly. Claude Code doesn't check for this — it assumes a modern terminal. So do we.

Our architecture avoids case 2 by design: we have one scrollable content area with fixed status bar and input line.

## The Scroll Pipeline

```
Scroll event (user scroll or new content)
    |
    |-- Compute scroll hint: {top, bot, delta}
    |
    |-- Terminal: CSI top;bot r  +  CSI n S  +  CSI r
    |   +-- Terminal shifts displayed rows (~20 bytes)
    |
    |-- prevScreen: scroll_up(top, bot, n)
    |   +-- Buffer cells shifted to match terminal state (~35KB memmove)
    |
    +-- Diff engine: only new/vacated rows differ
        +-- Emit ANSI for ~1 row instead of ~37 rows (~200 bytes vs ~20KB)
```

Hardware scroll is the second most impactful optimization after the blit. Blit eliminates work for unchanged subtrees. Hardware scroll eliminates work for shifted content. Together, they reduce per-frame work from "redraw everything" to "redraw only what's actually new." In a streaming coding agent, that typically means 1-2 rows per frame out of 40.

---

**Next: [Part 10 — Double Buffering and Frame Lifecycle](/post/double-buffering-frame-lifecycle)**
