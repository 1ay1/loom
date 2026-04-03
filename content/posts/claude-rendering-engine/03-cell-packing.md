---
title: "Cell Packing — Fitting a Universe into 8 Bytes"
date: 2026-04-01
slug: cell-packing-eight-bytes
tags: c++23, bit-packing, phantom-types, static-assert, memory-layout, claude-code
excerpt: "How do you fit a character, a style, a width, and type safety into exactly 8 bytes? Claude Code uses bitfield surgery. We use phantom-tagged IDs and static_assert. Both compile to one cmp instruction."
---

The 8-byte cell is the most important design constraint in the entire rendering engine. Everything flows from it: cache line efficiency, SIMD friendliness, single-instruction comparison, O(1) buffer swaps. Getting the cell layout right means everything downstream is fast by default. Getting it wrong means you're fighting the hardware on every frame.

Let me show you how Claude Code packs its cells, why we made different choices, and how C++'s type system gives us safety guarantees that JavaScript can never provide.

## Inside Claude Code's Bit Packing

Each cell in Claude Code is stored as two `Int32` values in a flat `Int32Array`:

```
cells[i * 2]     = charId       (interned string ID from CharPool)
cells[i * 2 + 1] = styleId << 17 | hyperlinkId << 2 | width
```

The magic constants I extracted from the minified binary:

```
z2_ = 17      styleId shift
w2_ = 2       hyperlinkId shift
Bb  = 3       width mask (2 bits)
Jg_ = 32767   hyperlinkId mask (15 bits = 0x7FFF)
IJT = 0       space character ID
uJT = 0n      empty cell (BigInt64 zero)
```

Here's the bit layout of the second int32:

```
 31                17 16              2 1  0
+-------------------+-----------------+----+
|     styleId       |   hyperlinkId   | w  |
|    (15 bits)      |   (15 bits)     |(2b)|
+-------------------+-----------------+----+
```

This is dense, efficient, and slightly terrifying. Let me trace through the read and write paths:

### Reading a cell (the `gt` function):

```js
function gt(screen, cellIndex, out) {
    let packed = screen.cells[cellIndex | 1];  // second int32
    out.char    = screen.charPool.get(screen.cells[cellIndex]);  // first int32
    out.styleId = packed >>> 17;
    out.width   = packed & 3;
    let hlId    = (packed >>> 2) & 32767;
    out.hyperlink = hlId === 0 ? undefined : screen.hyperlinkPool.get(hlId);
}
```

### Writing a cell (the `Bt` function):

```js
function Bt(styleId, hyperlinkId, width) {
    return styleId << 17 | hyperlinkId << 2 | width;
}
```

### The killer comparison:

```js
// cells64[i] covers cells[i*2] and cells[i*2+1]
if (prevScreen.cells64[i] === nextScreen.cells64[i]) {
    // Cell unchanged -- skip
}
```

One 64-bit integer comparison. Zero unpacking. This is the hot path of the diff engine and it's as fast as physically possible.

## Why Intern Characters?

You might wonder: why not just store the character directly? Claude Code interns strings into integer IDs because:

1. **Grapheme clusters.** An emoji like `U+1F468 U+200D U+1F469 U+200D U+1F467 U+200D U+1F466` is a multi-codepoint sequence stored as a single string. Interning lets you compare it as a single integer.

2. **Memory density.** A string pointer is 8 bytes on 64-bit systems. An interned ID is 4 bytes (or fewer if packed).

3. **Comparison speed.** Comparing two interned IDs is `cmp eax, ebx`. Comparing two strings is a loop.

The trade-off: you need a pool (hash map) for interning, and you need to look up the actual string when emitting output. Claude Code's `CharPool` has an ASCII fast path — an array indexed directly by character code — that makes the common case (ASCII text in a coding agent) extremely fast.

## Our C++ Approach: Direct Codepoints

We made a fundamentally different trade-off:

```cpp
struct SCell {
    char32_t ch       = U' ';   // 4 bytes
    uint16_t style_id = 0;      // 2 bytes
    uint8_t  width    = 1;      // 1 byte
    uint8_t  flags    = 0;      // 1 byte
};
static_assert(sizeof(SCell) == 8);
```

### Why `char32_t` instead of an interned ID?

1. **We don't need grapheme cluster support yet.** For a coding agent TUI, the vast majority of characters are ASCII or simple Unicode. We don't render emoji sequences in cells — they appear in streamed markdown output.

2. **Debuggability is a feature.** Inspect an `SCell` in a debugger and you see the actual character. Inspect an interned ID and you see... the number 47. Is that a space? A period? The letter 'o'? You'll need to look it up in a pool that probably isn't visible in your current stack frame. Life is too short.

3. **No pool overhead.** No hash map lookup on write or read. Direct field access compiles to a single load instruction. Zero indirection.

4. **We still get 8-byte cells.** `char32_t` (4) + `uint16_t` (2) + two `uint8_t` (1+1) = 8 bytes. Same density as Claude Code's packed pair. Same single-instruction comparison.

### Why `uint16_t` style_id instead of bitfield packing?

Claude Code uses 15 bits for the style ID (max 32768 styles). We use a full `uint16_t` (max 65536 styles). The difference at the machine level:

```cpp
// Claude Code (JS): read styleId from packed int
out.styleId = packed >>> 17;

// Our C++: read styleId
auto sid = cell.style_id;  // direct field access
```

