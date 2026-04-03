---
title: "The Diff Engine — Only Paint What Changed"
date: 2026-03-15
slug: diff-engine-only-paint-what-changed
tags: c++23, diff-engine, ansi-escape, cursor-optimization, damage-rect, claude-code
excerpt: "The heart of the renderer: compare two screen buffers cell by cell, emit the minimal ANSI to transform one into the other. Every optimization upstream exists to make this loop faster. Every optimization here directly reduces bytes to stdout."
---

This is it. The inner sanctum. The diff engine is the single most performance-critical component in the entire rendering pipeline. It takes two screen buffers — what the terminal currently shows and what we want it to show — and emits the minimal ANSI escape sequences to bridge the gap.

Everything we've built so far — interning, blit, damage tracking — exists to make this loop's job easier. And every optimization *inside* this loop directly reduces bytes written to stdout, syscalls made, and time the terminal spends parsing ANSI.

## Claude Code's Diff: Two Stages

The diff happens in two stages in Claude Code's binary:

### Stage 1: The Cell Scanner (`Fkq`)

```js
function Fkq(prevScreen, nextScreen, callback) {
    // Compute bounding rect: union of prev.damage and next.damage
    let damage = unionDamage(prevScreen.damage, nextScreen.damage);

    // Same-width buffers: fast path
    if (prevScreen.width === nextScreen.width) {
        CWK(prevScreen, nextScreen, damage, callback);
    } else {
        // Different widths (resize): slow path
        SWK(prevScreen, nextScreen, damage, callback);
    }
}
```

The fast path `CWK` handles the common case — same-width buffers between frames:

```js
function CWK(prev, next, damage, callback) {
    let w = next.width;
    for (let y = damage.y; y < damage.y + damage.height; y++) {
        let base = y * w;
        // Find first difference in this row
        let start = vWK(prev.cells, next.cells,
                        (base + damage.x) << 1,
                        damage.width);
        if (start === damage.width) continue;  // entire row unchanged

        // Iterate from first difference to end of damage region
        for (let x = damage.x + start; x < damage.x + damage.width; x++) {
            let idx = (base + x) << 1;
            if (prev.cells[idx] === next.cells[idx]
                && prev.cells[idx | 1] === next.cells[idx | 1])
                continue;  // cell unchanged

            let prevCell = unpackCell(prev, idx);
            let nextCell = unpackCell(next, idx);
            callback(x, y, prevCell, nextCell);
        }
    }
}
```

Notice how it only iterates within the damage rectangle. Rows outside the damage rect are never touched. And within each row, it first scans for the *first* different cell using the `vWK` fast scan:

```js
function vWK(prevCells, nextCells, offset, count) {
    for (let i = 0; i < count; i++, offset += 2) {
        if (prevCells[offset] !== nextCells[offset] ||
            prevCells[offset + 1] !== nextCells[offset + 1])
            return i;
    }
    return count;  // all same
}
```

Two comparisons per cell — char ID and packed style. For unchanged rows, this scans the entire row and returns `count` (row unchanged, skip it). For rows with changes near the end, it blazes through the unchanged prefix at full memory bandwidth.

### Stage 2: The Render Method

The render method processes the scanner's callbacks and builds ANSI output:

```js
class Renderer {
    render(prevScreen, nextScreen, scrollHint) {
        let ops = [];
        let currentStyle = nextScreen.emptyStyleId;
        let cursorX = 0, cursorY = 0;

        // Handle scroll hint (hardware scroll, covered in Part 9)
        if (scrollHint) {
            this.applyHardwareScroll(ops, scrollHint, prevScreen);
        }

        // Diff changed cells
        Fkq(prevScreen, nextScreen, (x, y, prev, next) => {
            // Move cursor if needed
            if (cursorY !== y || cursorX !== x) {
                if (cursorY === y)
                    ops.push({ type: "cursorTo", col: x });
                else
                    ops.push({ type: "cursorMove", x, y });
            }

            // Transition style if needed
            if (next.styleId !== currentStyle) {
                let transition = this.stylePool.transition(
                    currentStyle, next.styleId);
                if (transition)
                    ops.push({ type: "styleStr", str: transition });
                currentStyle = next.styleId;
            }

            // Emit character
            ops.push({ type: "stdout", content: next.char });
            cursorX = x + (next.width || 1);
        });

        return this.serializeOps(ops);
    }
}
```

