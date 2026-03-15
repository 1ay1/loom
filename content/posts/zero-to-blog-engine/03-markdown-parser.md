---
title: "Zero to Blog Engine: Parsing Markdown by Hand"
date: 2024-07-01
slug: markdown-parser
tags: cpp, parsing, loom
excerpt: No regex, no libraries. Just a state machine that turns markdown into HTML one character at a time.
---

A blog engine needs to turn markdown into HTML. You could use a library, but where's the fun in that?

## The Approach

Markdown parsing is a two-phase process:

1. **Block parsing**: split input into paragraphs, headings, code blocks, lists
2. **Inline parsing**: within each block, handle bold, italic, links, code spans

Block boundaries are defined by blank lines. Everything else is inline formatting.

## Block Parser

Walk through lines, classify each one:

```cpp
for (const auto& line : lines)
{
    if (line.starts_with("# "))
        emit_heading(line, 1);
    else if (line.starts_with("```"))
        toggle_code_block();
    else if (line.starts_with("- ") || line.starts_with("* "))
        emit_list_item(line);
    else if (line.empty())
        close_paragraph();
    else
        append_paragraph(line);
}
```

The tricky parts:

- **Code blocks** are verbatim — no inline parsing inside them
- **Lists** can be nested (indentation matters)
- **Blank lines** close the current block but might also separate list items

## Inline Parser

Within a text block, scan for formatting markers:

```cpp
std::string parse_inline(const std::string& text)
{
    std::string html;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '*' && text[i+1] == '*')
        {
            html += in_bold ? "</strong>" : "<strong>";
            in_bold = !in_bold;
            ++i;
        }
        else if (text[i] == '`')
        {
            auto end = text.find('`', i + 1);
            html += "<code>" + text.substr(i+1, end-i-1) + "</code>";
            i = end;
        }
        else if (text[i] == '[')
        {
            // Parse [text](url)
            auto close = text.find(']', i);
            auto open_paren = text.find('(', close);
            auto close_paren = text.find(')', open_paren);
            // ... extract and emit <a> tag
        }
        else
        {
            html += text[i];
        }
    }
    return html;
}
```

## What About Edge Cases?

There are hundreds. Nested emphasis (`***bold and italic***`), links inside headings, code spans containing backticks, reference-style links. The CommonMark spec is 30,000 words long.

The pragmatic approach: handle the 90% case well, accept that pathological inputs will produce weird output. A blog author controls their own markdown — they'll just fix it if something looks wrong.

## Tables

GitHub-flavored markdown tables are the most satisfying feature to implement:

```
| Column A | Column B |
|----------|----------|
| data     | data     |
```

Parse the header row for column count, parse the separator for alignment, then emit `<table>`, `<thead>`, `<tbody>` with the right `text-align` styles.

## The Result

About 1,300 lines of code, zero dependencies, and a markdown parser that handles everything a tech blog needs. It's not CommonMark-compliant. It doesn't need to be.

Next: pre-rendering the entire site into an atomic cache for instant responses.
