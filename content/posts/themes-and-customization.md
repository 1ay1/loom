---
title: "Theming Loom: 22 Themes, CSS Variables, and Full Control"
date: 2025-11-03
slug: themes-and-customization
tags: feature, themes, tutorial
excerpt: "A complete guide to Loom's theming system: pick a built-in theme, override individual variables, inject custom CSS, or replace the stylesheet entirely."
---

Loom ships with 6 built-in themes. Every theme supports light and dark mode, switches instantly via hot reload, and ships inlined — zero external requests.

This post walks through every layer of customization, from the simplest one-line change to building your own theme from scratch.

## Pick a Theme

Open `site.conf` and set the `theme` key:

```
theme = gruvbox
```

Save. If Loom is running, the change is live immediately — no restart needed.

Here's every available theme:

| Theme | Vibe |
|-------|------|
| `default` | Clean, neutral grays, blue accent |
| `terminal` | Monospace hacker aesthetic, green accent, sharp corners |
| `nord` | Arctic frost palette, muted blues |
| `gruvbox` | Retro groove, warm earthy contrast, orange accent |
| `rose` | Soft pinks, magenta accent, round corners |
| `hacker` | CRT phosphor green-on-black, scanlines, custom components |

Each theme defines a light palette, a dark palette, a font stack, font size, and content width. But themes go beyond colors — they also define **structural choices** that change the shape and behavior of UI components:

- **Corners** — Soft (rounded), Sharp (square), or Round (pill-shaped). Affects every element site-wide.
- **Tag style** — Pill badges, bordered rectangles, outlined with muted borders, or plain text.
- **Link style** — Solid underline, dotted, dashed, or none.
- **Headings** — Normal, UPPERCASE, or lowercase.

All code styling is palette-driven — code blocks use `var(--text)` on a subtle `var(--bg)` tint, inline code uses `var(--accent)` on an accent-tinted background. Define your five palette colors and code rendering works in both light and dark mode automatically. No separate light/dark paths needed.

This is why `terminal` and `rose` feel completely different — terminal uses sharp corners, bordered tags, and no link decoration, while rose uses round corners, pill-shaped tags, and pill-shaped nav links. Themes can also override content defaults like date format and index headings without writing component code.

## Override Individual Variables

You don't need to write CSS. Every theme is built on CSS custom properties. Override any of them from `site.conf`:

```
theme = nord
theme_accent = #e06c75
theme_dark-accent = #e06c75
```

The naming rule is simple: `theme_` + the variable name, with hyphens written as underscores. For dark-mode-only overrides, prefix the variable with `dark-`:

```
theme_font_size = 18px        # affects both modes
theme_max_width = 800px       # affects both modes
theme_dark-bg = #000000       # only changes dark mode background
```

### What Can You Override?

Everything. Here are the most useful variables:

**Colors** — the five core tokens:

| Variable | What it controls |
|----------|-----------------|
| `bg` | Page background |
| `text` | Primary text |
| `muted` | Dates, meta text, secondary content |
| `border` | Lines, separators, card borders |
| `accent` | Links, active states, highlights |

**Typography:**

| Variable | Default | What it controls |
|----------|---------|-----------------|
| `font` | System sans-serif | Body font stack |
| `font-size` | 17px | Base text size |
| `heading-font` | `inherit` | Heading font (set to a different family) |
| `heading-weight` | 700 | Heading boldness |
| `line-height` | 1.7 | Text line spacing |
| `code-font` | Monospace stack | Font for code blocks |

**Layout:**

| Variable | Default | What it controls |
|----------|---------|-----------------|
| `max-width` | 700px | Content column width |
| `content-width` | Same as max-width | Overall container width |
| `sidebar-width` | 260px | Sidebar column width |
| `sidebar-gap` | 32px | Space between content and sidebar |
| `container-padding` | 40px 20px | Page padding |

**Components:**

| Variable | What it controls |
|----------|-----------------|
| `border-radius` | Default corner rounding |
| `card-bg`, `card-border`, `card-radius`, `card-padding` | Post cards (when using `post_list_style = cards`) |
| `tag-bg`, `tag-text`, `tag-radius` | Tag badges |
| `tag-hover-bg`, `tag-hover-text` | Tag hover state |
| `header-border-width` | Header bottom line (set `0` to remove) |
| `footer-border-width` | Footer top line |
| `link-decoration` | Link underline style |

Every variable has a `dark-` counterpart. Override `bg` to change the light background; override `dark-bg` to change the dark background independently.

### Example: Nord with Warm Accents

```
theme = nord
theme_accent = #bf616a
theme_dark-accent = #bf616a
theme_heading_font = Georgia, serif
theme_heading_weight = 600
```

This gives you Nord's arctic palette with rosy accent links and serif headings. Three lines, no CSS.

### Example: Make Any Theme Wider

```
theme = nord
theme_max_width = 900px
theme_font_size = 16px
```

## Custom CSS

For changes that go beyond variables — say you want to restyle blockquotes or add a hover animation — use `custom_css` in `site.conf`:

```
custom_css = .post-card:hover { transform: scale(1.02); } .post-content blockquote { border-left-color: orange; }
```

This CSS is injected *after* the theme, so it wins any specificity ties. It's a single line in `site.conf` — keep it short. For anything longer, use a full CSS override.

## Full CSS Override

Drop a `style.css` file in `content/theme/`:

```
content/
  theme/
    style.css
```

This *replaces* the entire default stylesheet. You're starting from zero — you own all layout, typography, responsive design, and dark mode handling. Loom detects the file automatically and stops using the built-in CSS.

This is the nuclear option. Most people never need it. Variable overrides and `custom_css` cover 95% of cases.

## Injecting External Resources

Need Google Fonts, analytics, or custom meta tags? Use `custom_head_html`:

```
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
```

Then reference the font via a variable override:

```
theme_font = Inter, system-ui, sans-serif
```

This is the only way to make an external request — Loom itself never reaches out to third-party servers.

## How It All Works

Every page Loom serves is a single self-contained HTML document. Here's the CSS injection order:

1. **Default CSS** — the full base stylesheet with all layout, typography, and responsive rules
2. **Theme CSS** — the selected theme's color variables and extra CSS (overrides defaults)
3. **Variable overrides** — your `theme_*` keys from `site.conf` (overrides the theme)
4. **Custom CSS** — your `custom_css` line (overrides everything)

If you drop a `style.css` file in `content/theme/`, step 1 is replaced entirely — your file is used instead of the built-in CSS. Steps 2-4 still apply on top.

Dark mode works via a `data-theme="dark"` attribute on `<html>`. A tiny inline script in `<head>` checks `localStorage` first (for returning visitors), then falls back to `prefers-color-scheme`, then defaults to light. The script runs before paint — no flash of the wrong theme.

The toggle button swaps the attribute and saves the choice to `localStorage`. Everything is CSS — no JavaScript framework, no runtime, no FOUC.

## Quick Reference

| I want to... | Do this |
|--------------|---------|
| Change the theme | `theme = name` in `site.conf` |
| Change one color | `theme_accent = #hexvalue` |
| Change dark mode only | `theme_dark-accent = #hexvalue` |
| Change the font | `theme_font = YourFont, fallback` |
| Make it wider | `theme_max_width = 900px` |
| Remove header border | `theme_header_border_width = 0` |
| Restyle a component | `custom_css = .selector { ... }` |
| Add Google Fonts | `custom_head_html = <link ...>` |
| Replace everything | Drop `content/theme/style.css` |
