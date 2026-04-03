---
title: "Interning Pools — Strings Are Integers Now"
date: 2026-04-01
slug: interning-pools-strings-are-integers
tags: c++23, interning, style-pool, transition-cache, constexpr, hash-map, claude-code
excerpt: "The moment you intern a style, comparison becomes integer equality. The moment you cache the transition between two styles, rendering becomes a hash table lookup. This is where Claude Code's renderer goes from fast to unreasonably fast."
---

Interning is one of those ideas that sounds boring until you see the numbers. The concept is simple: map objects to unique integer IDs. Once interned, comparison becomes integer equality — O(1) regardless of the object's size or complexity.

Claude Code interns three things:
1. **Characters** -> `CharPool` (string -> int, with an ASCII fast path)
2. **Styles** -> `StylePool` (ANSI token array -> int, with a transition cache)
3. **Hyperlinks** -> `HyperlinkPool` (URL string -> int)

We focus on the first two. Hyperlinks are rare enough in a coding agent TUI that we can ignore them without guilt.

## Claude Code's CharPool: The ASCII Fast Path

Here's what I extracted from the binary:

```js
class CharPool {
    strings = [" ", ""];           // index 0 = space, 1 = empty string
    stringMap = new Map();         // string -> id (for non-ASCII)
    ascii = new Int32Array(128);   // fast path: ascii[charCode] -> id

    constructor() {
        this.ascii.fill(-1);
        this.ascii[32] = 0;       // space -> id 0
        this.stringMap.set(" ", 0);
        this.stringMap.set("", 1);
    }

    intern(str) {
        // ASCII fast path: single character, code < 128
        if (str.length === 1) {
            let code = str.charCodeAt(0);
            if (code < 128) {
                let id = this.ascii[code];
                if (id !== -1) return id;
                id = this.strings.length;
                this.strings.push(str);
                this.ascii[code] = id;
                return id;
            }
        }
        // General path: hash map lookup
        let existing = this.stringMap.get(str);
        if (existing !== undefined) return existing;
        let id = this.strings.length;
        this.strings.push(str);
        this.stringMap.set(str, id);
        return id;
    }

    get(id) { return this.strings[id] ?? " "; }
}
```

The ASCII fast path is smart. In a coding agent TUI, roughly 95% of characters are ASCII. The `Int32Array` lookup avoids the hash map entirely for these — it's just `ascii[charCode]`, an array index. O(1) with a tiny constant factor.

### Why We Don't Need a CharPool

In our C++ design, cells store `char32_t` directly (Part 3). This eliminates character interning entirely because:

1. Single codepoints fit in 4 bytes — same size as an interned ID would be.
2. No grapheme cluster storage needed in cells.
3. Eliminating the pool removes a level of indirection on every cell read during diff output.

If we later need grapheme cluster support (emoji with ZWJ sequences), we can add a pool then. Until then, YAGNI. The pool would add complexity and indirection for zero measurable benefit.

## The StylePool: Where the Real Magic Lives

Characters are straightforward. Styles are where Claude Code gets genuinely clever. Here's the extracted StylePool:

```js
class StylePool {
    ids = new Map();                // serialized style -> id
    styles = [];                    // id -> style token array
    transitionCache = new Map();    // (from*1048576+to) -> ANSI string

    none;  // the empty/default style ID

    constructor() {
        this.none = this.intern([]);
    }

    intern(tokens) {
        let key = tokens.length === 0
            ? ""
            : tokens.map(t => t.code).join("\0");

        let id = this.ids.get(key);
        if (id === undefined) {
            let idx = this.styles.length;
            this.styles.push(tokens);
            id = idx << 1 | (hasReverseStyle(tokens) ? 1 : 0);
            this.ids.set(key, id);
        }
        return id;
    }

    get(id) { return this.styles[id >>> 1] ?? []; }

    // THE KEY METHOD
    transition(fromId, toId) {
        if (fromId === toId) return "";
        let cacheKey = fromId * 1048576 + toId;
        let cached = this.transitionCache.get(cacheKey);
        if (cached === undefined) {
            cached = computeTransition(this.get(fromId), this.get(toId));
            this.transitionCache.set(cacheKey, cached);
        }
        return cached;
    }
}
```

Read that `transition` method carefully. This is the single most important function in the entire rendering pipeline.

### The Transition Cache: Why It Matters

`transition(from, to)` is called for *every changed cell* in the diff engine. For every cell that differs between frames, the engine needs the ANSI escape sequences to switch from the previous cell's style to the current cell's style.

Without the cache, each transition requires:
1. Look up both style token arrays
2. Compute the set difference (which attributes to close, which to open)
3. Serialize to an ANSI escape string

With the cache, subsequent transitions between the same pair of styles are a single hash map lookup returning a pre-computed string. One probe. Done.

The cache key `fromId * 1048576 + toId` packs two 20-bit IDs into one integer. This supports up to ~1 million unique styles — more than any terminal UI will ever use.