The compiler generates a simple `movzx` (zero-extending load) for the field access. No shift, no mask. In a tight diff loop that processes millions of cells, removing two arithmetic operations per cell adds up.

## Phantom-Tagged Style IDs

Here's where C++ gets interesting. A raw `uint16_t` is dangerous — nothing stops you from accidentally passing a character index where a style ID is expected:

```cpp
uint16_t style = 42;
uint16_t char_idx = 42;
do_something(char_idx);  // oops, passed the wrong one. compiles fine.
```

C++ can catch this at compile time with phantom types:

```cpp
template <typename Tag>
struct StrongU16 {
    uint16_t raw = 0;
    constexpr StrongU16() = default;
    constexpr explicit StrongU16(uint16_t v) : raw(v) {}
    constexpr bool operator==(const StrongU16&)  const = default;
    constexpr auto operator<=>(const StrongU16&) const = default;
};

using StyleId = StrongU16<struct style_tag>;
```

Now `StyleId` and a hypothetical `CharId` are different types:

```cpp
StyleId s{42};
CharId  c{42};
s == c;  // COMPILE ERROR -- different types
```

The `Tag` type parameter exists only at compile time. It generates no runtime code, occupies no memory, and has no representation in the binary. But it provides full type safety. The `explicit` constructor prevents implicit conversions from raw integers. You have to *mean it* when you construct a `StyleId`.

This is the kind of guarantee that JavaScript literally cannot express. In Claude Code's codebase, if someone passes a `charId` where a `styleId` is expected, the bug is discovered at runtime (maybe) by a test (hopefully). In our C++, it's discovered by the compiler. Before anything runs. Before tests even compile.

### The Sizeof Trap

You might think: just use `StyleId` directly in the cell struct:

```cpp
struct SCell {
    char32_t ch;
    StyleId  style;   // StrongU16<style_tag>
    uint8_t  width;
    uint8_t  flags;
};
```

But `StrongU16` has a `uint16_t` member plus the explicit constructor, which can affect alignment on some compilers. I've seen `sizeof(SCell)` become 12 instead of 8 due to padding with certain compiler/platform combinations.

Solution: store the raw `uint16_t` in the struct, provide typed accessors at the API boundary:

```cpp
struct SCell {
    char32_t ch       = U' ';
    uint16_t style_id = 0;      // raw storage
    uint8_t  width    = 1;
    uint8_t  flags    = 0;

    constexpr StyleId style() const { return StyleId{style_id}; }
    constexpr void set_style(StyleId s) { style_id = s.raw; }
};
static_assert(sizeof(SCell) == 8);  // guaranteed
```

Type safety at the API boundary. Raw performance in the struct layout. The `static_assert` catches any future regression. This is having your cake, eating it too, and the compiler checking that the cake was correctly constructed.

## Side-by-Side: JS vs C++ Cell Access

### Reading a cell

```js
// Claude Code (JS) -- 2 array lookups + 2 bit operations
let charStr  = screen.charPool.get(screen.cells[idx]);
let styleId  = screen.cells[idx + 1] >>> 17;
let width    = screen.cells[idx + 1] & 3;
```

```cpp
// Our C++ -- 1 struct access, fields are direct
auto& cell = buf.at(row, col);
char32_t ch    = cell.ch;        // single load
uint16_t sid   = cell.style_id;  // single load
uint8_t  w     = cell.width;     // single load
```

### Comparing two cells

```js
// Claude Code (JS) -- BigInt64 comparison
if (prev.cells64[i] !== next.cells64[i]) { /* changed */ }
```

```cpp
// Our C++ -- memcmp of 8 bytes (compiles to single uint64 compare)
if (prev.at(r, c) != next.at(r, c)) { /* changed */ }
```

Both are single-instruction comparisons at the machine level. The C++ version is arguably cleaner because the comparison operator is defined on the type itself, not on a reinterpreted memory view.

### Writing a cell

```js
// Claude Code (JS) -- 2 array writes + bit packing
screen.cells[idx] = charPool.intern(ch);
screen.cells[idx + 1] = Bt(styleId, 0, width);
```

```cpp
// Our C++ -- 1 struct assignment
buf.at(r, c) = { ch, style.raw, w, 0 };
```

Aggregate initialization. One store to contiguous memory. The compiler may optimize this to a single 8-byte store instruction.

## The 8-Byte Constraint as a Design Force

Everything downstream benefits from the 8-byte cell:

- **Cache line efficiency**: 8 cells per 64-byte cache line. The diff engine processes cells in groups that map perfectly to hardware cache lines.
- **Alignment**: naturally aligned for 64-bit operations. No unaligned access penalties.
- **SIMD friendly**: 128-bit SSE register holds 2 cells, 256-bit AVX holds 4. When we eventually add SIMD diffing, the data is already in the right shape.
- **Bulk operations**: `memset`, `memcpy`, `memcmp` work on cell arrays directly without any conversion or packing step.
- **Buffer swap**: swapping two `std::vector<SCell>`s is O(1) — just three pointer swaps.

This isn't a limitation we grudgingly accept. It's a constraint we *chose*, and it makes everything else faster by default. Constraints are underrated in systems design. The right constraint, applied early, prevents a thousand bad decisions later.

---

**Next: [Part 4 — Interning Pools](/post/interning-pools-strings-are-integers)**
