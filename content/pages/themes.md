---
title: Themes
slug: themes
---

## Built-in Themes

Set `theme = name` in `site.conf`. All 5 themes support dark and light mode. Changes take effect instantly via hot reload.

| Theme | Font | Style |
|-------|------|-------|
| `default` | System sans-serif | Clean, neutral grays, blue accent, soft corners |
| `terminal` | Monospace | Dark hacker aesthetic, green accent, sharp corners, custom date format |
| `nord` | Inter / sans-serif | Arctic frost palette, muted blues, soft glow on card hover |
| `gruvbox` | System sans-serif | Retro groove, warm earthy contrast, orange accent, bordered tags |
| `rose` | System sans-serif | Soft pinks, magenta accent, pill-shaped nav and tags, round corners |

---

## Choosing the Right Theme

**Technical blog with lots of code?** Try `terminal` — monospace font and a dark aesthetic give code blocks room to breathe.

**Want a familiar editor palette?** Try `nord` or `gruvbox` — your readers will feel at home.

**Want something soft and elegant?** Try `rose` — round corners, pill-shaped navigation, and a warm magenta accent.

**Playing it safe?** `default` works for everyone.

---

## How Themes Differ

Themes aren't just color palettes. Each theme defines **structural choices** that change how UI components look and behave:

| Property | What it controls | Example variants |
|----------|-----------------|-----------------|
| Corners | Border-radius across all elements | Soft (rounded), Sharp (square), Round (pill-shaped) |
| Tag style | Tag badge appearance | Pill, Rectangle, Bordered, Outlined, Plain text |
| Link style | Content link decoration | Solid underline, Dotted, Dashed, None |
| Code blocks | Code block treatment | Plain background, Full border, Left accent stripe |
| Blockquotes | Blockquote left border | Accent-colored, Muted |
| Headings | Heading text transform | Normal, UPPERCASE, lowercase |

Compare how `terminal` and `rose` use the same structural system to achieve completely different feels:

| Property | terminal | rose |
|----------|----------|------|
| Corners | Sharp (square) | Round (pill-shaped) |
| Tags | Bordered rectangles | Pill badges |
| Nav style | Default | Pills |
| Card hover | Border highlight | Lift effect |
| Links | No decoration | Underline |
| Accent | Green (#5fba7d) | Magenta (#c2185b) |

Same theme system, radically different UI.

Themes can also override content-level defaults like date formatting and index headings — the terminal theme uses `"%b %d"` dates and a lowercase "posts" heading without needing any component overrides.

---

## Customizing a Theme

### Step 1: Override Variables

Every theme is built on CSS custom properties. Override any from `site.conf` without writing CSS:

```
theme = nord
theme_accent = #bf616a
theme_dark-accent = #bf616a
theme_font_size = 18px
```

The naming rule: `theme_` + variable name, hyphens as underscores. Prefix with `dark-` for dark-mode-only.

### Step 2: Available Variables

**Colors** — the five core tokens every theme defines:

| Variable | Controls |
|----------|----------|
| `bg` | Page background |
| `text` | Primary text |
| `muted` | Dates, secondary text, meta |
| `border` | Lines, separators, card borders |
| `accent` | Links, active states, highlights |

**Typography:**

| Variable | Default |
|----------|---------|
| `font` | Varies by theme |
| `font-size` | 14px–18px |
| `heading-font` | `inherit` |
| `heading-weight` | `700` |
| `line-height` | `1.7` |
| `code-font` | Monospace stack |

**Layout:**

| Variable | Default |
|----------|---------|
| `max-width` | 640px–820px |
| `content-width` | Same as max-width |
| `sidebar-width` | `260px` |
| `sidebar-gap` | `32px` |
| `container-padding` | `40px 20px` |

**Components:**

| Variable | Controls |
|----------|----------|
| `border-radius` | Global corner rounding |
| `card-bg`, `card-border`, `card-radius`, `card-padding` | Post cards |
| `tag-bg`, `tag-text`, `tag-radius` | Tag badges |
| `tag-hover-bg`, `tag-hover-text` | Tag hover state |
| `header-border-width` | Header line (set `0` to hide) |
| `footer-border-width` | Footer line |
| `link-decoration` | Link underline style |
| `accent-hover` | Accent color on hover |

Every variable has a `dark-` counterpart for independent dark mode control.

### Step 3: Custom CSS (optional)

For changes beyond variables, add `custom_css` in `site.conf`:

```
custom_css = .post-content blockquote { border-left-color: orange; font-style: normal; }
```

This is injected after the theme and overrides it.

### Step 4: Google Fonts (optional)

Load an external font with `custom_head_html`, then reference it:

```
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono&display=swap" rel="stylesheet">
theme_code_font = JetBrains Mono, monospace
```

---

## Full CSS Override

For total control, drop a `style.css` in `content/theme/`:

```
content/
  theme/
    style.css    # Replaces the entire default stylesheet
```

This replaces all built-in CSS. You own everything: layout, typography, responsive breakpoints, dark mode. The theme you set in `site.conf` still applies on top (its variable overrides and extra CSS), so you can combine a custom base with a built-in theme's color palette.

---

## How Theme Injection Works

Every page is a single HTML document. CSS is injected in this order:

1. **Base CSS** — full layout, typography, responsive rules (or your `style.css` override)
2. **Theme** — color variables + extra CSS from the selected theme
3. **Variable overrides** — your `theme_*` keys from `site.conf`
4. **Custom CSS** — your `custom_css` value

Each layer overrides the previous. Dark mode uses `[data-theme="dark"]` selectors — a tiny inline script in `<head>` detects the user's preference before paint, so there's no flash.

---

## Recipes

### Make any theme wider with a larger font

```
theme = gruvbox
theme_max_width = 860px
theme_font_size = 18px
```

### Remove all border-radius

```
theme = default
theme_border_radius = 0
theme_card_radius = 0
theme_tag_radius = 0
```

### Hide the header and footer borders

```
theme_header_border_width = 0
theme_footer_border_width = 0
```

### Use a different heading font

```
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=Playfair+Display:wght@700&display=swap" rel="stylesheet">
theme_heading_font = Playfair Display, Georgia, serif
```

### Dark background, light text (force dark feel in light mode)

```
theme_bg = #0f0f0f
theme_text = #e0e0e0
theme_muted = #888888
theme_border = #222222
```
