---
title: "The Markdown Parser — 1200 Lines, Two Passes, No Dependencies"
date: 2025-12-05
slug: markdown-parser
tags: [loom-internals, c++20, parsing, markdown]
excerpt: "A hand-written markdown-to-HTML converter in 1200 lines of C++. Two passes, zero dependencies, custom extensions, and sub-millisecond performance."
---

Loom's markdown parser is hand-written. No CommonMark library, no third-party dependency, no regex engine. Just 1,445 lines of C++ in `src/util/markdown.cpp` that convert markdown to HTML in two passes.

This might seem like a bad idea. Why not use cmark, or pulldown-cmark via FFI, or any of the dozen markdown libraries out there? Because Loom needs three things that most libraries do not give you: zero dependencies (not even a system library), custom extensions (the `^` new-tab link syntax), and complete control over the output HTML. When your blog engine's entire value proposition is that everything is compiled C++ with no runtime overhead, pulling in a C library kind of undermines the point.

This post builds primarily on [post #6 on string_view](/post/string-view-and-zero-copy), since the parser is essentially a string processing machine.

## Two-Pass Architecture

The parser works in two passes over the input.

**Pass 1** scans for reference link definitions and footnote definitions. These are lines like:

```markdown
[example]: https://example.com "Example Site"
[^note]: This is a footnote.
```

They can appear anywhere in the document, but they need to be resolved before any links that reference them are encountered. The first pass collects them into lookup maps:

```cpp
struct RefDef
{
    std::string url;
    std::string title;
};
using RefMap = std::unordered_map<std::string, RefDef>;

struct FootnoteDef
{
    std::string id;
    std::string content;
};
```

Reference keys are case-insensitive (converted to lowercase with a simple `to_lower` function). Footnote IDs are stored as-is.

**Pass 2** processes the document line by line, parsing blocks and inline elements. Reference links and footnotes are resolved using the maps from pass 1.

This two-pass design is the simplest way to handle forward references. In markdown, you can write `[text][ref]` before the `[ref]: url` definition appears. A single-pass parser would need backtracking or deferred resolution. Two passes make it straightforward.

## Block Parsing

Pass 2 walks through lines, classifying each one to determine what kind of block it starts or continues. The classification functions are simple string inspections:

```cpp
static int atx_heading_level(const std::string& s)
{
    auto t = ltrim(s);
    if (t.empty() || t[0] != '#') return 0;
    int level = 0;
    while (level < (int)t.size() && t[level] == '#') level++;
    if (level > 6) return 0;
    if (level < (int)t.size() && t[level] != ' ' && t[level] != '\t') return 0;
    return level;
}
```

Count the `#` characters, verify there is a space after them, return the heading level (1-6) or 0 if it is not a heading. No regex, no state machine — just character counting.

The block types handled are:

- **ATX headings** (`# ... ######`): detected by leading `#` characters
- **Setext headings** (`===` or `---` underlines): detected by looking ahead one line
- **Fenced code blocks** (triple backticks or tildes): tracked with a state variable for the fence character and length
- **Blockquotes** (`>` prefix): stripped and recursively processed
- **Unordered lists** (`-`, `*`, `+` markers): detected with `is_ul_item()`
- **Ordered lists** (`1.`, `2)` etc.): detected with `is_ol_item()`
- **Tables** (pipe-separated): detected by the separator row (`|---|---|`)
- **Horizontal rules** (`---`, `***`, `___`): three or more marker characters
- **Raw HTML** (lines starting with `<`): passed through unmodified
- **Paragraphs**: everything else

The detection functions for lists are careful to distinguish markers from other constructs:

```cpp
static size_t is_ul_item(const std::string& s)
{
    auto t = ltrim(s);
    if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ')
    {
        // Make sure it's not a horizontal rule
        if (t[0] == '-' || t[0] == '*')
        {
            bool all_same = true;
            for (size_t i = 0; i < t.size(); i++)
                if (t[i] != t[0] && t[i] != ' ') { all_same = false; break; }
            size_t marker_count = 0;
            for (char c : t) if (c == t[0]) marker_count++;
            if (all_same && marker_count >= 3) return 0;  // horizontal rule
        }
        return leading_spaces(s) + 2;
    }
    return 0;
}
```

A line like `- item` is a list item. A line like `---` is a horizontal rule. The function checks for the horizontal rule case before declaring it a list item.

### Fenced Code Blocks

Code fence handling tracks the opening fence to match it correctly:

```cpp
static bool is_fenced_code(const std::string& s, std::string& lang,
                            char& fence_char, size_t& fence_len)
{
    auto t = ltrim(s);
    if (t.size() >= 3 && (t[0] == '`' || t[0] == '~'))
    {
        char ch = t[0];
        size_t count = 0;
        while (count < t.size() && t[count] == ch) count++;
        if (count >= 3)
        {
            fence_char = ch;
            fence_len = count;
            lang = trim(t.substr(count));
            return true;
        }
    }
    return false;
}
```

The closing fence must use the same character and be at least as long. This handles both backtick and tilde fences, and correctly ignores shorter sequences within the block.

Content inside code blocks is HTML-escaped but otherwise unmodified — no inline processing.

### Blockquotes and Lists

Blockquotes are handled by stripping the `>` prefix and recursively processing the inner content:

```cpp
static std::string strip_blockquote(const std::string& s)
{
    auto t = ltrim(s);
    if (t.empty() || t[0] != '>') return s;
    t = t.substr(1);
    if (!t.empty() && t[0] == ' ') t = t.substr(1);
    return t;
}
```

The stripped lines are collected and then passed back through `process_blocks()` recursively. This means blockquotes can contain any block-level element, including other blockquotes and code blocks.

Lists use a similar approach — collect continuation lines based on indentation, then process each item's content.

### Task Lists

The parser detects checkbox syntax within list items:

```cpp
static bool is_task_item(const std::string& text, bool& checked, std::string& rest)
{
    if (text.size() >= 3 && text[0] == '[' && text[2] == ']')
    {
        if (text[1] == 'x' || text[1] == 'X') { checked = true; }
        else if (text[1] == ' ') { checked = false; }
        else return false;
        rest = text.size() > 3 ? text.substr(4) : "";
        return true;
    }
    return false;
}
```

`[x]` becomes a checked checkbox, `[ ]` becomes unchecked. The list item gets a special CSS class for styling.

## Inline Parsing

After blocks are identified, their text content goes through `process_inline()`. This is a single function that walks through the text character by character, handling:

- **Backslash escapes**: `\*` becomes a literal `*`
- **Code spans**: `` `code` `` with matching backtick counts
- **HTML passthrough**: `<tags>` are passed through unescaped
- **Autolinks**: `<https://example.com>` becomes a clickable link
- **Images**: `![alt](url)` and `![alt][ref]`
- **Footnote references**: `[^id]` becomes a superscript link
- **Links**: three forms (inline, reference, collapsed reference)
- **Emphasis**: `*italic*`, `**bold**`, `***bold italic***`

