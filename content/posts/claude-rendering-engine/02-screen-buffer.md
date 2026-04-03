---
title: "The Screen Buffer — A 2D Cell Grid"
date: 2026-04-01
slug: screen-buffer-2d-cell-grid
tags: c++23, terminal-rendering, screen-buffer, memory-layout, simd, claude-code
excerpt: Every terminal UI, no matter how complex, reduces to filling a 2D grid of cells and diffing it against the previous frame. Claude Code uses a dual-view memory trick to make this fast. In C++, we don't need the trick.
---

Here's a fact that will simplify everything that follows: **a terminal displays a grid of cells.** That's it. Every fancy UI you've ever seen in a terminal — syntax-highlighted code, box-drawn panels, animated spinners, progress bars — is just a 2D array of cells being updated 60 times per second.

Each cell contains exactly three things:

- **A character** — a Unicode codepoint (or grapheme cluster for emoji/CJK)
- **A style** — foreground color, background color, attributes (bold, italic, etc.)
- **A width** — 1 for normal characters, 2 for wide characters (CJK, emoji), 0 for the continuation half of a wide character

That's the atom. Everything in this series — the interning, the diffing, the caching, the scroll tricks — exists to manipulate this grid efficiently. The screen buffer is just the container.

## What I Found in Claude Code

When I decompiled the binary, the `createScreen` function was one of the first things I identified:

```js
// JS (extracted from Claude Code binary)
function createScreen(width, height, stylePool, charPool, hyperlinkPool) {
    let total = width * height;
    let buffer = new ArrayBuffer(total << 3);  // 8 bytes per cell
    return {
        width, height,
        cells:    new Int32Array(buffer),     // 2 x int32 per cell
        cells64:  new BigInt64Array(buffer),  // same memory, 64-bit view
        charPool, hyperlinkPool,
        emptyStyleId: stylePool.none,
        damage: undefined,
        noSelect: new Uint8Array(total)
    };
}
```

Notice something clever? Two typed array views over the same `ArrayBuffer`:
- `cells` (Int32Array) — for reading/writing individual fields (charId, packed style)
- `cells64` (BigInt64Array) — for blazing fast 8-byte comparison

This dual-view trick is the first key insight. When the diff engine needs to check whether a cell changed between frames, it doesn't compare two 32-bit integers separately. It compares one 64-bit integer. One instruction. Done.

## The C++ Design: We Don't Need the Trick

In JavaScript, you're stuck with typed arrays and creative memory aliasing because you can't define custom structs with controlled layout. In C++, we just... define the struct:

```cpp
struct SCell {
    char32_t ch       = U' ';   // 4 bytes -- Unicode codepoint
    uint16_t style_id = 0;      // 2 bytes -- interned style handle
    uint8_t  width    = 1;      // 1 byte  -- 0/1/2 display width
    uint8_t  flags    = 0;      // 1 byte  -- reserved

    constexpr bool operator==(const SCell& o) const {
        return ch == o.ch && style_id == o.style_id && width == o.width;
    }
    constexpr bool operator!=(const SCell& o) const { return !(*this == o); }
};

static_assert(sizeof(SCell) == 8);
```

Exactly 8 bytes. No padding. The `static_assert` is a compile-time guarantee — if anyone ever adds a field that breaks the layout, the build fails immediately. Not at runtime. Not in a test. During compilation.

### Why 8 Bytes Is Magic

8 bytes = 1 `uint64_t`. We can compare two cells with a single 64-bit operation:

```cpp
// Fast path: reinterpret as uint64_t for single-instruction compare
inline bool cells_equal(const SCell& a, const SCell& b) {
    uint64_t va, vb;
    std::memcpy(&va, &a, 8);
    std::memcpy(&vb, &b, 8);
    return va == vb;
}
```

The compiler sees `std::memcpy` of 8 bytes into a `uint64_t` and optimizes it into a single register load. No function call, no loop. One `mov`, one `cmp`. This is the same performance as Claude Code's `BigInt64Array` comparison, achieved without any aliasing tricks — just a struct that happens to be the right size.

For bulk operations, the benefits compound:

```cpp
// Clear a row: fill with blank cells
void clear_row(int row) {
    std::fill_n(cells_.data() + row * w_, w_, SCell{});
}

// Compare two rows: memcmp over w_ x 8 bytes
bool rows_equal(const SCell* a, const SCell* b, int width) {
    return std::memcmp(a, b, width * sizeof(SCell)) == 0;
}
```

### Why Not Bitpack Like Claude Code?

Claude Code packs `charId << 17 | hyperlinkId << 2 | width` into one int32, giving:
- charId: unlimited (int32 range)
- styleId: 15 bits = 32768 styles
- hyperlinkId: 15 bits = 32768 hyperlinks
- width: 2 bits = 0-3

We chose a different layout for four reasons:

1. **We don't need hyperlink IDs in the cell.** Terminal hyperlinks (OSC 8) are rare in a coding agent TUI and can be tracked separately.

2. **`char32_t` is debuggable.** Inspect an `SCell` in GDB or LLDB and you see the actual character. Inspect an interned ID and you see... a number. Good luck figuring out if that's a space or an `x`.