Think about what happens after the first few frames of rendering: the cache contains every style transition that actually occurs in your UI. Code blocks alternate between keyword, string, comment, and operator styles. The status bar alternates between normal and highlighted. Every one of these transitions is computed *once* and then served from cache forever.

## Our C++ StylePool

We take the same approach but with stronger types and a different interning strategy:

```cpp
class StylePool {
public:
    StylePool();

    // Intern a style -> get a stable ID
    StyleId intern(const PackedStyle& s);

    // Resolve an ID back to the style
    const PackedStyle& resolve(StyleId id) const;

    // Get pre-computed ANSI string for a style
    const std::string& ansi(StyleId id) const;

    // The default (no-style) ID
    StyleId default_id() const;

private:
    std::vector<PackedStyle> styles_;
    std::vector<std::string> ansi_;                    // pre-computed ANSI per style
    std::unordered_map<uint64_t, uint16_t> index_;     // hash -> id

    static std::string compute_ansi(const PackedStyle& s);
};
```

### PackedStyle: Compile-Time Style Building

Our style representation is a simple struct with a `constexpr` fluent API:

```cpp
struct PackedStyle {
    uint32_t fg    = palette::text;    // 24-bit RGB foreground
    uint32_t bg    = palette::base;    // 24-bit RGB background
    uint8_t  attrs = attr::none;       // bold/italic/underline/etc

    constexpr PackedStyle with_fg(uint32_t c)  const { auto s = *this; s.fg = c; return s; }
    constexpr PackedStyle with_bg(uint32_t c)  const { auto s = *this; s.bg = c; return s; }
    constexpr PackedStyle with_bold()          const { auto s = *this; s.attrs |= attr::bold; return s; }
    constexpr PackedStyle with_italic()        const { auto s = *this; s.attrs |= attr::italic; return s; }
    // ...

    // Injective hash for 24-bit colors
    constexpr uint64_t hash() const {
        return (static_cast<uint64_t>(fg & 0xFFFFFF) << 32)
             | (static_cast<uint64_t>(bg & 0xFFFFFF) << 8)
             | static_cast<uint64_t>(attrs);
    }
};
```

The `constexpr` fluent API means you can build styles at compile time:

```cpp
constexpr auto heading_style = PackedStyle{}
    .with_fg(palette::blue)
    .with_bold();

// This is evaluated at compile time. Zero runtime cost.
// The style exists as a literal constant in the binary.
```

The hash function is **injective** for valid 24-bit colors — no collisions possible. This means the `unordered_map` lookup in `intern()` is guaranteed O(1) with no collision handling. Not amortized O(1). Actually O(1). Every time. Because the hash is a perfect mapping from the input space to the key space.

### Pre-computed ANSI Strings

When a style is interned, we immediately compute its full ANSI representation and store it alongside the style:

```cpp
std::string StylePool::compute_ansi(const PackedStyle& s) {
    std::string o;
    o.reserve(48);
    o += "\033[0m";  // always reset first
    if (s.attrs & attr::bold)          o += "\033[1m";
    if (s.attrs & attr::dim)           o += "\033[2m";
    if (s.attrs & attr::italic)        o += "\033[3m";
    if (s.attrs & attr::underline)     o += "\033[4m";
    if (s.attrs & attr::reverse)       o += "\033[7m";
    if (s.attrs & attr::strikethrough) o += "\033[9m";
    o += std::format("\033[38;2;{};{};{}m",
        (s.fg >> 16) & 0xFF, (s.fg >> 8) & 0xFF, s.fg & 0xFF);
    if (s.bg != palette::base)
        o += std::format("\033[48;2;{};{};{}m",
            (s.bg >> 16) & 0xFF, (s.bg >> 8) & 0xFF, s.bg & 0xFF);
    return o;
}
```

This runs *once* per unique style, at intern time. During rendering, we just look up the pre-computed string by ID — no formatting, no string building, no allocation. The cost is paid upfront and amortized across millions of uses.

## The Transition Cache

Our current `StylePool` pre-computes the full ANSI for each style. But Claude Code goes further — it caches the **transition** between pairs of styles. The difference matters more than you'd think:

```
// Full ANSI per style (naive approach):
// Style A: "\033[0m\033[1m\033[38;2;137;180;250m"
// Style B: "\033[0m\033[38;2;166;227;161m"
//
// To go from A to B, emit all of B's ANSI (reset + new style).
// That's ~35 bytes even though only the color changed.

// Transition cache (what Claude Code does):
// transition(A, B): "\033[22m\033[38;2;166;227;161m"
//
// Just close bold + change foreground. ~25 bytes.
// No redundant reset. No re-emitting unchanged attributes.
```

For a line with alternating bold/normal text, the transition cache saves ~40% of ANSI output bytes. Over a full screen diff, this compounds into significantly less data written to the terminal. Less data means less pty overhead, less terminal parsing, faster frame display.

### Our C++ Transition Cache

