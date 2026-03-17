---
title: Hand-Written Markdown Parser
date: 2024-01-15
slug: markdown-parser
tags: feature, architecture
excerpt: No dependencies means writing your own markdown parser. Here's what Loom's supports.
---

Loom includes a hand-written markdown-to-HTML parser. No regex engines, no external libraries — just a single-pass C++ parser.

## Supported Syntax #####

**Headings:** ATX (`# h1` through `###### h6`) and setext (underlined with `===` or `---`).

**Emphasis:** `*italic*`, `**bold**`, `***bold italic***`, `~~strikethrough~~`.

**Code:** inline `` `code` `` with backtick nesting, fenced code blocks with triple backticks or tildes, optional language identifier for syntax hinting.

**Links and images:** `[text](url)`, `[text](url "title")`, `![alt](url)`, reference-style `[text][ref]` with `[ref]: url` definitions.

**Lists:** unordered (`-`, `*`, `+`), ordered (`1.`, `2)`), task lists (`- [ ]`, `- [x]`), nested to arbitrary depth.

**Tables:** pipe-delimited with column alignment (`:---`, `:---:`, `---:`).

**Blockquotes:** `>` prefix, nested supported.

**Footnotes:** `[^id]` references with `[^id]: text` definitions. Rendered at the bottom with back-references.

**Other:** horizontal rules (`---`, `***`, `___`), autolinks (`<url>`), backslash escapes, raw HTML passthrough, line breaks via trailing spaces.

## What's Not Supported

No LaTeX math, no syntax highlighting (code blocks are plain `<pre><code>`), no embedded media beyond images. These could be added but aren't needed for a text-focused blog.