The emphasis parsing is the trickiest part. It follows CommonMark's flanking delimiter rules:

```cpp
static bool is_punctuation(char c)
{
    static constexpr bool table[128] = {
        // ... 128-entry lookup table
    };
    auto uc = static_cast<unsigned char>(c);
    return uc < 128 && table[uc];
}
```

A `constexpr` lookup table for punctuation classification, indexed by ASCII value. The flanking rules determine whether `*` opens or closes emphasis based on the surrounding characters — whitespace and punctuation affect the behavior differently.

### The ^ New-Tab Extension

Links can be marked for new-tab opening with a `^` suffix:

```markdown
[text](url)^
```

This is a Loom-specific extension. The parser checks for `^` after the closing `)` of an inline link:

When detected, the generated `<a>` tag includes `target="_blank" rel="noopener"`. This is a small quality-of-life feature for blog posts that link to external resources — you do not want your reader to lose their place.

### Heading IDs

Every heading gets an auto-generated `id` attribute for anchor linking:

```cpp
static std::string slugify(const std::string& text)
{
    std::string slug;
    slug.reserve(text.size());
    bool prev_dash = true;
    for (char c : text)
    {
        auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
        {
            slug += static_cast<char>(std::tolower(uc));
            prev_dash = false;
        }
        else if (!prev_dash)
        {
            slug += '-';
            prev_dash = true;
        }
    }
    if (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug;
}
```