```cpp
class StylePool {
    // ... existing members ...

    // Transition cache: (from_id, to_id) -> minimal ANSI string
    std::unordered_map<uint32_t, std::string> transition_cache_;

public:
    const std::string& transition(StyleId from, StyleId to) {
        if (from == to) return empty_string_;

        uint32_t key = (static_cast<uint32_t>(from.raw) << 16) | to.raw;
        auto it = transition_cache_.find(key);
        if (it != transition_cache_.end()) return it->second;

        auto& result = transition_cache_[key];
        result = compute_transition(resolve(from), resolve(to));
        return result;
    }

private:
    static std::string compute_transition(const PackedStyle& from,
                                          const PackedStyle& to);
    static inline const std::string empty_string_;
};
```

The `compute_transition` function is where it gets interesting:

```cpp
std::string StylePool::compute_transition(const PackedStyle& from,
                                          const PackedStyle& to) {
    std::string o;
    o.reserve(32);

    // If any attribute was ON in 'from' but OFF in 'to', we must reset
    uint8_t removed = from.attrs & ~to.attrs;
    if (removed) {
        // SGR doesn't support turning off individual attributes reliably,
        // so reset and re-apply all of 'to'
        return compute_ansi(to);
    }

    // Otherwise: only emit what changed
    uint8_t added = to.attrs & ~from.attrs;
    if (added & attr::bold)          o += "\033[1m";
    if (added & attr::dim)           o += "\033[2m";
    if (added & attr::italic)        o += "\033[3m";
    if (added & attr::underline)     o += "\033[4m";
    if (added & attr::reverse)       o += "\033[7m";
    if (added & attr::strikethrough) o += "\033[9m";

    if (to.fg != from.fg)
        o += std::format("\033[38;2;{};{};{}m",
            (to.fg >> 16) & 0xFF, (to.fg >> 8) & 0xFF, to.fg & 0xFF);

    if (to.bg != from.bg) {
        if (to.bg == palette::base)
            o += "\033[49m";  // reset bg to default
        else
            o += std::format("\033[48;2;{};{};{}m",
                (to.bg >> 16) & 0xFF, (to.bg >> 8) & 0xFF, to.bg & 0xFF);
    }

    return o;
}
```

The bit arithmetic `from.attrs & ~to.attrs` computes which attributes were *removed*. If any attribute was turned off, we have to do a full reset — SGR (Select Graphic Rendition) doesn't reliably support turning off individual attributes across all terminals. `\033[22m` *should* turn off bold, but some terminals interpret it as "normal intensity" which also kills dim. So we reset and re-apply. It costs a few extra bytes, but it's correct on every terminal.

The common case — adding attributes or changing colors — takes the minimal path: only emit what actually changed. Syntax highlighting typically adds bold/italic when entering keywords or comments, which means the minimal path handles ~70% of transitions.

## Attribute Flags

```cpp
namespace attr {
    inline constexpr uint8_t none          = 0;
    inline constexpr uint8_t bold          = 0x01;
    inline constexpr uint8_t italic        = 0x02;
    inline constexpr uint8_t underline     = 0x04;
    inline constexpr uint8_t strikethrough = 0x08;
    inline constexpr uint8_t dim           = 0x10;
    inline constexpr uint8_t reverse       = 0x20;
}
```

`inline constexpr` ensures these are compile-time constants with no storage. Using a namespace instead of an `enum` avoids the need for casts when combining flags with `|` — a small ergonomic win that saves a surprising amount of typing and reading.

## The Catppuccin Mocha Palette

Both Claude Code and our implementation use Catppuccin Mocha as the default color scheme:

```cpp
namespace palette {
    inline constexpr uint32_t base     = 0x1E1E2E;  // dark background
    inline constexpr uint32_t text     = 0xCDD6F4;  // light foreground
    inline constexpr uint32_t blue     = 0x89B4FA;
    inline constexpr uint32_t green    = 0xA6E3A1;
    inline constexpr uint32_t red      = 0xF38BA8;
    inline constexpr uint32_t yellow   = 0xF9E2AF;
    inline constexpr uint32_t mauve    = 0xCBA6F7;
    // ... 20+ more colors
}
```

All `constexpr`. Zero runtime cost. The compiler substitutes the literal 24-bit values at every use site. There's no palette object in memory, no array to index into, no indirection. Just numbers baked into the instructions.

## The Full Interning Pipeline

Here's how it all fits together:

```
Character "A"  -->  cell.ch = U'A'          (direct, no pool)
Style           -->  StylePool::intern()  -->  StyleId{7}
                      |-- pre-computes ANSI string
                      |-- caches transitions lazily

Diff engine reads:
  cell.ch        -> char32_t (direct)
  cell.style_id  -> StyleId  -> transition string (cached)
```

The style pool is the only interning layer. Characters are stored directly. This gives us the same O(1) cell comparison as Claude Code (style comparison is still integer equality) while eliminating the character pool overhead entirely.

One layer of indirection. One hash map. One cache. That's the entire interning system. And it's responsible for more performance gains than any other single component in the pipeline.

---

**Next: [Part 5 — The Output Builder](/post/output-builder-recording-before-rendering)**