## The Three Costs of a Changed Cell

Every changed cell incurs three costs. Understanding them is the key to understanding the diff engine's optimization strategy:

### Cost 1: Cursor Positioning

Moving the cursor to the cell's position. The diff engine minimizes this by tracking cursor state:

```
Unchanged: ||||||||...|||||||||||||  (. = changed)
                    ^
                    Only move cursor here once
```

The cursor advances automatically after each character. For a run of consecutive changed cells, only *one* cursor move is needed at the start. The rest are free — the cursor is already there.

For non-consecutive changes in the same row:

```
Row:  Hello...World...!
           ^^^     ^^^
Two cursor moves: move(5,y) ... move(13,y)
```

Same-row cursor movement uses the cheaper `CSI G` sequence (~5 bytes) instead of `CSI H` (~7 bytes). Moving to column 0 uses `\r` (1 byte). Moving to the next row at column 0 uses `\r\n` (2 bytes). These special cases add up.

### Cost 2: Style Transition

Switching from the previous cell's style to the current cell's. The diff engine tracks `currentStyle`:

```
if (next.styleId !== currentStyle) {
    emit(stylePool.transition(currentStyle, next.styleId));
    currentStyle = next.styleId;
}
```

Adjacent changed cells with the same style: **zero style bytes emitted.** This is common in code blocks where entire lines share one syntax highlighting style. The transition cache (Part 4, more in Part 8) ensures that even when styles do change, the minimal ANSI is emitted.

### Cost 3: Character Output

The actual content. Usually 1 byte for ASCII, 2-4 bytes for Unicode. This is the irreducible minimum — you have to send the character no matter what.

## Our C++ Diff Engine

```cpp
class DiffEngine {
public:
    void diff(const ScreenBuf& prev, const ScreenBuf& next,
              const StylePool& styles, std::string& out);

private:
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    StyleId current_style_{0};

    void move_cursor(std::string& out, int x, int y);
    void emit_transition(std::string& out, const StylePool& styles,
                         StyleId to);
};
```

### The Core Loop

```cpp
void DiffEngine::diff(const ScreenBuf& prev, const ScreenBuf& next,
                      const StylePool& styles, std::string& out) {
    int w = next.w();
    int h = next.h();

    // Damage rect bounds the work
    Rect damage = (prev.w() == w && prev.h() == h)
        ? union_damage(prev.damage(), next.damage())
        : Rect{0, 0, w, h};

    for (int y = damage.y; y < damage.y + damage.h && y < h; ++y) {
        auto prev_row = prev.row(y);
        auto next_row = next.row(y);

        for (int x = damage.x; x < damage.x + damage.w && x < w; ++x) {
            const auto& pc = prev_row[x];
            const auto& nc = next_row[x];

            // Fast path: 8-byte compare
            if (cells_equal(pc, nc)) continue;

            // Skip continuation cells (width == 0)
            if (nc.width == 0) continue;

            // Move cursor to this position
            move_cursor(out, x, y);

            // Transition style if needed
            StyleId next_style{nc.style_id};
            emit_transition(out, styles, next_style);

            // Emit character
            encode_one(nc.ch, out);
            cursor_x_ = x + nc.width;
        }
    }
}
```

The `cells_equal` call is where the 8-byte cell layout pays off — it compiles to a single 64-bit comparison. If the cell hasn't changed, we skip it immediately. No unpacking, no field-by-field check. One instruction and we're on to the next cell.

### Cursor Movement: Every Byte Counts

