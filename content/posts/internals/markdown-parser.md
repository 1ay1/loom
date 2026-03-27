---
title: Hand-Written Markdown Parser — Two Passes, No Regex
date: 2025-09-22
slug: markdown-parser
tags: internals, architecture, cpp
excerpt: Loom's markdown parser is ~1200 lines of C++ with no regex engine and no external library. A first pass collects references and footnotes; a second pass renders HTML.
---

Loom includes a hand-written markdown-to-HTML parser. No regex engine, no external libraries — just a character-by-character C++ parser with two passes over the input.

## Why Hand-Write a Parser

Two reasons: zero dependencies, and control. A library like `cmark` or `discount` would work, but adds a compile-time dependency and a foreign code path. Writing the parser means we can add Loom-specific extensions (the `^` new-tab sigil, for instance) without patching a library.

A markdown parser is also a good size for a hand-written implementation. The syntax is mostly context-free, the edge cases are known, and the input is trusted (your own content directory). You don't need the robustness of a general-purpose parser.

## Two-Pass Architecture

The parser works in two passes over the input:

**Pass 1 — Reference collection.** Scan the whole document for reference definitions and footnote definitions. Store them in maps.

```cpp
RefMap refs;          // [key]: url "title"
FootnoteMap footnotes; // [^id]: content

for (const auto& line : lines) {
    std::string key;
    RefDef def;
    if (parse_ref_def(line, key, def)) refs[key] = def;

    std::string fn_id, fn_content;
    if (parse_footnote_def(line, fn_id, fn_content))
        footnotes[fn_id] = {fn_id, fn_content};
}
```

**Pass 2 — Rendering.** Process blocks and inline elements, using the reference maps from pass 1.

The two-pass design means forward references work. A link like `[see below][conclusion]` with the reference `[conclusion]: /post/conclusion` defined later in the file resolves correctly.

## Block-Level Parsing

`process_blocks` handles the document structure. It walks the lines array, identifies block types by their leading characters, and either renders them directly or recurses.

```
Line starts with    →  Block type
#                   →  ATX heading
=== or ---          →  Setext heading (previous line is text)
```                  →  Fenced code block (until matching ```)
>                   →  Blockquote (recurse)
- / * / +           →  Unordered list item
1. / 1)             →  Ordered list item
- [ ] / - [x]       →  Task list item
| ... |             →  Table row
---/***             →  Horizontal rule
[blank line]        →  Paragraph boundary
<div/script/...>    →  Raw HTML block (passthrough until blank line)
otherwise           →  Paragraph text
```

Paragraphs accumulate lines until a blank line or a non-paragraph block type. The accumulated text goes to `process_inline`.

## Inline-Level Parsing

`process_inline` is a character-by-character scanner over a single line or paragraph of text. It handles the inline formatting:

**Code spans:** `` ` `` starts a code span. The parser handles variable-length backtick fences (`` ` ``, ` `` `, ` ``` `) so backticks can appear inside code spans.

**Emphasis and strong:**
- `*text*` or `_text_` → `<em>`
- `**text**` or `__text__` → `<strong>`
- `***text***` → `<strong><em>`
- `_` within words is not emphasis (prevents `foo_bar_baz` from rendering oddly)

**Strikethrough:** `~~text~~` → `<del>`

**Links:**
```
[text](url)              inline link
[text](url "title")      with title attribute
[text][ref]              reference link
[text]                   shortcut reference
[text](url)^             new-tab link (Loom extension)
[text][ref]^             new-tab reference link
```

**Images:**
```
![alt](url)              inline image
![alt](url "title")      with title
![alt][ref]              reference image
```

**Autolinks:** `<https://example.com>` → `<a href="https://example.com">`

**Footnote references:** `[^id]` → `<sup class="footnote-ref"><a href="#fn-id">id</a></sup>`

**Backslash escapes:** `\*`, `\[`, `\_`, etc. suppress formatting.

**Line breaks:** Two trailing spaces + newline → `<br>`.

## The `^` New-Tab Extension

Appending `^` after a link opens it in a new tab:

```markdown
[epoll man page](https://man7.org/linux/man-pages/man7/epoll.7.html)^
```

Renders as:
```html
<a href="..." target="_blank" rel="noopener noreferrer">epoll man page</a>
```

This is handled in `process_inline` at the point where the closing `)` is found. If the next character is `^`, `target="_blank" rel="noopener noreferrer"` is added and the cursor advances past `^`.

Works on all three link forms: inline `[text](url)^`, reference `[text][ref]^`, shortcut `[text]^`.

## Tables

Pipe-delimited tables with a separator row:

```markdown
| Left | Center | Right |
|:-----|:------:|------:|
| a    |   b    |     c |
```

The separator row's colon positions determine column alignment:
- `:---` → `text-align: left`
- `:---:` → `text-align: center`
- `---:` → `text-align: right`
- `---` → no alignment attribute

## Footnotes

```markdown
Epoll[^1] is efficient for high-connection-count servers.

[^1]: Linux epoll scales to millions of file descriptors with O(1) event notification.
```

Footnote references `[^1]` render inline as superscript links. Footnote definitions are collected in pass 1 and rendered at the end of the document as a `<section class="footnotes">` with back-reference links.

## Fenced Code Blocks

````markdown
```cpp
template<typename T>
concept ContentSource = requires(T s) {
    { s.all_posts() } -> std::same_as<std::vector<Post>>;
};
```
````

The language identifier after the opening fence is captured but currently used only as a CSS class hint (`<code class="language-cpp">`). Syntax highlighting would require a second library — Loom leaves it to CSS or a client-side highlighter if needed.

Fenced blocks can use either backticks or tildes. The opening fence length determines the closing fence — a ` ``` ` block can contain `\`\`\`` without ending the block; it needs `\`\`\`` to close.

## What's Not Supported

**LaTeX math.** `$x^2 + y^2 = z^2$` is rendered as plain text. Add KaTeX or MathJax via `custom_head_html` if needed.

**Syntax highlighting.** Code blocks are `<pre><code class="language-X">`. Client-side: include Prism.js via `custom_head_html`. Server-side: would need a highlighter library.

**Definition lists.** `term\n: definition` CommonMark extension — not implemented.

**Embedded HTML forms, iframes.** Raw HTML blocks are passed through verbatim, so `<iframe>` and `<form>` work if you write them as HTML. They're not a markdown extension.

## Performance

The parser is called once per file at cache build time, not per request. A 2000-word post takes under a millisecond to parse. For a blog with 200 posts, the total parse time is a fraction of a second. Once in the cache, the rendered HTML is served directly.

The hand-written approach avoids the overhead of general-purpose regex engines and external library call stacks. More importantly, it means the parser is always available: no `apt install`, no version conflict, no breaking change in a library update.