3. **`uint16_t` style_id gives 65536 styles** — double what Claude Code gets with 15 bits — and we get a full `char32_t` for the character without any interning overhead.

4. **No bit manipulation on the hot path.** Reading `cell.style_id` compiles to a single `movzx` (zero-extending load). Reading `packed >>> 17` is a shift plus a mask. In a tight diff loop running millions of times per second, this matters.

The trade-off: we store the actual codepoint instead of an interned char ID. Character comparison is 4 bytes instead of potentially fewer. But the bottleneck is style transitions, not character comparison, so this trade-off is free in practice.

## The ScreenBuf Class

```cpp
class ScreenBuf {
public:
    ScreenBuf() = default;
    ScreenBuf(int w, int h);

    void resize(int w, int h);
    void clear();
    void clear_row(int row);

    // Direct cell access
    SCell&       at(int r, int c)       { return cells_[r * w_ + c]; }
    const SCell& at(int r, int c) const { return cells_[r * w_ + c]; }

    // Write operations
    void put(int r, int c, char32_t ch, StyleId style, uint8_t width = 1);
    int  write_text(int r, int c, std::string_view utf8, StyleId style);
    void fill(int r, int c, int n, char32_t ch, StyleId style);

    // Geometry
    int w() const { return w_; }
    int h() const { return h_; }

    // Row access (zero-copy view)
    std::span<SCell>       row(int r);
    std::span<const SCell> row(int r) const;

    // Scroll: shift rows up within [top, bot)
    void scroll_up(int top, int bot, int n);

private:
    std::vector<SCell> cells_;
    int w_ = 0, h_ = 0;
};
```

Clean, boring, exactly what you'd expect. The interesting bits:

### Row Access via `std::span`

`std::span` gives us a zero-copy view into the cell array:

```cpp
std::span<SCell> row(int r) {
    return { cells_.data() + r * w_, static_cast<size_t>(w_) };
}
```

This is powerful for the diff engine — it can iterate a row without bounds checking on every cell access, and the span carries its own length for safety at the boundary. No raw pointer arithmetic exposed to the caller. No copies. Just a view.

### Scroll via `std::copy_n`

Hardware scroll shifts rows in the buffer without touching the terminal:

```cpp
void ScreenBuf::scroll_up(int top, int bot, int n) {
    if (n <= 0 || top >= bot) return;
    n = std::min(n, bot - top);
    // Move rows [top+n .. bot) -> [top .. bot-n)
    for (int r = top; r < bot - n; ++r)
        std::copy_n(cells_.data() + (r + n) * w_, w_,
                    cells_.data() + r * w_);
    // Clear vacated rows at bottom
    for (int r = bot - n; r < bot; ++r)
        std::fill_n(cells_.data() + r * w_, w_, SCell{});
}
```

Compare with Claude Code's `dJT` which uses `BigInt64Array.copyWithin()` — same operation, same performance. C++ `std::copy_n` on a contiguous array compiles to `memmove`, which is SIMD-optimized on every modern platform.

## Memory Layout

Let's do the math for a standard 120x40 terminal:

```
cells_: [ SCell x 4800 ]
         -- 4800 x 8 bytes = 38.4 KB

Two buffers (current + previous): 76.8 KB total
```

This fits comfortably in L1 cache on modern CPUs (typically 32-64 KB per core). When the diff engine iterates both buffers, it gets excellent cache behavior because the data is contiguous and access is sequential. No pointer chasing. No cache misses. Just a flat array that the prefetcher loves.

## The `put` Operation

Writing a character into the buffer is simple, but it has to handle wide characters correctly:

```cpp
void ScreenBuf::put(int r, int c, char32_t ch, StyleId style, uint8_t w) {
    if (r < 0 || r >= h_ || c < 0 || c >= w_) return;
    cells_[r * w_ + c] = {ch, style.raw, w, 0};
    // Wide character: mark next cell as continuation
    if (w == 2 && c + 1 < w_)
        cells_[r * w_ + c + 1] = {U'\0', style.raw, 0, 0};
}
```

Wide characters (CJK, some emoji) occupy two cells. The first cell has `width = 2` and the actual character. The second cell has `width = 0` (continuation) and a null character. The diff engine knows to skip continuation cells — they're just placeholders for the right half of a wide glyph.

This seems like a small detail, but getting it wrong means your cursor positioning drifts by one column for every wide character on screen. Ask me how I know.

## What Claude Code Taught Me

The JS implementation uses `ArrayBuffer` with typed array views to achieve what C++ gives you for free: flat, contiguous, fixed-size memory. The dual `Int32Array`/`BigInt64Array` view is a clever hack born of language limitations — it's unnecessary in C++ where struct layout is deterministic.

The real takeaway is simpler: **the screen buffer is intentionally boring.** It's a flat array with simple accessors. All the complexity lives in what *writes* into it (the render tree walk) and what *reads* from it (the diff engine). The buffer itself is just dumb storage optimized for sequential access.

This is good design. The boring parts should be boring. Save the cleverness for where it matters.

---

**Next: [Part 3 — Cell Packing](/post/cell-packing-eight-bytes)**