```cpp
void DiffEngine::move_cursor(std::string& out, int x, int y) {
    if (cursor_y_ == y && cursor_x_ == x) return;  // already there

    if (cursor_y_ == y) {
        // Same row: absolute column position (CSI G)
        out += std::format("\033[{}G", x + 1);
    } else if (x == 0) {
        if (y == cursor_y_ + 1) {
            out += "\r\n";  // next row, col 0: 2 bytes
        } else {
            out += std::format("\033[{};1H", y + 1);
        }
    } else {
        // General case: absolute position (CSI H)
        out += std::format("\033[{};{}H", y + 1, x + 1);
    }

    cursor_x_ = x;
    cursor_y_ = y;
}
```

We special-case the common movements:
- Already at position: 0 bytes (the cheapest byte is the one you don't send)
- Same row, different column: `CSI <col>G` (~5 bytes)
- Next row, column 0: `\r\n` (2 bytes instead of ~7 for full positioning)
- Everything else: `CSI <row>;<col>H` (~7 bytes)

These savings are small per occurrence but multiply across thousands of cursor movements per second.

### Row Skip Optimization

We can detect unchanged row prefixes using the C++ equivalent of Claude Code's `vWK`:

```cpp
int find_first_diff(std::span<const SCell> prev,
                    std::span<const SCell> next, int count) {
    for (int i = 0; i < count; ++i) {
        uint64_t a, b;
        std::memcpy(&a, &prev[i], 8);
        std::memcpy(&b, &next[i], 8);
        if (a != b) return i;
    }
    return count;
}
```

The `memcpy` + compare compiles to a single `cmp` instruction per cell. On a row where the change is near the end, this skips the unchanged prefix at full memory bandwidth. On a fully unchanged row, it scans the entire width and returns `count` — the caller skips the row entirely.

For larger terminals, we could use SIMD to compare 4 cells (32 bytes) per instruction with AVX2. But for a typical 120-column terminal (960 bytes per row), scalar comparison is already sub-microsecond. We'd be optimizing something that isn't the bottleneck.

## The Resize Special Case

When the terminal resizes, the previous and current buffers have different dimensions. Claude Code handles this pragmatically:

```js
if (prevScreen.width !== nextScreen.width
    || prevScreen.height !== nextScreen.height) {
    ops.push({ type: "clearTerminal", reason: "resize" });
}
```

Just clear and redraw everything. Resize is rare — the user has to manually drag the terminal window. A full redraw is imperceptible at the speed this engine runs. Trying to do a partial diff across different-width buffers adds significant complexity for a case that happens once every few minutes.

We do the same:

```cpp
if (prev.w() != next.w() || prev.h() != next.h()) {
    out += "\033[2J";    // clear screen
    out += "\033[1;1H";  // home cursor
    // Fall through to full-screen diff with damage = entire screen
}
```

## No Intermediate Representation

Claude Code's render method produces an array of operation objects that get serialized to a string in a final pass. Clean architecture, but it creates garbage — JS objects that exist only to be iterated once and thrown away.

We skip the intermediate representation entirely. The diff loop appends cursor moves, style transitions, and characters directly to a `std::string` buffer:

```cpp
out.reserve(next.w() * next.h());  // worst case: 1 byte per cell
```

No intermediate allocations. No garbage collection pressure. The output string is pre-reserved and reused across frames. In practice, the output is 5-10% of the worst-case reservation because most cells are unchanged.

## Putting the Costs in Perspective

For a typical frame on a 120x40 terminal:

| Scenario | Cells diffed | ANSI bytes | Time |
|----------|-------------|------------|------|
| Spinner tick | 1-3 cells | ~20 bytes | <1us |
| One line of streamed text | ~80 cells | ~200 bytes | ~5us |
| Full code block appeared | ~400 cells | ~2000 bytes | ~50us |
| Terminal resize (full redraw) | 4800 cells | ~15000 bytes | ~200us |

Even the worst case — full screen redraw — is sub-millisecond on modern hardware. We have a 16ms frame budget and we're using ~1% of it. The bottleneck is never the diff engine. It's always the stdout I/O — writing bytes through the pty to the terminal emulator.

The diff engine's job is to minimize *those* bytes. And between damage-bounded iteration, cursor tracking, style transition caching, and the 8-byte fast-compare cell layout, it does exactly that.

---

**Next: [Part 8 — Style Transitions](/post/style-transitions-transition-cache)**
