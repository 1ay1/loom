---
title: Markdown Rendering Overhaul — Heading Anchors, Fence Fixes, and Performance
date: 2026-03-03
slug: markdown-rendering-overhaul
tags: changelog, markdown, rendering, release
excerpt: Major improvements to the markdown parser — proper fenced code block handling, heading IDs with anchor links, lazy image loading, and performance tuning across the board.
---

This release overhauls Loom's markdown-to-HTML parser with correctness fixes, new rendering features, and performance improvements.

## Fenced Code Block Fixes

The parser now correctly tracks fence length and character type. A block opened with four backticks (``````) only closes with four or more backticks — inner triple-backtick lines are treated as content, not as closing fences. This was the most visible bug: documentation posts that showed code block examples inside code blocks were rendering broken HTML.

The closing fence detection also now requires the line to contain only fence characters and optional whitespace. A line like ` ``` → Fenced code block` no longer prematurely ends a block.

Before:

````
```
Line with ``` in it was treated as a closing fence
More content here was rendered as regular markdown
```
````

After: the entire block renders correctly as a single code block.

## Pass 1 Skips Code Blocks

Reference definitions (`[key]: url`) and footnote definitions (`[^id]: content`) inside fenced code blocks are no longer collected during the first pass. Previously, a code example showing footnote syntax would register as a real footnote definition, causing phantom footnote sections to appear at the bottom of the page.

## Heading IDs and Anchor Links

All headings now receive slug-based `id` attributes derived from their text content. Duplicate headings get `-1`, `-2` suffixes automatically. Each heading includes an anchor link (`#`) that appears on hover, making it easy to link directly to any section.

```
## Block-Level Parsing
```

Renders as:

```html
<h2 id="block-level-parsing">
  <a class="heading-anchor" href="#block-level-parsing">#</a>
  Block-Level Parsing
</h2>
```

This is essential for documentation navigation — readers can now right-click any heading to copy a direct link.

## Image Lazy Loading

All images rendered from markdown now include `loading="lazy"`, which defers off-screen image loading until the user scrolls near them. This reduces initial page weight without any configuration.

## Table Separator Validation

The table separator detection now requires at least 3 dashes per cell (previously accepted 1). This matches the CommonMark specification and prevents false table detection on content that happens to contain pipes.

## Performance Improvements

Several changes reduce allocations and improve throughput:

- **`std::unordered_map` for references** — O(1) lookups instead of O(log n) for link and image reference resolution.
- **Output buffer pre-allocation** — `process_inline` reserves 1.5x the input size; `process_blocks` reserves based on line count. Eliminates repeated reallocations for larger documents.
- **`constexpr` punctuation lookup table** — A 128-byte array replaces a 32-branch `||` chain for backslash escape detection.
- **Batch code block escaping** — Code block content is accumulated as raw text and HTML-escaped in a single pass, rather than escaping line by line.

These changes are most noticeable on large documents with many code blocks and reference links, though the parser was already fast (under 1ms per typical post).