Duplicate headings get a numeric suffix via `make_heading_id()`:

```cpp
static std::string make_heading_id(const std::string& text,
                                   std::unordered_map<std::string, int>& seen)
{
    auto slug = slugify(text);
    if (slug.empty()) slug = "heading";
    auto it = seen.find(slug);
    if (it == seen.end())
    {
        seen[slug] = 1;
        return slug;
    }
    auto id = slug + "-" + std::to_string(it->second);
    it->second++;
    return id;
}
```

Two headings with the same text get `#my-heading` and `#my-heading-1`.

## String Utilities

The parser relies on a handful of string helpers that work without allocations where possible:

```cpp
static std::string trim(const std::string& s);
static std::string ltrim(const std::string& s);
static size_t leading_spaces(const std::string& s);
static std::string strip_leading(const std::string& s, size_t count);
static std::string escape_html(const std::string& s);
```

`escape_html` is called frequently — every text node passes through it:

```cpp
static std::string escape_html(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            default:   out += c;
        }
    }
    return out;
}
```

Simple character-by-character replacement. The `reserve` call avoids reallocations for the common case where the output is the same size as the input (no special characters).

## Performance

The markdown parser runs once per post at build time. For a typical blog post (2,000 words, a few code blocks, some links), parsing takes well under a millisecond. Even for the longest posts, it stays sub-millisecond on modern hardware.

This is not surprising. The parser does a single allocation per output string (plus the intermediate data structures). There is no regex engine, no backtracking, no dynamic dispatch. It is a linear scan through the input with some nested linear scans for things like emphasis matching.

The parser does not need to be fast — it runs once during the build phase and its output is cached forever (until the source changes). But being fast means the build phase is fast, which means hot reload feels instant. Change a markdown file, save, and the new HTML appears in under 50 milliseconds. Most of that time is spent in the rest of the pipeline (component rendering, minification, gzip compression), not in parsing.

## What It Does Not Do

This is not a CommonMark-compliant parser. It handles the markdown constructs that appear in real blog posts, which is a much smaller set than the full CommonMark specification. Things it does not handle:

- **Lazy continuation lines** in blockquotes and lists (each line must have its marker)
- **Link reference definitions** can only appear at the document level, not inside blocks
- **Setext heading underlines** must be at least two characters
- **Indented code blocks** (only fenced code blocks are supported)

These are deliberate simplifications. The full CommonMark spec is complex — the reference implementation is thousands of lines of C. Loom's parser handles the markdown that people actually write in blog posts, and it handles it correctly.

The custom extensions (new-tab links, auto-generated heading IDs, footnotes) are more useful to a blog engine than full spec compliance. And because the parser is 1,445 lines of code with no dependencies, it is easy to read, easy to modify, and easy to debug when something does not render as expected.

## Why Hand-Write It?

Three reasons:

1. **Zero dependencies.** Loom compiles with nothing but a C++20 compiler and the standard library. No vcpkg, no conan, no system packages. Adding a markdown library would be the first external dependency, and it would pull in either a C library (with its own build system) or a C++ library (with its own template weight).

2. **Full control over output.** The parser generates exactly the HTML that Loom's component system and CSS expect. No extra wrapper divs, no unexpected class names, no generated IDs that clash with the theme system. Every heading gets a consistent ID scheme. Every link can be extended with custom attributes.

3. **No abstraction boundary.** When a markdown edge case does not render correctly, the fix is in the same codebase, in the same language, with the same build system. No upstream issues, no version pinning, no "works in 2.3 but not 2.4" surprises. The parser is part of Loom, not a dependency of Loom.

The cost is that someone had to write 1,445 lines of string processing code. But that code is straightforward — no clever algorithms, no complex data structures, just careful character-by-character parsing. Anyone who can read C++ can read this parser.
