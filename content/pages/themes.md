---
title: Themes
slug: themes
---

## Built-in Themes

Set `theme = name` in `site.conf`. All 22 themes support dark and light mode. Changes take effect instantly via hot reload.

### Classic

| Theme | Font | Style |
|-------|------|-------|
| `default` | System sans-serif | Clean, neutral grays, blue accent |
| `serif` | Georgia / Garamond | Editorial, warm tones, literary feel |
| `mono` | Monospace | Terminal aesthetic, green accent |
| `typewriter` | Courier New | Old-school ink-on-paper, dashed borders, uppercase headers |
| `brutalist` | System sans-serif | Anti-design, bold borders, red accent, inverted tags |

### Editor Palettes

| Theme | Font | Style |
|-------|------|-------|
| `nord` | Inter / sans-serif | Arctic palette, muted blues |
| `solarized` | System sans-serif | Ethan Schoonover's precision-engineered palette |
| `dracula` | System sans-serif | Dark-first purple, pink highlights |
| `gruvbox` | System sans-serif | Retro groove, warm earthy contrast, orange accent |
| `catppuccin` | System sans-serif | Soothing pastels, purple accent |
| `tokyonight` | System sans-serif | Neon-tinged Tokyo evening palette |
| `kanagawa` | Charter / Georgia | Inspired by Hokusai's The Great Wave |

### Warm & Earthy

| Theme | Font | Style |
|-------|------|-------|
| `earth` | Charter / Georgia | Warm organic tones, olive accent |
| `warm` | Georgia / Charter | Golden amber palette, cozy serif reading |

### Cool & Blue

| Theme | Font | Style |
|-------|------|-------|
| `cobalt` | System sans-serif | Deep blue developer theme |
| `ocean` | System sans-serif | Deep blue, calm and professional |
| `midnight` | System sans-serif | Rich dark-first, electric blue, subtle lift on hover |

### Soft & Colorful

| Theme | Font | Style |
|-------|------|-------|
| `rose` | System sans-serif | Soft pinks, elegant, magenta accent |
| `sakura` | System sans-serif | Cherry blossom pinks, delicate and refined |
| `lavender` | System sans-serif | Soft purple tones, warm and inviting |

### Minimal & Raw

| Theme | Font | Style |
|-------|------|-------|
| `hacker` | Monospace | Green accent, sharp corners, outlined tags, bordered code |
| `terminal` | Monospace | Teal/sky accent, sharp corners, bordered tags, dotted links, left-accent code |

---

## Choosing the Right Theme

**Writing long-form essays?** Try `serif`, `warm`, or `kanagawa` — serif fonts and generous spacing make paragraphs easy to read.

**Technical blog with lots of code?** Try `terminal`, `hacker`, `mono`, or `tokyonight` — monospace fonts and wider layouts give code blocks room to breathe.

**Want a recognizable editor look?** Try `dracula`, `gruvbox`, `catppuccin`, or `solarized` — your readers will feel at home.

**Want something bold and opinionated?** Try `brutalist` or `typewriter` — these break from blog conventions on purpose.

**Playing it safe?** `default` or `nord` work for everyone.

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

This is why `hacker` and `terminal` look different despite both being monospace themes:

| Property | hacker | terminal |
|----------|--------|----------|
| Corners | Sharp | Sharp |
| Tags | Outlined (muted border) | Bordered (accent border) |
| Links | Solid underline | Dotted underline |
| Code blocks | Full border | Left accent stripe |
| Blockquotes | Muted border | Muted border |
| Accent | Green | Teal / sky blue |

Same shape language (sharp corners), completely different component treatments.

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
theme = catppuccin
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
