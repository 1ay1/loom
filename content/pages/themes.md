---
title: Themes
slug: themes
---

## Built-in Themes

Set `theme = name` in `site.conf`. All 21 themes support dark and light mode.

### Classic

| Theme | Font | Style |
|-------|------|-------|
| `default` | System sans-serif | Clean, neutral grays, blue accent |
| `serif` | Georgia / Garamond | Editorial, warm tones, literary feel |
| `mono` | Monospace | Terminal aesthetic, green accent |
| `typewriter` | Courier New | Old-school ink-on-paper, raw and minimal |
| `brutalist` | System sans-serif | Anti-design, bold borders, red accent, uppercase |

### Editor Palettes

| Theme | Font | Style |
|-------|------|-------|
| `nord` | Inter / sans-serif | Arctic palette, muted blues |
| `solarized` | System sans-serif | Ethan Schoonover's precision-engineered palette |
| `dracula` | System sans-serif | The beloved dark-first theme, purple accent |
| `gruvbox` | System sans-serif | Retro groove, warm earthy contrast, orange accent |
| `catppuccin` | System sans-serif | Soothing pastels, purple accent |
| `tokyonight` | System sans-serif | Neon-tinged Tokyo evening palette |
| `kanagawa` | Charter / Georgia | Inspired by Hokusai's The Great Wave |

### Warm & Earthy

| Theme | Font | Style |
|-------|------|-------|
| `earth` | Charter / Georgia | Warm organic tones, olive accent |
| `warm` | Georgia / Charter | Golden amber palette, cozy reading feel |

### Cool & Blue

| Theme | Font | Style |
|-------|------|-------|
| `cobalt` | System sans-serif | Deep blue developer theme |
| `ocean` | System sans-serif | Deep blue, calm and professional |
| `midnight` | System sans-serif | Rich dark-first, electric blue, polished |

### Soft & Colorful

| Theme | Font | Style |
|-------|------|-------|
| `rose` | System sans-serif | Soft pinks, elegant, magenta accent |
| `sakura` | System sans-serif | Cherry blossom pinks, delicate and refined |
| `lavender` | System sans-serif | Soft purple tones, warm and inviting |

### Minimal & Raw

| Theme | Font | Style |
|-------|------|-------|
| `hacker` | Monospace | Green on black, no border-radius, no frills |

## Overriding Variables

Tweak a built-in theme without replacing it entirely:

```
theme = nord
theme_accent = #e06c75
theme_dark-accent = #e06c75
theme_font_size = 18px
theme_max_width = 800px
```

Underscores in key names become hyphens in CSS. Prefix with `dark-` for dark-mode-only overrides.

### Available Variables

| Variable | Description |
|----------|-------------|
| `bg` | Background color |
| `text` | Primary text color |
| `muted` | Secondary/muted text |
| `border` | Border and separator color |
| `accent` | Links, highlights, active elements |
| `font` | Font family stack |
| `font-size` | Base font size |
| `max-width` | Content area max width |
| `heading-font` | Heading font (defaults to `inherit`) |
| `code-font` | Code/monospace font |
| `line-height` | Body line height |
| `heading-weight` | Heading font weight |
| `border-radius` | Default border radius |
| `sidebar-width` | Sidebar width |
| `sidebar-gap` | Gap between content and sidebar |
| `nav-gap` | Gap between nav items |
| `header-size` | Site title font size |
| `tag-radius` | Tag badge border radius |
| `tag-bg` | Tag background |
| `tag-text` | Tag text color |
| `link-decoration` | Link text decoration |
| `card-bg` | Card background (cards layout) |
| `card-border` | Card border color |
| `card-radius` | Card border radius |
| `card-padding` | Card padding |
| `accent-hover` | Accent color on hover |
| `header-border-width` | Header bottom border (set `0` to remove) |
| `footer-border-width` | Footer top border |
| `content-width` | Content column max width |

All variables have `dark-` counterparts (e.g., `dark-bg`, `dark-accent`).

## Full CSS Override

Drop a `style.css` in `content/theme/`:

```
content/
  theme/
    style.css    # Replaces the entire default stylesheet
```

This completely replaces the built-in CSS. You're responsible for all layout, typography, and responsive design.

## Custom CSS Additions

For small tweaks without replacing everything:

```
custom_css = .post-full h1 { font-size: 2.2em; } .sidebar { display: none; }
```

This CSS is injected after the theme, so it overrides theme styles.

## Custom Head HTML

Inject anything into `<head>` — Google Fonts, analytics, meta tags:

```
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
```
